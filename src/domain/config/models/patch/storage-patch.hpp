#pragma once

#include "domain/config/models/storage.hpp"

namespace sst::config::domain {

inline auto apply_patch(StorageSectionData& modifiedData, const StorageSectionData& defaultData)
    -> void {
    if (defaultData.enabled) {
        modifiedData.enabled = defaultData.enabled;
    }
    if (defaultData.path) {
        modifiedData.path = defaultData.path;
    }
    if (defaultData.format) {
        modifiedData.format = defaultData.format;
    }
}

inline auto apply_patch(StorageData& modifiedData, const StorageData& defaultData) -> void {
    if (defaultData.recording) {
        if (!modifiedData.recording) {
            modifiedData.recording = StorageSectionData{};
        }
        apply_patch(*modifiedData.recording, *defaultData.recording);
    }

    if (defaultData.snapshots) {
        if (!modifiedData.snapshots) {
            modifiedData.snapshots = StorageSectionData{};
        }
        apply_patch(*modifiedData.snapshots, *defaultData.snapshots);
    }

    if (defaultData.logs) {
        if (!modifiedData.logs) {
            modifiedData.logs = StorageSectionData{};
        }
        apply_patch(*modifiedData.logs, *defaultData.logs);
    }

    if (defaultData.thumbnails) {
        if (!modifiedData.thumbnails) {
            modifiedData.thumbnails = StorageSectionData{};
        }
        apply_patch(*modifiedData.thumbnails, *defaultData.thumbnails);
    }
}

}  // namespace sst::config::domain
