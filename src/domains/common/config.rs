use serde::{Deserialize, Serialize};
use std::{env, fs, path::PathBuf};

#[derive(Debug)]
pub enum ProfilesEnum {
    Default,
    User,
}

impl ProfilesEnum {
    pub fn to_foldername(&self) -> PathBuf {
        let mut folderpath = env::current_dir().unwrap();
        folderpath.push("src/config");
        match self {
            ProfilesEnum::Default => folderpath.push("default"),
            ProfilesEnum::User => folderpath.push("user"),
        }
        folderpath
    }
}

#[derive(Debug)]
pub enum ConfigFilesEnums {
    Calibration,
    Device,
    Storage,
    Stream,
}

impl ConfigFilesEnums {
    pub fn to_file(&self, profile: &ProfilesEnum) -> PathBuf {
        let mut filepath = profile.to_foldername();
        match self {
            ConfigFilesEnums::Calibration => filepath.push("calibration.toml"),
            ConfigFilesEnums::Device => filepath.push("device.toml"),
            ConfigFilesEnums::Storage => filepath.push("storage.toml"),
            ConfigFilesEnums::Stream => filepath.push("stream.toml"),
        }
        filepath
    }
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct ProfileConfigFiles {
    pub calibration: bool,
    pub device: bool,
    pub stream: bool,
    pub storage: bool,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct ProfileConfig {
    pub user: ProfileConfigFiles,
}

#[derive(Debug)]
pub enum ConfigError {
    FileNotFound(String),
    ParseError(String),
    IoError(String),
}

impl std::fmt::Display for ConfigError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ConfigError::FileNotFound(path) => write!(f, "Configuration file not found: {}", path),
            ConfigError::ParseError(msg) => write!(f, "Failed to parse configuration: {}", msg),
            ConfigError::IoError(msg) => write!(f, "IO error: {}", msg),
        }
    }
}

impl std::error::Error for ConfigError {}

/// Common configuration utilities
pub struct ConfigUtils;

impl ConfigUtils {
    /// Load profile configuration
    pub fn load_profile_config() -> Result<ProfileConfig, ConfigError> {
        let profile_path = env::current_dir()
            .map_err(|e| ConfigError::IoError(e.to_string()))?
            .join("src/config/profile.toml");

        let profile_content = fs::read_to_string(&profile_path)
            .map_err(|_| ConfigError::FileNotFound(profile_path.display().to_string()))?;

        toml::from_str(&profile_content)
            .map_err(|e| ConfigError::ParseError(format!("profile.toml: {}", e)))
    }

    /// Update profile flag for a specific config type
    pub fn update_profile_flag(config_type: &str, enabled: bool) -> Result<(), ConfigError> {
        let profile_path = env::current_dir()
            .map_err(|e| ConfigError::IoError(e.to_string()))?
            .join("src/config/profile.toml");

        // Load current profile
        let mut profile = Self::load_profile_config()?;

        // Update the specific flag
        match config_type {
            "stream" => profile.user.stream = enabled,
            "device" => profile.user.device = enabled,
            "storage" => profile.user.storage = enabled,
            "calibration" => profile.user.calibration = enabled,
            _ => return Err(ConfigError::ParseError("Invalid config type".to_string())),
        }

        // Save updated profile
        let toml_content = toml::to_string_pretty(&profile)
            .map_err(|e| ConfigError::ParseError(format!("Failed to serialize profile: {}", e)))?;

        fs::write(&profile_path, toml_content)
            .map_err(|e| ConfigError::IoError(format!("Failed to write profile.toml: {}", e)))?;

        Ok(())
    }

    /// Ensure user config directory exists
    pub fn ensure_user_dir_exists() -> Result<(), ConfigError> {
        let user_dir = ProfilesEnum::User.to_foldername();
        fs::create_dir_all(&user_dir)
            .map_err(|e| ConfigError::IoError(format!("Failed to create user directory: {}", e)))?;
        Ok(())
    }

    /// Load configuration file content
    pub fn load_config_file(path: &PathBuf) -> Result<String, ConfigError> {
        fs::read_to_string(path).map_err(|_| ConfigError::FileNotFound(path.display().to_string()))
    }

    /// Save configuration to file
    pub fn save_config_file<T: Serialize>(path: &PathBuf, config: &T) -> Result<(), ConfigError> {
        Self::ensure_user_dir_exists()?;

        let toml_content = toml::to_string_pretty(config)
            .map_err(|e| ConfigError::ParseError(format!("Failed to serialize config: {}", e)))?;

        fs::write(path, toml_content)
            .map_err(|e| ConfigError::IoError(format!("Failed to write {}: {}", path.display(), e)))
    }
}
