-- =============================================================================
-- SST CAM FIRMWARE - DATABASE SCHEMA
-- Path: /var/lib/sst/cam/data.db
-- =============================================================================

PRAGMA journal_mode = WAL;

PRAGMA foreign_keys = ON;

PRAGMA synchronous = NORMAL;

-- =============================================================================
-- MIGRATIONS TRACKING
-- =============================================================================

CREATE TABLE IF NOT EXISTS migrations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT NOT NULL UNIQUE,
    applied_at TEXT NOT NULL DEFAULT(datetime('now'))
);

-- =============================================================================
-- USERS & AUTH
-- =============================================================================

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL CHECK (
        role IN ('admin', 'operator', 'viewer')
    ),
    created_at TEXT NOT NULL DEFAULT(datetime('now')),
    last_login TEXT
);

-- =============================================================================
-- CONFIG - DEVICE
-- =============================================================================

CREATE TABLE IF NOT EXISTS device_config (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- =============================================================================
-- CONFIG - NETWORK
-- =============================================================================

CREATE TABLE IF NOT EXISTS network_config (
    interface TEXT PRIMARY KEY, -- 'wifi_client', 'wifi_ap', 'ethernet', 'bluetooth'
    enabled INTEGER NOT NULL DEFAULT 0,
    config TEXT NOT NULL DEFAULT '{}' -- JSON blob per interface
);

-- =============================================================================
-- CONFIG - CAMERA CALIBRATION
-- =============================================================================

CREATE TABLE IF NOT EXISTS camera_calibration (
    camera_id INTEGER PRIMARY KEY,
    exposure INTEGER NOT NULL DEFAULT 100,
    gain REAL NOT NULL DEFAULT 1.0,
    white_balance TEXT NOT NULL DEFAULT 'auto',
    focus TEXT NOT NULL DEFAULT 'auto',
    width INTEGER NOT NULL DEFAULT 1920,
    height INTEGER NOT NULL DEFAULT 1080,
    format TEXT NOT NULL DEFAULT 'YUV422',
    fps INTEGER NOT NULL DEFAULT 60,
    intrinsic_matrix TEXT NOT NULL DEFAULT '[]', -- JSON array, 3x3 flat
    distortion_coefficients TEXT NOT NULL DEFAULT '[]', -- JSON array, 5 values
    last_calibration_date TEXT
);

-- =============================================================================
-- CONFIG - MICROPHONE CALIBRATION
-- =============================================================================

CREATE TABLE IF NOT EXISTS microphone_calibration (
    mic_id INTEGER PRIMARY KEY,
    noise_reduction INTEGER NOT NULL DEFAULT 1,
    sensitivity REAL NOT NULL DEFAULT 1.0,
    last_calibration_date TEXT
);

-- =============================================================================
-- CONFIG - STORAGE
-- =============================================================================

CREATE TABLE IF NOT EXISTS storage_config (
    type TEXT PRIMARY KEY, -- 'logs', 'recording', 'snapshots', 'thumbnails'
    enabled INTEGER NOT NULL DEFAULT 1,
    format TEXT NOT NULL,
    path TEXT NOT NULL
);

-- =============================================================================
-- CONFIG - STREAM PLATFORMS (built-in presets)
-- =============================================================================

CREATE TABLE IF NOT EXISTS stream_platform_config (
    platform TEXT PRIMARY KEY, -- 'youtube', 'twitch', 'facebook', 'instagram', 'tik_tok'
    enabled INTEGER NOT NULL DEFAULT 0,
    stream_type TEXT NOT NULL DEFAULT 'rtmp',
    url TEXT NOT NULL,
    stream_key TEXT NOT NULL DEFAULT '',
    codec TEXT NOT NULL DEFAULT 'h264',
    width INTEGER NOT NULL DEFAULT 1920,
    height INTEGER NOT NULL DEFAULT 1080,
    framerate INTEGER NOT NULL DEFAULT 60,
    bitrate_kbps INTEGER NOT NULL DEFAULT 4000
);

-- =============================================================================
-- CONFIG - CUSTOM STREAM SERVERS (user-defined)
-- =============================================================================

CREATE TABLE IF NOT EXISTS stream_custom_servers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE, -- user-defined label
    stream_type TEXT NOT NULL DEFAULT 'rtmp',
    url TEXT NOT NULL,
    stream_key TEXT NOT NULL DEFAULT '',
    codec TEXT NOT NULL DEFAULT 'h264',
    width INTEGER NOT NULL DEFAULT 1920,
    height INTEGER NOT NULL DEFAULT 1080,
    framerate INTEGER NOT NULL DEFAULT 60,
    bitrate_kbps INTEGER NOT NULL DEFAULT 4000,
    enabled INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT(datetime('now'))
);

-- =============================================================================
-- SPORT & TEAM DATA
-- =============================================================================

CREATE TABLE IF NOT EXISTS sports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE -- 'soccer', 'basketball', etc.
);

CREATE TABLE IF NOT EXISTS teams (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    sport_id INTEGER NOT NULL REFERENCES sports (id),
    logo_path TEXT, -- path to logo file on device
    created_at TEXT NOT NULL DEFAULT(datetime('now')),
    UNIQUE (name, sport_id)
);

CREATE TABLE IF NOT EXISTS players (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    team_id INTEGER NOT NULL REFERENCES teams (id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    number INTEGER,
    position TEXT,
    created_at TEXT NOT NULL DEFAULT(datetime('now'))
);

-- =============================================================================
-- MATCH DATA
-- =============================================================================

CREATE TABLE IF NOT EXISTS matches (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sport_id INTEGER NOT NULL REFERENCES sports (id),
    home_team_id INTEGER NOT NULL REFERENCES teams (id),
    away_team_id INTEGER NOT NULL REFERENCES teams (id),
    venue TEXT,
    scheduled_at TEXT,
    started_at TEXT,
    ended_at TEXT,
    status TEXT NOT NULL DEFAULT 'scheduled' CHECK (
        status IN (
            'scheduled',
            'live',
            'finished',
            'cancelled'
        )
    ),
    created_at TEXT NOT NULL DEFAULT(datetime('now'))
);

CREATE TABLE IF NOT EXISTS score_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    match_id INTEGER NOT NULL REFERENCES matches (id) ON DELETE CASCADE,
    team_id INTEGER NOT NULL REFERENCES teams (id),
    player_id INTEGER REFERENCES players (id), -- nullable, own goal or unknown
    event_type TEXT NOT NULL DEFAULT 'goal' CHECK (
        event_type IN ('goal', 'own_goal', 'penalty')
    ),
    match_time TEXT, -- e.g. "00:34:12" elapsed time in match
    timestamp TEXT NOT NULL DEFAULT(datetime('now'))
);

-- =============================================================================
-- SESSIONS & RECORDINGS
-- =============================================================================

CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    match_id INTEGER REFERENCES matches (id), -- nullable, can stream without a match
    started_at TEXT NOT NULL DEFAULT(datetime('now')),
    ended_at TEXT,
    status TEXT NOT NULL DEFAULT 'active' CHECK (
        status IN (
            'active',
            'finished',
            'crashed'
        )
    )
);

CREATE TABLE IF NOT EXISTS recordings (
    id TEXT PRIMARY KEY, -- UUID
    session_id INTEGER NOT NULL REFERENCES sessions (id) ON DELETE CASCADE,
    match_id INTEGER REFERENCES matches (id), -- denormalized for quick query
    file_path TEXT NOT NULL,
    filename TEXT NOT NULL,
    format TEXT NOT NULL DEFAULT 'mp4',
    duration_sec INTEGER,
    file_size_bytes INTEGER,
    cameras_active TEXT NOT NULL DEFAULT '[]', -- JSON array of camera ids
    started_at TEXT NOT NULL DEFAULT(datetime('now')),
    ended_at TEXT,
    status TEXT NOT NULL DEFAULT 'recording' CHECK (
        status IN (
            'recording',
            'finished',
            'corrupted'
        )
    )
);

-- =============================================================================
-- STREAM HISTORY
-- =============================================================================

CREATE TABLE IF NOT EXISTS stream_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL REFERENCES sessions (id) ON DELETE CASCADE,
    platform TEXT NOT NULL, -- 'youtube', 'twitch', or custom server name
    started_at TEXT NOT NULL DEFAULT(datetime('now')),
    ended_at TEXT,
    avg_bitrate_kbps INTEGER,
    dropped_frames INTEGER,
    status TEXT NOT NULL DEFAULT 'streaming' CHECK (
        status IN (
            'streaming',
            'finished',
            'failed'
        )
    )
);

-- =============================================================================
-- AI TRACKING EVENTS
-- =============================================================================

CREATE TABLE IF NOT EXISTS tracking_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL REFERENCES sessions (id) ON DELETE CASCADE,
    match_id INTEGER REFERENCES matches (id),
    player_id INTEGER REFERENCES players (id),
    event_type TEXT NOT NULL,
    -- 'ball_detected', 'ball_lost', 'player_tracked',
    -- 'camera_switch', 'goal_detected', 'offside_detected'
    camera_id INTEGER,
    payload TEXT NOT NULL DEFAULT '{}', -- JSON for event-specific data
    timestamp TEXT NOT NULL DEFAULT(datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_tracking_events_session ON tracking_events (session_id);

CREATE INDEX IF NOT EXISTS idx_tracking_events_match ON tracking_events (match_id);

CREATE INDEX IF NOT EXISTS idx_tracking_events_type ON tracking_events (event_type);

CREATE INDEX IF NOT EXISTS idx_score_events_match ON score_events (match_id);

CREATE INDEX IF NOT EXISTS idx_recordings_session ON recordings (session_id);

CREATE INDEX IF NOT EXISTS idx_stream_history_session ON stream_history (session_id);

CREATE INDEX IF NOT EXISTS idx_players_team ON players (team_id);