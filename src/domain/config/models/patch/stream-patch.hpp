#pragma once

#include "domain/config/models/stream.hpp"

namespace sst::config::domain {

inline auto apply_patch(StreamPlatformSettingsData& modifiedData,
                        const StreamPlatformSettingsData& defaultData) -> void {
    if (defaultData.width) {
        modifiedData.width = defaultData.width;
    }
    if (defaultData.height) {
        modifiedData.height = defaultData.height;
    }
    if (defaultData.framerate) {
        modifiedData.framerate = defaultData.framerate;
    }
    if (defaultData.bitrate_kbps) {
        modifiedData.bitrate_kbps = defaultData.bitrate_kbps;
    }
}

inline auto apply_patch(StreamPlatformData& modifiedData, const StreamPlatformData& defaultData)
    -> void {
    if (defaultData.enabled) {
        modifiedData.enabled = defaultData.enabled;
    }
    if (defaultData.url) {
        modifiedData.url = defaultData.url;
    }
    if (defaultData.key) {
        modifiedData.key = defaultData.key;
    }
    if (defaultData.stream_type) {
        modifiedData.stream_type = defaultData.stream_type;
    }
    if (defaultData.codec) {
        modifiedData.codec = defaultData.codec;
    }
    if (defaultData.settings) {
        if (!modifiedData.settings) {
            modifiedData.settings = StreamPlatformSettingsData{};
        }
        apply_patch(*modifiedData.settings, *defaultData.settings);
    }
}

inline auto apply_patch(StreamData& modifiedData, const StreamData& defaultData) -> void {
    if (defaultData.youtube) {
        if (!modifiedData.youtube) {
            modifiedData.youtube = StreamPlatformData{};
        }
        apply_patch(*modifiedData.youtube, *defaultData.youtube);
    }

    if (defaultData.twitch) {
        if (!modifiedData.twitch) {
            modifiedData.twitch = StreamPlatformData{};
        }
        apply_patch(*modifiedData.twitch, *defaultData.twitch);
    }

    if (defaultData.facebook) {
        if (!modifiedData.facebook) {
            modifiedData.facebook = StreamPlatformData{};
        }
        apply_patch(*modifiedData.facebook, *defaultData.facebook);
    }

    if (defaultData.instagram) {
        if (!modifiedData.instagram) {
            modifiedData.instagram = StreamPlatformData{};
        }
        apply_patch(*modifiedData.instagram, *defaultData.instagram);
    }

    if (defaultData.tik_tok) {
        if (!modifiedData.tik_tok) {
            modifiedData.tik_tok = StreamPlatformData{};
        }
        apply_patch(*modifiedData.tik_tok, *defaultData.tik_tok);
    }

    if (defaultData.custom) {
        modifiedData.custom = defaultData.custom;
    }
}

}  // namespace sst::config::domain
