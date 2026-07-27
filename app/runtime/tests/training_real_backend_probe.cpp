#include "v2/dataset/dataset_manifest_v2.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/training/training_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <iostream>

namespace {

using desktop_app::v2::OperationCoordinator;
using desktop_app::v2::dataset::DatasetClass;
using desktop_app::v2::dataset::DatasetManifestData;
using desktop_app::v2::dataset::DatasetManifestV2;
using desktop_app::v2::dataset::DatasetRecord;
using desktop_app::v2::dataset::UserLabelRecord;
using desktop_app::v2::training::TrainingProfile;
using desktop_app::v2::training::TrainingRequest;
using desktop_app::v2::training::TrainingService;
using desktop_app::v2::training::TrainingState;

int fail(int code, const QString &message)
{
    std::cerr << message.toStdString() << '\n';
    return code;
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

QString sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

QString sha256Set(QStringList paths)
{
    std::sort(paths.begin(), paths.end());
    QCryptographicHash combined(QCryptographicHash::Sha256);
    for (const QString &path : paths) {
        const QString hash = sha256File(path);
        if (hash.isEmpty())
            return {};
        combined.addData(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()).toUtf8());
        combined.addData("\n");
        combined.addData(hash.toUtf8());
        combined.addData("\n");
    }
    return QString::fromLatin1(combined.result().toHex());
}

bool waitForTerminalState(TrainingService &service, int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (service.state() == TrainingState::Running && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return service.state() != TrainingState::Running;
}

QString productionPython()
{
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA").trimmed();
    if (localAppData.isEmpty())
        return {};
    const QString candidate =
        QDir(localAppData).filePath(QStringLiteral("OpenDSS/training-venv-gpu/Scripts/python.exe"));
    return QFileInfo(candidate).isFile() ? QFileInfo(candidate).absoluteFilePath() : QString{};
}

QJsonObject runEnvironmentCheck(const QString &python, const QString &output, QString *error)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("PYTHONHOME"));
    environment.remove(QStringLiteral("PYTHONPATH"));
    environment.insert(QStringLiteral("PYTHONNOUSERSITE"), QStringLiteral("1"));
    environment.insert(QStringLiteral("PIP_NO_INDEX"), QStringLiteral("1"));
    environment.insert(QStringLiteral("PIP_INDEX_URL"), QStringLiteral("https://127.0.0.1:1/no-network"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QFileInfo(output).absolutePath());
    process.setProgram(python);
    process.setArguments({
        QStringLiteral("-I"),
        QStringLiteral("-m"),
        QStringLiteral("droplet_trainer"),
        QStringLiteral("env-check"),
        QStringLiteral("--device"),
        QStringLiteral("auto"),
        QStringLiteral("--check-output"),
        output,
        QStringLiteral("--require-training"),
        QStringLiteral("--require-onnx"),
        QStringLiteral("--json"),
    });
    process.start();
    if (!process.waitForStarted(10'000) || !process.waitForFinished(120'000)) {
        process.kill();
        process.waitForFinished(3'000);
        *error = QStringLiteral("The isolated production env-check did not finish: %1")
                     .arg(process.errorString());
        return {};
    }
    const QByteArray standardOutput = process.readAllStandardOutput().trimmed();
    const QByteArray standardError = process.readAllStandardError().trimmed();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(standardOutput, &parseError);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
        || parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("Production env-check failed (%1): %2")
                     .arg(process.exitCode())
                     .arg(QString::fromUtf8(standardError));
        return {};
    }
    return document.object();
}

bool createDatasetFixture(const QString &root, QString *manifestPath, QStringList *sourceFiles,
                          QString *error)
{
    const QString crops = QDir(root).filePath(QStringLiteral("crops"));
    if (!QDir().mkpath(crops)) {
        *error = QStringLiteral("Could not create fixture crop directory.");
        return false;
    }

    const QByteArray png = QByteArray::fromBase64(
        QByteArrayLiteral("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="));
    DatasetManifestData data;
    data.datasetId = QStringLiteral("uat-train-real-001");
    data.provenance.name = QStringLiteral("UAT-TRAIN-REAL-001 deterministic fixture");
    data.provenance.opendssVersion = QStringLiteral("v2-probe");
    data.provenance.createdAt = QStringLiteral("2026-07-27T00:00:00Z");
    data.provenance.updatedAt = data.provenance.createdAt;
    data.provenance.captureStartedAt = data.provenance.createdAt;
    data.provenance.captureEndedAt = data.provenance.createdAt;
    data.provenance.stopReason = QStringLiteral("fixture-complete");
    data.provenance.status = QStringLiteral("completed");
    data.provenance.sequence.frameCount = 9;
    data.provenance.sequence.imageWidth = 1;
    data.provenance.sequence.imageHeight = 1;
    data.provenance.sequence.bitDepth = 8;
    data.provenance.sequence.nominalFps = 1.0;
    data.classes = {
        DatasetClass{QStringLiteral("0"), QStringLiteral("Waste")},
        DatasetClass{QStringLiteral("1"), QStringLiteral("Hit")},
        DatasetClass{QStringLiteral("2"), QStringLiteral("Multiple")},
    };

    qint64 frameIndex = 0;
    for (const DatasetClass &datasetClass : data.classes) {
        for (int sample = 0; sample < 3; ++sample) {
            ++frameIndex;
            const QString recordId =
                QStringLiteral("class-%1-sample-%2").arg(datasetClass.id).arg(sample + 1);
            const QString relativePath = QStringLiteral("crops/%1.png").arg(recordId);
            const QString absolutePath = QDir(root).filePath(relativePath);
            if (!writeBytes(absolutePath, png)) {
                *error = QStringLiteral("Could not write fixture crop %1.").arg(absolutePath);
                return false;
            }
            const QString hash = sha256File(absolutePath);
            if (hash.isEmpty()) {
                *error = QStringLiteral("Could not hash fixture crop %1.").arg(absolutePath);
                return false;
            }
            sourceFiles->append(absolutePath);
            data.records.append(DatasetRecord{
                recordId,
                relativePath,
                hash,
                QStringLiteral("frame-%1").arg(frameIndex),
                QStringLiteral("event-%1").arg(frameIndex),
                data.provenance.createdAt,
                QRect(0, 0, 1, 1),
                frameIndex,
            });
            data.labels.append(UserLabelRecord{
                QStringLiteral("label-%1").arg(recordId),
                recordId,
                datasetClass.id,
                false,
            });
        }
    }

    *manifestPath = QDir(root).filePath(QStringLiteral("dataset.json"));
    if (!DatasetManifestV2::save(*manifestPath, data, error))
        return false;
    sourceFiles->append(*manifestPath);
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("training_real_backend_probe"));

    const QString python = productionPython();
    if (python.isEmpty())
        return fail(1, QStringLiteral("Installer-owned OpenDSS GPU Python was not found."));

    QTemporaryDir temporary(QDir::tempPath() + QStringLiteral("/opendss-train-real-XXXXXX"));
    if (!temporary.isValid())
        return fail(2, QStringLiteral("Could not create disposable TEMP fixture."));
    const QString fixtureRoot = QDir(temporary.path()).filePath(QStringLiteral("fixture"));
    const QString outputRoot = QDir(temporary.path()).filePath(QStringLiteral("output"));
    if (!QDir().mkpath(fixtureRoot) || !QDir().mkpath(outputRoot))
        return fail(3, QStringLiteral("Could not create fixture/output roots."));

    QString manifestPath;
    QStringList fixtureFiles;
    QString fixtureError;
    if (!createDatasetFixture(fixtureRoot, &manifestPath, &fixtureFiles, &fixtureError))
        return fail(4, fixtureError);

    const QString modelsRoot =
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .filePath(QStringLiteral("OpenDropletSortingSuite/models"));
    const QString registryPath = QDir(modelsRoot).filePath(QStringLiteral("model_registry.json"));
    const QString weightPath =
        QDir(modelsRoot).filePath(
            QStringLiteral("weights/imagenet/mobilenet_v3_small-047dcff4.pth"));
    if (!QFileInfo(registryPath).isFile() || !QFileInfo(weightPath).isFile())
        return fail(5, QStringLiteral("Installed model registry or allowlisted ImageNet weight is absent."));

    const QString fixtureHashBefore = sha256Set(fixtureFiles);
    const QString registryHashBefore = sha256File(registryPath);
    const QString weightHashBefore = sha256File(weightPath);
    if (fixtureHashBefore.isEmpty() || registryHashBefore.isEmpty() || weightHashBefore.isEmpty())
        return fail(6, QStringLiteral("Could not establish immutable input hashes."));

    qputenv("PYTHONHOME", QByteArrayLiteral("C:\\poisoned-python-home"));
    qputenv("PYTHONPATH", QByteArrayLiteral("C:\\poisoned-python-path"));
    qputenv("PYTHONNOUSERSITE", QByteArrayLiteral("0"));
    qputenv("PIP_NO_INDEX", QByteArrayLiteral("1"));
    qputenv("PIP_INDEX_URL", QByteArrayLiteral("https://127.0.0.1:1/no-network"));

    QString environmentError;
    const QJsonObject environment = runEnvironmentCheck(python, outputRoot, &environmentError);
    if (environment.isEmpty())
        return fail(7, environmentError);
    const QJsonObject devices = environment.value(QStringLiteral("devices")).toObject();
    if (environment.value(QStringLiteral("status")).toString() != QStringLiteral("ok")
        || devices.value(QStringLiteral("selected")).toString() != QStringLiteral("cuda")
        || !devices.value(QStringLiteral("cuda_available")).toBool()
        || !devices.value(QStringLiteral("onnxruntime_providers")).toArray().contains(
            QStringLiteral("CUDAExecutionProvider"))) {
        return fail(8, QStringLiteral("Automatic production environment selection did not resolve CUDA."));
    }

    OperationCoordinator operations;
    TrainingService service(operations);
    const TrainingRequest request{
        manifestPath,
        TrainingProfile::Faster,
        QStringLiteral("UAT-TRAIN-REAL-001"),
        outputRoot,
        python,
        QStringLiteral("cuda"),
        QCoreApplication::applicationDirPath(),
        QStringLiteral("imagenet"),
        weightPath,
    };

    QString startError;
    if (!service.start(request, &startError))
        return fail(9, QStringLiteral("Real TrainingService start failed: %1").arg(startError));

    QElapsedTimer progressTimer;
    progressTimer.start();
    while (service.state() == TrainingState::Running && service.progress().stage.isEmpty()
           && progressTimer.elapsed() < 180'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    const bool genuineProgress = !service.progress().stage.isEmpty();
    if (service.state() == TrainingState::Running)
        service.cancel();
    if (!waitForTerminalState(service, 15'000))
        return fail(10, QStringLiteral("Real training did not stop within the bounded interval."));
    if (!genuineProgress)
        return fail(11, QStringLiteral("Real trainer did not report genuine stage progress: %1 %2")
                            .arg(service.lastError(), service.standardError()));
    if (service.state() != TrainingState::Interrupted
        && service.state() != TrainingState::Completed) {
        return fail(12, QStringLiteral("Real trainer ended in an unexpected state: %1 %2")
                            .arg(service.lastError(), service.standardError()));
    }

    if (sha256Set(fixtureFiles) != fixtureHashBefore
        || sha256File(registryPath) != registryHashBefore
        || sha256File(weightPath) != weightHashBefore) {
        return fail(13, QStringLiteral("Training modified an immutable fixture/model source."));
    }

    std::cout << "UAT-TRAIN-REAL-001 PASS\n"
              << "python=" << python.toStdString() << '\n'
              << "device=cuda\n"
              << "terminal="
              << (service.state() == TrainingState::Interrupted ? "interrupted" : "completed")
              << '\n';
    return 0;
}
