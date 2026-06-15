#include "dataset_capture_session.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {
std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

std::string sha256Bytes(const std::vector<unsigned char>& data) {
    static constexpr std::array<std::uint32_t, 64> k = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
    std::vector<unsigned char> msg = data;
    const std::uint64_t bitLen = static_cast<std::uint64_t>(msg.size()) * 8u;
    msg.push_back(0x80u);
    while ((msg.size() % 64u) != 56u) {
        msg.push_back(0u);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<unsigned char>((bitLen >> (i * 8)) & 0xffu));
    }

    std::array<std::uint32_t, 8> h = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

    for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t j = chunk + i * 4;
            w[i] = (static_cast<std::uint32_t>(msg[j]) << 24) | (static_cast<std::uint32_t>(msg[j + 1]) << 16) |
                   (static_cast<std::uint32_t>(msg[j + 2]) << 8) | static_cast<std::uint32_t>(msg[j + 3]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = hh + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::uint32_t value : h) {
        out << std::setw(8) << value;
    }
    return out.str();
}
} // namespace

bool DatasetCaptureSession::parseCollectionMode(const std::string& text, DatasetCollectionMode& mode) {
    if (text == "hit-only") {
        mode = DatasetCollectionMode::HitOnly;
        return true;
    }
    if (text == "waste-only") {
        mode = DatasetCollectionMode::WasteOnly;
        return true;
    }
    if (text == "mixed") {
        mode = DatasetCollectionMode::Mixed;
        return true;
    }
    return false;
}

std::string DatasetCaptureSession::collectionModeToString(DatasetCollectionMode mode) {
    switch (mode) {
    case DatasetCollectionMode::HitOnly:
        return "hit-only";
    case DatasetCollectionMode::WasteOnly:
        return "waste-only";
    case DatasetCollectionMode::Mixed:
        return "mixed";
    }
    return "mixed";
}

bool DatasetCaptureSession::start(const DatasetCaptureConfig& config, std::string& err) {
    if (config.sessionDir.empty()) {
        err = "dataset capture session directory is empty";
        return false;
    }
    if (config.batchTarget == 0) {
        err = "dataset capture batch target must be greater than zero";
        return false;
    }

    config_ = config;
    currentBatchTarget_ = config.batchTarget;
    cropsDir_ = config_.sessionDir / "pending" / "crops";
    sourceFramesDir_ = config_.sessionDir / "pending" / "source_frames";
    overlaysDir_ = config_.sessionDir / "pending" / "overlays";
    metadataDir_ = config_.sessionDir / "metadata";
    cropsCsvPath_ = metadataDir_ / "crops.csv";
    std::error_code ec;
    fs::create_directories(cropsDir_, ec);
    if (ec) {
        err = "failed to create pending/crops directory: " + ec.message();
        return false;
    }
    fs::create_directories(sourceFramesDir_, ec);
    if (ec) {
        err = "failed to create pending/source_frames directory: " + ec.message();
        return false;
    }
    fs::create_directories(overlaysDir_, ec);
    if (ec) {
        err = "failed to create pending/overlays directory: " + ec.message();
        return false;
    }
    fs::create_directories(metadataDir_, ec);
    if (ec) {
        err = "failed to create metadata directory: " + ec.message();
        return false;
    }
    for (const char* dirName :
         {"reviewed/hit", "reviewed/waste", "reviewed/exclude", "trainer_ready/hit", "trainer_ready/waste"}) {
        fs::create_directories(config_.sessionDir / dirName, ec);
        if (ec) {
            err = std::string("failed to create Dataset Builder directory ") + dirName + ": " + ec.message();
            return false;
        }
    }
    if (fs::exists(cropsCsvPath_, ec)) {
        err = "refusing to overwrite existing Dataset Builder crops manifest: " + cropsCsvPath_.string();
        return false;
    }

    cropsCsv_.open(cropsCsvPath_.string(), std::ios::out | std::ios::trunc);
    if (!cropsCsv_.is_open()) {
        err = "failed to open Dataset Builder crops manifest: " + cropsCsvPath_.string();
        return false;
    }
    cropsCsv_ << "crop_id,path,source_type,source_sequence_id,source_frame_filename,source_frame_index,"
              << "event_id,classification_frame,crop_x,crop_y,crop_w,crop_h,bbox_x,bbox_y,bbox_w,bbox_h,"
              << "auto_label,auto_label_mode,predicted_class_id,predicted_label,confidence,review_label,"
              << "review_status,exclude_reason,source_crop_path,model_path,metadata_path,created_at,hash_sha256\n";
    cropsCsv_.flush();
    batchStopReason_ = "not_stopped";
    createdAt_ = nowIso8601();
    batchContinueCount_ = 0;
    collectedCount_ = 0;
    batchPromptAudit_.clear();
    items_.clear();
    started_ = true;
    return writeSessionArtifacts(err);
}

bool DatasetCaptureSession::addCrop(const DatasetCropCandidate& candidate, std::string& err) {
    if (!started_) {
        err = "Dataset Builder capture session has not been started";
        return false;
    }
    if (targetReached()) {
        err = "Dataset Builder capture batch target has already been reached";
        return false;
    }
    std::error_code ec;
    if (!fs::exists(candidate.sourceCropPath, ec) || !fs::is_regular_file(candidate.sourceCropPath, ec)) {
        err = "source crop does not exist: " + candidate.sourceCropPath.string();
        return false;
    }

    const std::size_t next = collectedCount_ + 1;
    const std::size_t batchIndex = ((next - 1) / config_.batchTarget) + 1;
    const std::string modeText = collectionModeToString(config_.collectionMode);
    const std::string autoLabel = autoLabelFor(candidate, config_.collectionMode);
    const std::string autoLabelFile = autoLabel.empty() ? "unknown" : autoLabel;
    std::ostringstream cropId;
    std::string shortId = config_.sessionId;
    const std::size_t underscore = shortId.rfind('_');
    if (underscore != std::string::npos && underscore + 1 < shortId.size()) {
        shortId = shortId.substr(underscore + 1);
    }
    if (shortId.empty())
        shortId = "session";
    std::ostringstream sourceFrameId;
    if (candidate.sourceFrameIndex >= 0) {
        sourceFrameId << "f" << std::setw(6) << std::setfill('0') << candidate.sourceFrameIndex;
    } else if (!candidate.sourceFrameFilename.empty()) {
        sourceFrameId << fs::path(candidate.sourceFrameFilename).stem().string();
    } else {
        sourceFrameId << "funknown";
    }
    cropId << "crop_" << shortId << "_" << std::setw(6) << std::setfill('0') << next;
    std::ostringstream cropFile;
    cropFile << "crop_" << shortId << "_b" << std::setw(3) << std::setfill('0') << batchIndex << "_c" << std::setw(6)
             << std::setfill('0') << next << "_" << sourceFrameId.str() << "_" << modeText << "_auto-" << autoLabelFile
             << ".png";
    fs::path destPath = cropsDir_ / cropFile.str();
    fs::copy_file(candidate.sourceCropPath, destPath, fs::copy_options::none, ec);
    if (ec) {
        err = "failed to copy crop into Dataset Builder session: " + ec.message();
        return false;
    }

    const std::string createdAt = nowIso8601();
    const std::string hash = sha256File(destPath);
    const std::string relPath = (fs::path("pending") / "crops" / destPath.filename()).generic_string();
    const double confidence = std::clamp(candidate.confidence, 0.0, 1.0);
    cropsCsv_ << cropId.str() << "," << csvQuote(relPath) << "," << csvQuote(candidate.sourceType) << ","
              << csvQuote(candidate.sourceSequenceId) << "," << csvQuote(candidate.sourceFrameFilename) << ","
              << candidate.sourceFrameIndex << "," << candidate.eventId << "," << candidate.classificationFrame << ","
              << candidate.cropX << "," << candidate.cropY << "," << candidate.cropW << "," << candidate.cropH << ","
              << candidate.bboxX << "," << candidate.bboxY << "," << candidate.bboxW << "," << candidate.bboxH << ","
              << csvQuote(autoLabelFile) << "," << csvQuote(modeText) << "," << csvQuote(candidate.predictedClassId)
              << "," << csvQuote(candidate.predictedLabel) << "," << std::fixed << std::setprecision(6) << confidence
              << ","
              << "," << csvQuote("unreviewed") << "," << "," << csvQuote(candidate.sourceCropPath.string()) << ","
              << csvQuote(config_.modelPath) << "," << csvQuote(config_.metadataPath) << "," << csvQuote(createdAt)
              << "," << csvQuote(hash) << "\n";
    cropsCsv_.flush();
    CapturedItem item;
    item.imageId = cropId.str();
    item.cropPath = relPath;
    item.sourceFramePath = candidate.sourceFramePath.empty() ? std::string() : candidate.sourceFramePath.string();
    item.overlayPath = candidate.overlayPath.empty() ? std::string() : candidate.overlayPath.string();
    item.sourceFrameId = sourceFrameId.str();
    item.timestamp = createdAt;
    item.cropX = candidate.cropX;
    item.cropY = candidate.cropY;
    item.cropW = candidate.cropW;
    item.cropH = candidate.cropH;
    item.collectionMode = modeText;
    item.batchIndex = batchIndex;
    item.batchTarget = config_.batchTarget;
    item.autoLabel = autoLabelFile;
    item.autoLabelSource = autoLabelSourceFor(config_.collectionMode);
    item.autoLabelConfidence = confidence;
    item.autoLabelModelId = (item.autoLabelSource == "model_prediction") ? config_.modelId : std::string();
    item.hashSha256 = hash;
    items_.push_back(item);
    collectedCount_ = next;
    if (targetReached()) {
        batchStopReason_ = "target_reached";
    }
    return writeSessionArtifacts(err);
}

void DatasetCaptureSession::extendBatchTarget() {
    recordBatchPrompt("continue_collecting");
    batchContinueCount_++;
    currentBatchTarget_ += config_.batchTarget;
    batchStopReason_ = "not_stopped";
}

void DatasetCaptureSession::recordBatchPrompt(const std::string& decision) {
    std::ostringstream audit;
    audit << "{\"batch_index\":" << (((collectedCount_ == 0 ? 1 : collectedCount_) - 1) / config_.batchTarget + 1)
          << ",\"collected_count\":" << collectedCount_ << ",\"prompted_at\":" << jsonQuote(nowIso8601())
          << ",\"decision\":" << jsonQuote(decision) << "}";
    batchPromptAudit_.push_back(audit.str());
}

void DatasetCaptureSession::setStopReason(const std::string& reason) {
    batchStopReason_ = reason;
}

bool DatasetCaptureSession::finalize(std::string& err) {
    if (!started_) {
        return true;
    }
    cropsCsv_.flush();
    cropsCsv_.close();
    if (batchStopReason_ == "not_stopped") {
        batchStopReason_ = "source_complete";
    }
    return writeSessionArtifacts(err);
}

std::string DatasetCaptureSession::csvQuote(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') {
            out.push_back('"');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string DatasetCaptureSession::jsonQuote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (c < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
            } else {
                out << static_cast<char>(c);
            }
            break;
        }
    }
    out << '"';
    return out.str();
}

std::string DatasetCaptureSession::nowIso8601() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string DatasetCaptureSession::autoLabelFor(const DatasetCropCandidate& candidate, DatasetCollectionMode mode) {
    if (mode == DatasetCollectionMode::HitOnly) {
        return "hit";
    }
    if (mode == DatasetCollectionMode::WasteOnly) {
        return "waste";
    }
    if (candidate.predictedClassId == "1") {
        return "hit";
    }
    if (candidate.predictedClassId == "0") {
        return "waste";
    }
    return "unknown";
}

std::string DatasetCaptureSession::autoLabelSourceFor(DatasetCollectionMode mode) {
    if (mode == DatasetCollectionMode::Mixed) {
        return "model_prediction";
    }
    return "mode_default";
}

std::string DatasetCaptureSession::sha256File(const fs::path& path) {
    std::ifstream in(path.string(), std::ios::binary);
    if (!in.is_open()) {
        return "";
    }
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return sha256Bytes(bytes);
}

std::string DatasetCaptureSession::normalizedStopReason(const std::string& reason) {
    if (reason == "sequence_complete")
        return "source_complete";
    if (reason == "live_stopped")
        return "cancelled";
    if (reason == "target_reached" || reason == "user_stop_after_batch_prompt" || reason == "source_complete" ||
        reason == "cancelled" || reason == "error" || reason == "not_stopped") {
        return reason;
    }
    return "cancelled";
}

bool DatasetCaptureSession::writeLabelsCsv(std::string& err) const {
    fs::path path = metadataDir_ / "labels.csv";
    fs::path tempPath = metadataDir_ / "labels.csv.tmp";
    std::ofstream out(tempPath.string(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        err = "failed to write labels.csv";
        return false;
    }
    out << "image_id,crop_path,source_frame_path,source_frame_id,timestamp,crop_x,crop_y,crop_w,crop_h,collection_mode,"
           "batch_index,auto_label,auto_label_source,auto_label_confidence,auto_label_model_id,review_state,reviewed_"
           "label,exclude_reason,trainer_eligible,hash_sha256\n";
    for (const auto& item : items_) {
        out << csvQuote(item.imageId) << "," << csvQuote(item.cropPath) << "," << csvQuote(item.sourceFramePath) << ","
            << csvQuote(item.sourceFrameId) << "," << csvQuote(item.timestamp) << "," << item.cropX << "," << item.cropY
            << "," << item.cropW << "," << item.cropH << "," << csvQuote(item.collectionMode) << "," << item.batchIndex
            << "," << csvQuote(item.autoLabel) << "," << csvQuote(item.autoLabelSource) << "," << std::fixed
            << std::setprecision(6) << item.autoLabelConfidence << "," << csvQuote(item.autoLabelModelId) << ","
            << csvQuote("unreviewed") << ",,,false," << csvQuote(item.hashSha256) << "\n";
    }
    out.flush();
    out.close();

    std::error_code ec;
    fs::rename(tempPath, path, ec);
    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(tempPath, path, ec);
    }
    if (ec) {
        err = "failed to finalize labels.csv: " + ec.message();
        return false;
    }
    return true;
}

bool DatasetCaptureSession::writeClassBalanceCsv(std::string& err) const {
    fs::path path = metadataDir_ / "class_balance.csv";
    fs::path tempPath = metadataDir_ / "class_balance.csv.tmp";
    std::ofstream out(tempPath.string(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        err = "failed to write class_balance.csv";
        return false;
    }
    out << "label,reviewed_count,trainer_eligible_count,excluded_count,unreviewed_count\n";
    out << "hit,0,0,0,0\n";
    out << "waste,0,0,0,0\n";
    out << "exclude,0,0,0,0\n";
    out << "total,0,0,0," << items_.size() << "\n";
    out.flush();
    out.close();

    std::error_code ec;
    fs::rename(tempPath, path, ec);
    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(tempPath, path, ec);
    }
    if (ec) {
        err = "failed to finalize class_balance.csv: " + ec.message();
        return false;
    }
    return true;
}

bool DatasetCaptureSession::writeSessionArtifacts(std::string& err) const {
    if (!writeLabelsCsv(err))
        return false;
    if (!writeClassBalanceCsv(err))
        return false;

    fs::path sessionPath = metadataDir_ / "collection_session.json";
    fs::path sessionTempPath = metadataDir_ / "collection_session.json.tmp";
    std::ofstream session(sessionTempPath.string(), std::ios::out | std::ios::trunc);
    if (!session.is_open()) {
        err = "failed to write collection_session.json";
        return false;
    }
    session << "{\n";
    session << "  \"schema_version\": \"dataset-builder-collection-session-v1\",\n";
    session << "  \"session_id\": " << jsonQuote(config_.sessionId) << ",\n";
    session << "  \"source_type\": " << jsonQuote(config_.sourceType) << ",\n";
    session << "  \"source_path\": " << jsonQuote(config_.sourcePath) << ",\n";
    session << "  \"collection_mode\": " << jsonQuote(collectionModeToString(config_.collectionMode)) << ",\n";
    session << "  \"batch_target\": " << config_.batchTarget << ",\n";
    session << "  \"current_batch_target\": " << currentBatchTarget_ << ",\n";
    session << "  \"collected_count\": " << collectedCount_ << ",\n";
    session << "  \"batch_continue_count\": " << batchContinueCount_ << ",\n";
    session << "  \"batch_stop_reason\": " << jsonQuote(normalizedStopReason(batchStopReason_)) << ",\n";
    session << "  \"batch_prompts\": [";
    for (std::size_t i = 0; i < batchPromptAudit_.size(); ++i) {
        if (i > 0)
            session << ", ";
        session << batchPromptAudit_[i];
    }
    session << "],\n";
    session << "  \"labels\": [\"hit\", \"waste\", \"exclude\"],\n";
    session << "  \"review_required\": true,\n";
    session << "  \"trainer_ready\": false\n";
    session << "}\n";
    session.flush();
    session.close();

    fs::path manifestPath = metadataDir_ / "dataset_manifest.json";
    fs::path manifestTempPath = metadataDir_ / "dataset_manifest.json.tmp";
    std::ofstream out(manifestTempPath.string(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        err = "failed to write dataset_manifest.json";
        return false;
    }
    out << "{\n";
    out << "  \"schema_version\": \"dataset-builder-manifest-v1\",\n";
    out << "  \"dataset_id\": " << jsonQuote(config_.sessionId) << ",\n";
    out << "  \"created_at\": " << jsonQuote(createdAt_.empty() ? nowIso8601() : createdAt_) << ",\n";
    out << "  \"updated_at\": " << jsonQuote(nowIso8601()) << ",\n";
    out << "  \"source\": {\n";
    out << "    \"type\": " << jsonQuote(config_.sourceType == "live_stream" ? "live_stream" : "recorded_sequence")
        << ",\n";
    out << "    \"path\": " << (config_.sourcePath.empty() ? "null" : jsonQuote(config_.sourcePath)) << ",\n";
    out << "    \"run_manifest_path\": "
        << (config_.runManifestPath.empty() ? "null" : jsonQuote(config_.runManifestPath)) << ",\n";
    out << "    \"capture_session_path\": \"metadata/collection_session.json\"\n";
    out << "  },\n";
    out << "  \"collection\": {\n";
    out << "    \"mode\": " << jsonQuote(collectionModeToString(config_.collectionMode)) << ",\n";
    out << "    \"batch_target\": " << config_.batchTarget << ",\n";
    out << "    \"batches_completed\": " << (collectedCount_ / config_.batchTarget) << ",\n";
    out << "    \"stopped_reason\": " << jsonQuote(normalizedStopReason(batchStopReason_)) << "\n";
    out << "  },\n";
    out << "  \"class_schema\": {\n";
    out << "    \"kind\": \"binary\",\n";
    out << "    \"classes\": [\n";
    out << "      {\"id\": \"waste\", \"index\": 0, \"display_name\": \"Waste\", \"folder\": \"reviewed/waste\"},\n";
    out << "      {\"id\": \"hit\", \"index\": 1, \"display_name\": \"Hit\", \"folder\": \"reviewed/hit\"}\n";
    out << "    ],\n";
    out << "    \"excluded_label\": {\"id\": \"exclude\", \"folder\": \"reviewed/exclude\"}\n";
    out << "  },\n";
    if (config_.collectionMode == DatasetCollectionMode::Mixed) {
        out << "  \"auto_label_model\": {\n";
        out << "    \"model_id\": " << (config_.modelId.empty() ? "null" : jsonQuote(config_.modelId)) << ",\n";
        out << "    \"model_path\": " << (config_.modelPath.empty() ? "null" : jsonQuote(config_.modelPath)) << ",\n";
        out << "    \"metadata_path\": " << (config_.metadataPath.empty() ? "null" : jsonQuote(config_.metadataPath))
            << ",\n";
        out << "    \"model_sha256\": " << (config_.modelSha256.empty() ? "null" : jsonQuote(config_.modelSha256))
            << ",\n";
        out << "    \"class_mapping\": {\"0\": \"waste\", \"1\": \"hit\"}\n";
        out << "  },\n";
    } else {
        out << "  \"auto_label_model\": null,\n";
    }
    out << "  \"items\": [\n";
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        out << "    {\n";
        out << "      \"image_id\": " << jsonQuote(item.imageId) << ",\n";
        out << "      \"crop_path\": " << jsonQuote(item.cropPath) << ",\n";
        out << "      \"source_frame_path\": "
            << (item.sourceFramePath.empty() ? "null" : jsonQuote(item.sourceFramePath)) << ",\n";
        out << "      \"overlay_path\": " << (item.overlayPath.empty() ? "null" : jsonQuote(item.overlayPath)) << ",\n";
        out << "      \"source_frame_id\": " << jsonQuote(item.sourceFrameId) << ",\n";
        out << "      \"timestamp\": " << jsonQuote(item.timestamp) << ",\n";
        out << "      \"crop_rect\": [" << item.cropX << ", " << item.cropY << ", " << item.cropW << ", " << item.cropH
            << "],\n";
        out << "      \"collection_mode\": " << jsonQuote(item.collectionMode) << ",\n";
        out << "      \"batch_index\": " << item.batchIndex << ",\n";
        out << "      \"batch_target\": " << item.batchTarget << ",\n";
        out << "      \"auto_label\": " << jsonQuote(item.autoLabel) << ",\n";
        out << "      \"auto_label_source\": " << jsonQuote(item.autoLabelSource) << ",\n";
        out << "      \"auto_label_confidence\": " << std::fixed << std::setprecision(6) << item.autoLabelConfidence
            << ",\n";
        out << "      \"auto_label_model_id\": "
            << (item.autoLabelModelId.empty() ? "null" : jsonQuote(item.autoLabelModelId)) << ",\n";
        out << "      \"review_state\": \"unreviewed\",\n";
        out << "      \"reviewed_label\": null,\n";
        out << "      \"reviewed_by\": null,\n";
        out << "      \"reviewed_at\": null,\n";
        out << "      \"exclude_reason\": null,\n";
        out << "      \"trainer_eligible\": false,\n";
        out << "      \"hash_sha256\": " << (item.hashSha256.empty() ? "null" : jsonQuote(item.hashSha256)) << ",\n";
        out << "      \"notes\": \"\"\n";
        out << "    }" << (i + 1 < items_.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    out.flush();
    out.close();

    std::error_code ec;
    fs::rename(sessionTempPath, sessionPath, ec);
    if (ec) {
        fs::remove(sessionPath, ec);
        ec.clear();
        fs::rename(sessionTempPath, sessionPath, ec);
    }
    if (ec) {
        err = "failed to finalize collection_session.json: " + ec.message();
        return false;
    }

    fs::rename(manifestTempPath, manifestPath, ec);
    if (ec) {
        fs::remove(manifestPath, ec);
        ec.clear();
        fs::rename(manifestTempPath, manifestPath, ec);
    }
    if (ec) {
        err = "failed to finalize dataset_manifest.json: " + ec.message();
        return false;
    }
    return true;
}
