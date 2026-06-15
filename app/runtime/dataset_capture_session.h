#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

enum class DatasetCollectionMode { HitOnly, WasteOnly, Mixed };

enum class DatasetBatchFullAction { Stop, Prompt, Continue };

struct DatasetCaptureConfig {
    std::filesystem::path sessionDir;
    std::string sessionId;
    std::string sourceType = "recorded_sequence";
    std::string sourcePath;
    std::string runManifestPath;
    DatasetCollectionMode collectionMode = DatasetCollectionMode::Mixed;
    std::size_t batchTarget = 100;
    std::string modelPath;
    std::string metadataPath;
    std::string modelId;
    std::string modelSha256;
    std::string metadataSha256;
};

struct DatasetCropCandidate {
    std::string sourceType = "recorded_sequence";
    std::string sourceSequenceId;
    std::string sourceFrameFilename;
    int sourceFrameIndex = -1;
    int eventId = 0;
    int classificationFrame = -1;
    int cropX = 0;
    int cropY = 0;
    int cropW = 0;
    int cropH = 0;
    int bboxX = 0;
    int bboxY = 0;
    int bboxW = 0;
    int bboxH = 0;
    std::string predictedClassId;
    std::string predictedLabel;
    double confidence = 0.0;
    std::filesystem::path sourceCropPath;
    std::filesystem::path sourceFramePath;
    std::filesystem::path overlayPath;
};

class DatasetCaptureSession {
  public:
    bool start(const DatasetCaptureConfig& config, std::string& err);
    bool addCrop(const DatasetCropCandidate& candidate, std::string& err);
    void extendBatchTarget();
    void recordBatchPrompt(const std::string& decision);
    void setStopReason(const std::string& reason);
    bool finalize(std::string& err);

    bool isStarted() const {
        return started_;
    }
    bool targetReached() const {
        return started_ && collectedCount_ >= currentBatchTarget_;
    }
    std::size_t collectedCount() const {
        return collectedCount_;
    }
    std::size_t currentBatchTarget() const {
        return currentBatchTarget_;
    }
    const std::filesystem::path& sessionDir() const {
        return config_.sessionDir;
    }
    const std::string& stopReason() const {
        return batchStopReason_;
    }

    static bool parseCollectionMode(const std::string& text, DatasetCollectionMode& mode);
    static std::string collectionModeToString(DatasetCollectionMode mode);

  private:
    static std::string csvQuote(const std::string& value);
    static std::string jsonQuote(const std::string& value);
    static std::string nowIso8601();
    static std::string autoLabelFor(const DatasetCropCandidate& candidate, DatasetCollectionMode mode);
    static std::string autoLabelSourceFor(DatasetCollectionMode mode);
    static std::string sha256File(const std::filesystem::path& path);
    static std::string normalizedStopReason(const std::string& reason);
    bool writeSessionArtifacts(std::string& err) const;
    bool writeLabelsCsv(std::string& err) const;
    bool writeClassBalanceCsv(std::string& err) const;

    struct CapturedItem {
        std::string imageId;
        std::string cropPath;
        std::string sourceFramePath;
        std::string overlayPath;
        std::string sourceFrameId;
        std::string timestamp;
        int cropX = 0;
        int cropY = 0;
        int cropW = 0;
        int cropH = 0;
        std::string collectionMode;
        std::size_t batchIndex = 1;
        std::size_t batchTarget = 100;
        std::string autoLabel;
        std::string autoLabelSource;
        double autoLabelConfidence = 0.0;
        std::string autoLabelModelId;
        std::string hashSha256;
    };

    DatasetCaptureConfig config_;
    std::filesystem::path cropsDir_;
    std::filesystem::path sourceFramesDir_;
    std::filesystem::path overlaysDir_;
    std::filesystem::path metadataDir_;
    std::filesystem::path cropsCsvPath_;
    std::ofstream cropsCsv_;
    bool started_ = false;
    std::size_t collectedCount_ = 0;
    std::size_t currentBatchTarget_ = 100;
    std::size_t batchContinueCount_ = 0;
    std::string batchStopReason_ = "source_complete";
    std::string createdAt_;
    std::vector<std::string> batchPromptAudit_;
    std::vector<CapturedItem> items_;
};
