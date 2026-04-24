BEGIN TRANSACTION;

-- User
INSERT INTO "users" ("username") VALUES ('admin');

-- Device info
INSERT INTO
    "device_info" (
        "model",
        "name",
        "timezone",
        "version"
    )
VALUES (
        'sst-cam-v2',
        'sst-cam',
        'UTC',
        '1.0.0'
    );

-- Network (all disabled by default)
INSERT INTO
    "config_client" (
        "user_id",
        "enabled",
        "medium",
        "static_ip"
    )
VALUES (1, false, 0, false);

INSERT INTO
    "config_access_point" (
        "user_id",
        "enabled",
        "medium",
        "ssid",
        "wifi_password",
        "static_ip"
    )
VALUES (
        1,
        false,
        0,
        'sst-cam',
        '123456',
        false
    );

INSERT INTO
    "config_bluetooth" (
        "user_id",
        "enabled",
        "name",
        "password"
    )
VALUES (1, true, 'sst-cam', '123456');

-- Storage config
INSERT INTO
    "storage_config" (
        "user_id",
        "type",
        "enabled",
        "format",
        "path"
    )
VALUES (
        1,
        0,
        true,
        0,
        '/var/log/sst/cam/'
    ),
    (
        1,
        1,
        true,
        1,
        '/var/lib/sst/cam/videos/'
    ),
    (
        1,
        2,
        true,
        2,
        '/var/lib/sst/cam/snapshots/'
    ),
    (
        1,
        3,
        true,
        2,
        '/var/lib/sst/cam/thumbnails/'
    );

-- Camera calibration defaults
INSERT INTO
    "camera_calibration" (
        "camera_id",
        "exposure",
        "gain",
        "white_balance",
        "focus",
        "width",
        "height",
        "format",
        "fps",
        "intrinsic_matrix",
        "distortion_coefficients"
    )
VALUES (
        0,
        100,
        1.0,
        0,
        0,
        1920,
        1080,
        0,
        60,
        jsonb (
            '[1000.0,0.0,960.0,0.0,1000.0,540.0,0.0,0.0,1.0]'
        ),
        jsonb ('[0.10,-0.05,0.00,0.00,0.00]')
    ),
    (
        1,
        100,
        1.0,
        0,
        0,
        1920,
        1080,
        0,
        60,
        jsonb (
            '[1000.0,0.0,960.0,0.0,1000.0,540.0,0.0,0.0,1.0]'
        ),
        jsonb ('[0.10,-0.05,0.00,0.00,0.00]')
    );

-- Microphone calibration defaults
INSERT INTO
    "microphone_calibration" (
        "mic_id",
        "noise_reduction",
        "sensitivity"
    )
VALUES (0, true, 1.0),
    (1, true, 1.0);

COMMIT;