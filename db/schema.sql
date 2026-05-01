-- SQLite database export
PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

-- ── Users ────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS "users" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "username" TEXT NOT NULL
);

-- ── Connectivity ─────────────────────────────────────────────────────
-- The device is always WiFi-Direct GO. Credentials are known fixed values
-- shipped via config/wifi-direct.json and seeded by DbSeeder on first boot.
CREATE TABLE IF NOT EXISTS "config_wifi_direct" (
    "user_id" INTEGER PRIMARY KEY NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT true,
    "ssid" TEXT NOT NULL,
    "passphrase" TEXT NOT NULL,
    "channel" INTEGER NOT NULL DEFAULT 6,
    "ip_address" TEXT,
    CHECK (channel > 0),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE TABLE IF NOT EXISTS "config_bluetooth" (
    "user_id" INTEGER PRIMARY KEY NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT true,
    "name" TEXT NOT NULL DEFAULT 'sst-cam',
    "password" TEXT NOT NULL DEFAULT 123456,
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

-- ── Streaming destinations ───────────────────────────────────────────
-- platform=6 (companion_app) is system-managed: a single seeded row per user
-- with stream_key and url null. Other platforms require both.
CREATE TABLE IF NOT EXISTS "stream_config" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "user_id" INTEGER NOT NULL,
    -- 0=youtube | 1=twitch | 2=facebook | 3=instagram | 4=tiktok | 5=custom | 6=companion_app
    "platform" INTEGER NOT NULL,
    "name" TEXT NOT NULL,
    "enabled" BOOLEAN NOT NULL DEFAULT false,
    "stream_key" TEXT,
    -- 0=rtmp | 1=rtmps
    "stream_type" INTEGER NOT NULL DEFAULT 0,
    "url" TEXT,
    -- 0=h264 | 1=h265
    "codec" INTEGER NOT NULL DEFAULT 0,
    "width" INTEGER NOT NULL DEFAULT 1920,
    "height" INTEGER NOT NULL DEFAULT 1080,
    "framerate" INTEGER NOT NULL DEFAULT 60,
    "bitrate_kbps" INTEGER NOT NULL DEFAULT 4000,
    CHECK (platform >= 0 AND platform <= 6),
    CHECK (platform = 6 OR (stream_key IS NOT NULL AND url IS NOT NULL)),
    CHECK (stream_type >= 0 AND stream_type <= 1),
    CHECK (codec >= 0 AND codec <= 1),
    CHECK (framerate > 0),
    CHECK (bitrate_kbps > 0),
    CHECK (width > 0),
    CHECK (height > 0),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE UNIQUE INDEX IF NOT EXISTS "stream_config_idx_user_id_platform_name"
    ON "stream_config" ("user_id", "platform", "name");

-- One singleton companion-app row per user.
CREATE UNIQUE INDEX IF NOT EXISTS "stream_config_idx_companion_singleton"
    ON "stream_config" ("user_id") WHERE "platform" = 6;

-- ── Camera ───────────────────────────────────────────────────────────
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
    "event_clip_pre_seconds" INTEGER NOT NULL DEFAULT 60,
    "event_clip_post_seconds" INTEGER NOT NULL DEFAULT 60,
    CHECK (exposure > 0),
    CHECK (gain > 0),
    CHECK (fps > 0),
    CHECK (width > 0),
    CHECK (height > 0),
    CHECK (event_clip_pre_seconds > 0),
    CHECK (event_clip_post_seconds > 0),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id")
);

CREATE TABLE IF NOT EXISTS "camera_calibration" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    -- 0 | 1
    "camera_id" INTEGER NOT NULL,
    -- JSON, 3x3 flat array
    "intrinsic_matrix" BLOB NOT NULL,
    -- JSON, 5 values
    "distortion_coefficients" BLOB NOT NULL,
    "calibrated_at" TEXT NOT NULL DEFAULT 'datetime(now)',
    CHECK (camera_id >= 0 AND camera_id <= 1)
);

CREATE INDEX IF NOT EXISTS "camera_calibration_idx_camera_id_calibrated_at"
    ON "camera_calibration" ("camera_id", "calibrated_at");

-- ── Microphone ───────────────────────────────────────────────────────
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
    CHECK (mic_id >= 0 AND mic_id <= 1),
    CHECK (sensitivity > 0)
);

CREATE INDEX IF NOT EXISTS "microphone_calibration_idx_mic_id_calibrated_at"
    ON "microphone_calibration" ("mic_id", "calibrated_at");

-- ── Sports / teams / matches ─────────────────────────────────────────
-- Sports + event kinds are runtime-mutable. The same physical sport
-- ("soccer") may have multiple rows for different tournaments with
-- different period structures. Tables ship empty on first boot.
CREATE TABLE IF NOT EXISTS "sport" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "code" TEXT NOT NULL UNIQUE,
    "name" TEXT NOT NULL,
    -- # of time segments per match (2 halves, 4 quarters, 3 sets, …)
    "periods" INTEGER NOT NULL,
    CHECK (periods > 0)
);

CREATE TABLE IF NOT EXISTS "sport_event_kind" (
    "sport_id" INTEGER NOT NULL,
    "code" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    PRIMARY KEY ("sport_id", "code"),
    FOREIGN KEY ("sport_id") REFERENCES "sport" ("id") ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS "team" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "sport_id" INTEGER NOT NULL,
    "name" TEXT NOT NULL,
    "short_name" TEXT NOT NULL,
    UNIQUE ("sport_id", "name"),
    -- Composite key target for the FK from "match".
    UNIQUE ("id", "sport_id"),
    FOREIGN KEY ("sport_id") REFERENCES "sport" ("id")
);

CREATE TABLE IF NOT EXISTS "player" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "team_id" INTEGER NOT NULL,
    "name" TEXT NOT NULL,
    "jersey_number" INTEGER,
    UNIQUE ("team_id", "jersey_number"),
    FOREIGN KEY ("team_id") REFERENCES "team" ("id") ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS "match" (
    -- UUID
    "id" TEXT PRIMARY KEY NOT NULL,
    "user_id" INTEGER NOT NULL,
    "sport_id" INTEGER NOT NULL,
    "team_a_id" INTEGER NOT NULL,
    "team_b_id" INTEGER NOT NULL,
    "name" TEXT,
    "started_at" TEXT NOT NULL,
    "ended_at" TEXT,
    "current_period" INTEGER NOT NULL DEFAULT 1,
    "final_score_a" INTEGER NOT NULL DEFAULT 0,
    "final_score_b" INTEGER NOT NULL DEFAULT 0,
    CHECK (team_a_id != team_b_id),
    CHECK (current_period > 0),
    CHECK (final_score_a >= 0),
    CHECK (final_score_b >= 0),
    FOREIGN KEY ("user_id") REFERENCES "users" ("id"),
    FOREIGN KEY ("sport_id") REFERENCES "sport" ("id"),
    -- Composite FKs guarantee both teams belong to this match's sport.
    FOREIGN KEY ("team_a_id", "sport_id") REFERENCES "team" ("id", "sport_id"),
    FOREIGN KEY ("team_b_id", "sport_id") REFERENCES "team" ("id", "sport_id")
);

CREATE INDEX IF NOT EXISTS "match_idx_user_id_started_at"
    ON "match" ("user_id", "started_at");

CREATE TABLE IF NOT EXISTS "match_event" (
    -- UUID
    "id" TEXT PRIMARY KEY NOT NULL,
    "match_id" TEXT NOT NULL,
    "sport_id" INTEGER NOT NULL,
    "event_code" TEXT NOT NULL,
    "period" INTEGER NOT NULL,
    -- seconds since match.started_at
    "timestamp_in_match" REAL NOT NULL,
    "wall_clock_at" TEXT NOT NULL,
    "metadata_json" TEXT,
    CHECK (period > 0),
    CHECK (timestamp_in_match >= 0),
    FOREIGN KEY ("match_id") REFERENCES "match" ("id") ON DELETE CASCADE,
    FOREIGN KEY ("sport_id", "event_code") REFERENCES "sport_event_kind" ("sport_id", "code")
);

CREATE INDEX IF NOT EXISTS "match_event_idx_match_id_timestamp"
    ON "match_event" ("match_id", "timestamp_in_match");

-- ── Recordings ───────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS "recording" (
    -- UUID
    "id" TEXT PRIMARY KEY NOT NULL,
    "match_id" TEXT,
    -- 0=full_game | 1=event_clip
    "kind" INTEGER NOT NULL,
    "file_path" TEXT NOT NULL,
    "started_at" TEXT NOT NULL,
    "ended_at" TEXT,
    CHECK (kind IN (0, 1)),
    FOREIGN KEY ("match_id") REFERENCES "match" ("id") ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS "recording_idx_match_id_kind"
    ON "recording" ("match_id", "kind");

CREATE TABLE IF NOT EXISTS "recording_segment" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "recording_id" TEXT NOT NULL,
    "sequence" INTEGER NOT NULL,
    "file_path" TEXT NOT NULL,
    "started_at" TEXT NOT NULL,
    "ended_at" TEXT,
    UNIQUE ("recording_id", "sequence"),
    CHECK (sequence >= 0),
    FOREIGN KEY ("recording_id") REFERENCES "recording" ("id") ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS "event_clip" (
    -- UUID, matches the "event_<uuid>" directory under <match_uuid>/
    "id" TEXT PRIMARY KEY NOT NULL,
    "match_event_id" TEXT NOT NULL,
    "recording_id" TEXT NOT NULL,
    "file_path" TEXT NOT NULL,
    "pre_seconds" INTEGER NOT NULL,
    "post_seconds" INTEGER NOT NULL,
    CHECK (pre_seconds > 0),
    CHECK (post_seconds > 0),
    FOREIGN KEY ("match_event_id") REFERENCES "match_event" ("id") ON DELETE CASCADE,
    FOREIGN KEY ("recording_id") REFERENCES "recording" ("id") ON DELETE CASCADE
);

COMMIT;
