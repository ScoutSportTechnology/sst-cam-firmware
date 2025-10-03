use serde::{Deserialize, Serialize};
use std::{env, fs, path::PathBuf};

pub enum ConfigFilesEnums {
    Calibration,
    Device,
    Storage,
    Stream,
}

impl ConfigFilesEnums {
    pub fn default_path(&self) -> PathBuf {
        match self {
            ConfigFilesEnums::Calibration => {
                PathBuf::from("src/config/calibration/calibration.default.toml")
            }
            ConfigFilesEnums::Device => PathBuf::from("src/config/device/device.default.toml"),
            ConfigFilesEnums::Storage => PathBuf::from("src/config/storage/storage.default.toml"),
            ConfigFilesEnums::Stream => PathBuf::from("src/config/stream/stream.default.toml"),
        }
    }
    pub fn user_path(&self) -> PathBuf {
        match self {
            ConfigFilesEnums::Calibration => {
                PathBuf::from("src/config/calibration/calibration.user.toml")
            }
            ConfigFilesEnums::Device => PathBuf::from("src/config/device/device.user.toml"),
            ConfigFilesEnums::Storage => PathBuf::from("src/config/storage/storage.user.toml"),
            ConfigFilesEnums::Stream => PathBuf::from("src/config/stream/stream.user.toml"),
        }
    }
}

#[derive(Debug)]
pub enum ProfilesEnum {
    Default,
    User,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct ProfileConfigFiles {
    pub calibration: bool,
    pub device: bool,
    pub stream: bool,
    pub storage: bool,
}

impl ProfileConfigFiles {
    fn path() -> PathBuf {
        PathBuf::from("src/config/profile/profile.toml")
    }

    fn load() -> Result<ProfileConfigFiles, String> {
        let path = Self::path();
        let content = fs::read_to_string(&path)
            .map_err(|e| format!("Failed to read profile config: {}", e))?;
        let profile: ProfileConfigFiles = toml::from_str(&content)
            .map_err(|e| format!("Failed to parse profile config: {}", e))?;
        Ok(profile)
    }
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct ProfileConfig {
    pub user: ProfileConfigFiles,
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_profile_config_loading() {
        let profile = ProfileConfigFiles::load().unwrap();
        print!("Loaded profile config: {:?}", profile);
        assert!(profile.calibration);
        assert!(profile.device);
        assert!(profile.stream);
        assert!(profile.storage);
    }
}
