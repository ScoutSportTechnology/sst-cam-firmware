-- =============================================================================
-- SST CAM FIRMWARE - DEFAULT SEED DATA
-- Run once on fresh install only
-- =============================================================================

-- Default admin user (password should be changed on first boot)
INSERT
    OR IGNORE INTO users (username, password_hash, role)
VALUES (
        'admin',
        'CHANGE_ON_FIRST_BOOT',
        'admin'
    );

-- Device config
INSERT
    OR IGNORE INTO device_config (key, value)
VALUES ('name', 'sst-cam'),
    (
        'manufacturer',
        'Scout Sport Technology'
    ),
    ('model', 'sst-cam-v2'),
    ('serial_number', '00000001'),
    ('version', '1.0.0'),
    ('timezone', 'UTC'),
    ('timestamp_mode', 'monotonic');

-- Network interfaces
INSERT
    OR IGNORE INTO network_config (interface, enabled, config)
VALUES (
        'bluetooth',
        1,
        '{"name":"sst-cam","password":"123456"}'
    ),
    (
        'wifi_ap',
        0,
        '{"ssid":"sst-cam","password":"123456"}'
    ),
    (
        'wifi_client',
        0,
        '{"ssid":"","password":"","static_ip":{"enabled":false,"ip":"","subnet":"","gateway":""}}'
    ),
    (
        'ethernet',
        0,
        '{"static_ip":{"enabled":false,"ip":"","subnet":"","gateway":""}}'
    );

-- Camera calibration defaults (2 cameras)
INSERT
    OR IGNORE INTO camera_calibration (
        camera_id,
        exposure,
        gain,
        white_balance,
        focus,
        width,
        height,
        format,
        fps,
        intrinsic_matrix,
        distortion_coefficients
    )
VALUES (
        0,
        100,
        1.0,
        'auto',
        'auto',
        1920,
        1080,
        'YUV422',
        60,
        '[1000.0,0.0,960.0,0.0,1000.0,540.0,0.0,0.0,1.0]',
        '[0.10,-0.05,0.00,0.00,0.00]'
    ),
    (
        1,
        100,
        1.0,
        'auto',
        'auto',
        1920,
        1080,
        'YUV422',
        60,
        '[1000.0,0.0,960.0,0.0,1000.0,540.0,0.0,0.0,1.0]',
        '[0.10,-0.05,0.00,0.00,0.00]'
    );

-- Microphone calibration defaults (2 mics)
INSERT
    OR IGNORE INTO microphone_calibration (
        mic_id,
        noise_reduction,
        sensitivity
    )
VALUES (0, 1, 1.0),
    (1, 1, 1.0);

-- Storage config
INSERT
    OR IGNORE INTO storage_config (type, enabled, format, path)
VALUES (
        'logs',
        1,
        'txt',
        '/var/log/sst/cam/'
    ),
    (
        'recording',
        1,
        'mp4',
        '/var/lib/sst/cam/videos/'
    ),
    (
        'snapshots',
        1,
        'jpg',
        '/var/lib/sst/cam/snapshots/'
    ),
    (
        'thumbnails',
        1,
        'jpg',
        '/var/lib/sst/cam/thumbnails/'
    );

-- Stream platform presets
INSERT
    OR IGNORE INTO stream_platform_config (
        platform,
        enabled,
        stream_type,
        url,
        codec,
        width,
        height,
        framerate,
        bitrate_kbps
    )
VALUES (
        'youtube',
        0,
        'rtmp',
        'rtmp://a.rtmp.youtube.com/live2',
        'h264',
        1920,
        1080,
        60,
        4000
    ),
    (
        'twitch',
        0,
        'rtmp',
        'rtmp://live.twitch.tv/app',
        'h264',
        1920,
        1080,
        60,
        4000
    ),
    (
        'facebook',
        0,
        'rtmps',
        'rtmps://live-api-s.facebook.com:443/rtmp/',
        'h264',
        1920,
        1080,
        60,
        4000
    ),
    (
        'instagram',
        0,
        'rtmp',
        'rtmp://live.instagram.com:443/rtmp/',
        'h264',
        1920,
        1080,
        60,
        4000
    ),
    (
        'tik_tok',
        0,
        'rtmp',
        'rtmp://live-api.tiktok.com/live',
        'h264',
        1920,
        1080,
        60,
        4000
    );

-- Default sport
INSERT OR IGNORE INTO sports (name) VALUES ('soccer');