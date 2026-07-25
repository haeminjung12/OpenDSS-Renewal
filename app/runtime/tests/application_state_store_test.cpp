#include "../v2/state/application_state_store.h"

#include <QCoreApplication>

#include <atomic>
#include <iostream>
#include <thread>

namespace {

using namespace desktop_app::v2;

int fail(int code, const char *message)
{
    std::cerr << message << '\n';
    return code;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    ApplicationStateStore store;
    std::atomic_int changedCount = 0;
    QObject::connect(
        &store,
        &ApplicationStateStore::changed,
        &store,
        [&changedCount] { ++changedCount; },
        Qt::DirectConnection);

    store.publishCamera({CameraStatus::Streaming, QStringLiteral("camera-1"), {}});
    store.publishDaq({DaqStatus::Ready, QStringLiteral("daq-1"), {}});
    store.publishActiveModel(
        {QStringLiteral("model-1"), QStringLiteral("Faster model"), true, {}});
    store.publishDataset(
        {QStringLiteral("dataset-1"), QStringLiteral("C:/data/dataset.json"), true, {}});
    store.publishSequence(
        {QStringLiteral("sequence-1"), QStringLiteral("C:/data/sequence.json"), true, {}});
    store.publishTraining(
        {QStringLiteral("training-1"), TrainingStatus::Running, {}});
    store.publishRun(
        {QStringLiteral("run-1"), QStringLiteral("C:/data/run"), RunStatus::Open, {}});
    store.publishResults(
        {QStringLiteral("selected-run"), QStringLiteral("loaded-run"), {}});
    store.publishPreferences({QStringLiteral("C:/OpenDSS"), 125});

    const ApplicationSnapshot first = store.snapshot();
    if (first.camera.status != CameraStatus::Streaming
        || first.camera.deviceId != QStringLiteral("camera-1")
        || first.daq.status != DaqStatus::Ready
        || first.daq.deviceId != QStringLiteral("daq-1")
        || !first.activeModel.ready || first.activeModel.packageId != QStringLiteral("model-1")
        || !first.dataset.ready || first.dataset.datasetId != QStringLiteral("dataset-1")
        || !first.sequence.ready || first.sequence.sequenceId != QStringLiteral("sequence-1")
        || first.training.status != TrainingStatus::Running
        || first.training.executionId != QStringLiteral("training-1")
        || first.run.status != RunStatus::Open || first.run.runId != QStringLiteral("run-1")
        || first.results.selectedRunId != QStringLiteral("selected-run")
        || first.results.loadedRunId != QStringLiteral("loaded-run")
        || first.preferences.storageRoot != QStringLiteral("C:/OpenDSS")
        || first.preferences.textSizePercent != 125 || changedCount != 9) {
        return fail(1, "Publishing every domain did not produce one aggregate value snapshot.");
    }

    store.publishResults(
        {QStringLiteral("selected-run-2"), QStringLiteral("loaded-run-2"),
         QStringLiteral("load failed")});
    if (first.results.selectedRunId != QStringLiteral("selected-run")
        || store.snapshot().run.runId != QStringLiteral("run-1")
        || store.snapshot().run.status != RunStatus::Open) {
        return fail(2, "Results publication altered the independent Run state.");
    }

    store.publishDataset(
        {QStringLiteral("dataset-2"), QStringLiteral("D:/data/dataset.json"), true, {}});
    if (first.dataset.datasetId != QStringLiteral("dataset-1")
        || first.results.selectedRunId != QStringLiteral("selected-run")
        || store.snapshot().dataset.datasetId != QStringLiteral("dataset-2")) {
        return fail(3, "A returned snapshot was not an independent value copy.");
    }

    std::thread publisher([&store] {
        store.publishPreferences({QStringLiteral("D:/OpenDSS"), 150});
    });
    publisher.join();
    const ApplicationSnapshot threaded = store.snapshot();
    if (threaded.preferences.storageRoot != QStringLiteral("D:/OpenDSS")
        || threaded.preferences.textSizePercent != 150
        || threaded.run.status != RunStatus::Open
        || threaded.results.loadedRunId != QStringLiteral("loaded-run-2")
        || changedCount != 12) {
        return fail(4, "Threaded publication did not safely update the value snapshot.");
    }

    return 0;
}
