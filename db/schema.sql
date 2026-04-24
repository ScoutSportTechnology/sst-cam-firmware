-- SQLite database export
PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS "users" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "username" TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS "config_client" (
    "user_id" INTEGER PRIMARY KEY NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT false,
    -- wifi=0 | ethernet=1
    "medium" INTEGER NOT NULL DEFAULT 0,
    "ssid" TEXT,
    "wifi_password" TEXT,
    "static_ip" BOOLEAN NOT NULL DEFAULT false,
    "ip_address" TEXT,
    "subnet_mask" TEXT,
    "gateway" TEXT,
    CHECK (
        medium = 1
        OR ssid IS NOT NULL
    ),
    CHECK (
        medium = 1
        OR wifi_password IS NOT NULL
    ),
    CHECK (
        static_ip = false
        OR ip_address IS NOT NULL
    ),
    CHECK (
        static_ip = false
        OR subnet_mask IS NOT NULL
    ),
    CHECK (
        static_ip = false
        OR gateway IS NOT NULL
    ),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE TABLE IF NOT EXISTS "config_access_point" (
    "user_id" INTEGER PRIMARY KEY NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT false,
    -- wifi=0 | ethernet=1
    "medium" INTEGER NOT NULL DEFAULT 0,
    "ssid" TEXT DEFAULT 'sst-cam',
    "wifi_password" TEXT DEFAULT 123456,
    "static_ip" BOOLEAN NOT NULL DEFAULT false,
    "ip_address" TEXT,
    "subnet_mask" TEXT,
    "gateway" TEXT,
    CHECK (
        medium = 1
        OR ssid IS NOT NULL
    ),
    CHECK (
        medium = 1
        OR wifi_password IS NOT NULL
    ),
    CHECK (
        static_ip = false
        OR ip_address IS NOT NULL
    ),
    CHECK (
        static_ip = false
        OR subnet_mask IS NOT NULL
    ),
    CHECK (
        static_ip = false
        OR gateway IS NOT NULL
    ),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE TABLE IF NOT EXISTS "config_bluetooth" (
    "user_id" INTEGER PRIMARY KEY NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT true,
    "name" TEXT NOT NULL DEFAULT 'sst-cam',
    "password" TEXT NOT NULL DEFAULT 123456,
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE TABLE IF NOT EXISTS "stream_config" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "user_id" INTEGER NOT NULL UNIQUE,
    -- 0=youtube | 1=twitch | 2=facebook | 3=instagram | 4=tiktok | 5=custom
    "platform" INTEGER NOT NULL,
    -- user defined label for custom, platform name for presets
    "name" TEXT NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT false,
    "stream_key" TEXT NOT NULL,
    -- 0=rtmp | 1=rtmps
    "stream_type" INTEGER NOT NULL DEFAULT 0,
    "url" TEXT NOT NULL,
    -- 0=h264 | 1=h265
    "codec" INTEGER NOT NULL DEFAULT 0,
    "width" INTEGER NOT NULL DEFAULT 1920,
    "height" INTEGER NOT NULL DEFAULT 1080,
    "framerate" INTEGER NOT NULL DEFAULT 60,
    "bitrate_kbps" INTEGER NOT NULL DEFAULT 4000,
    CHECK (
        platform >= 0
        AND platform <= 5
    ),
    CHECK (
        stream_type >= 0
        AND stream_type <= 1
    ),
    CHECK (
        codec >= 0
        AND codec <= 1
    ),
    CHECK (framerate > 0),
    CHECK (bitrate_kbps > 0),
    CHECK (width > 0),
    CHECK (height > 0),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

-- Indexes
CREATE UNIQUE INDEX IF NOT EXISTS "stream_config_idx_stream_config_user_id_platform_name" ON "stream_config" ("user_id", "platform", "name");

CREATE TABLE IF NOT EXISTS "storage_config" (
    "user_id" INTEGER NOT NULL,
    -- 0=logs | 1=recording | 2=snapshots | 3=thumbnails
    "type" INTEGER NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT true,
    -- 0=txt | 1=mp4 | 2=jpg
    "format" INTEGER NOT NULL,
    "path" TEXT NOT NULL,
    PRIMARY KEY ("user_id", "type"),
    CHECK (
        type >= 0
        AND type <= 3
    ),
    CHECK (
        format >= 0
        AND format <= 2
    ),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE TABLE IF NOT EXISTS "camera_config" (
    "user_id" INTEGER PRIMARY KEY NOT NULL,
    "exposure" INTEGER NOT NULL DEFAULT 100,
    "gain" REAL NOT NULL DEFAULT 1,
    -- 0=auto | 1=manual
    "white_balance" INTEGER NOT NULL DEFAULT 0,
    -- 0=auto | 1=manual
    "focus" INTEGER NOT NULL DEFAULT 0,
    "width" INTEGER NOT NULL DEFAULT 1920,
    "height" INTEGER NOT NULL DEFAULT 1080,
    -- 0=YUV422
    "format" INTEGER NOT NULL DEFAULT 0,
    "fps" INTEGER NOT NULL DEFAULT 60,
    CHECK (exposure > 0),
    CHECK (gain > 0),
    CHECK (fps > 0),
    CHECK (width > 0),
    CHECK (height > 0),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE TABLE IF NOT EXISTS "camera_calibration" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    -- 0 | 1
    "camera_id" INTEGER NOT NULL,
    -- JSONB, 3x3 flat array
    "intrinsic_matrix" BLOB NOT NULL,
    -- JSONB, 5 values
    "distortion_coefficients" BLOB NOT NULL,
    "calibrated_at" TEXT NOT NULL DEFAULT 'datetime(now)',
    CHECK (
        camera_id >= 0
        AND camera_id <= 1
    )
);

-- Indexes
CREATE INDEX IF NOT EXISTS "camera_calibration_idx_camera_calibration_camera_id_calibrat" ON "camera_calibration" ("camera_id", "calibrated_at");

CREATE TABLE IF NOT EXISTS "microphone_config" (
    "user_id" INTEGER PRIMARY KEY NOT NULL,
    "noise_reduction" BOOLEAN NOT NULL DEFAULT true,
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE TABLE IF NOT EXISTS "microphone_calibration" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    -- 0 | 1
    "mic_id" INTEGER NOT NULL,
    "sensitivity" REAL NOT NULL DEFAULT 1,
    "calibrated_at" TEXT NOT NULL DEFAULT 'datetime(now)',
    CHECK (
        mic_id >= 0
        AND mic_id <= 1
    ),
    CHECK (sensitivity > 0)
);

-- Indexes
CREATE INDEX IF NOT EXISTS "microphone_calibration_idx_microphone_calibration_mic_id_cal" ON "microphone_calibration" ("mic_id", "calibrated_at");

COMMIT;