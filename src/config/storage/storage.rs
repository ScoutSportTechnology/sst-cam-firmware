use crate::domains::common::config::{ConfigError, ConfigFilesEnums, ConfigUtils, ProfilesEnum};
use once_cell::sync::Lazy;
use parking_lot::RwLock;
use serde::{Deserialize, Serialize};

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StorageConfiguration {
    pub enabled: bool,
    pub path: String,
    pub format: String,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StorageConfig {
    pub enabled: bool,
    pub recording: StorageConfiguration,
    pub snapshots: StorageConfiguration,
    pub logs: StorageConfiguration,
    pub thumbnails: StorageConfiguration,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StorageData {
    pub storage: StorageConfig,
}

// Direct access wrapper for Storage configuration
pub struct StorageAccess {
    data: RwLock<StorageData>,
}

impl StorageAccess {
    fn new() -> Result<Self, ConfigError> {
        let data = Self::load_storage_config()?;
        Ok(StorageAccess {
            data: RwLock::new(data),
        })
    }

    fn load_storage_config() -> Result<StorageData, ConfigError> {
        let profile = ConfigUtils::load_profile_config()?;

        let path = if profile.user.storage {
            ConfigFilesEnums::Storage.to_file(&ProfilesEnum::User)
        } else {
            ConfigFilesEnums::Storage.to_file(&ProfilesEnum::Default)
        };

        let content = ConfigUtils::load_config_file(&path)?;
        toml::from_str(&content)
            .map_err(|e| ConfigError::ParseError(format!("storage.toml: {}", e)))
    }

    fn save_to_user_profile(&self) -> Result<(), ConfigError> {
        let path = ConfigFilesEnums::Storage.to_file(&ProfilesEnum::User);
        let data = self.data.read().clone();
        ConfigUtils::save_config_file(&path, &data)?;
        ConfigUtils::update_profile_flag("storage", true)?;
        Ok(())
    }

    // Direct access properties - what you wanted: Storage.enabled, etc.
    pub fn enabled(&self) -> bool {
        self.data.read().storage.enabled
    }

    pub fn recording_enabled(&self) -> bool {
        self.data.read().storage.recording.enabled
    }

    pub fn recording_path(&self) -> String {
        self.data.read().storage.recording.path.clone()
    }

    pub fn snapshots_enabled(&self) -> bool {
        self.data.read().storage.snapshots.enabled
    }

    pub fn snapshots_path(&self) -> String {
        self.data.read().storage.snapshots.path.clone()
    }

    // Update methods - what you wanted: Storage.update.enabled(true)
    pub fn update(&self) -> StorageUpdate {
        StorageUpdate { storage: self }
    }

    pub fn reset_to_default(&self) -> Result<(), ConfigError> {
        let default_path = ConfigFilesEnums::Storage.to_file(&ProfilesEnum::Default);
        let content = ConfigUtils::load_config_file(&default_path)?;
        let default_data: StorageData = toml::from_str(&content)
            .map_err(|e| ConfigError::ParseError(format!("storage.toml: {}", e)))?;

        {
            let mut data = self.data.write();
            *data = default_data;
        }

        ConfigUtils::update_profile_flag("storage", false)?;

        // Remove user file if it exists
        let user_path = ConfigFilesEnums::Storage.to_file(&ProfilesEnum::User);
        let _ = std::fs::remove_file(user_path);

        Ok(())
    }

    // Get full config for complex operations
    pub fn get_full_config(&self) -> StorageData {
        self.data.read().clone()
    }
}

// Update interface - what you wanted: Storage.update.enabled(true)
pub struct StorageUpdate<'a> {
    storage: &'a StorageAccess,
}

impl<'a> StorageUpdate<'a> {
    pub fn enabled(self, value: bool) -> Result<(), ConfigError> {
        {
            let mut data = self.storage.data.write();
            data.storage.enabled = value;
        }
        self.storage.save_to_user_profile()
    }

    pub fn recording_enabled(self, value: bool) -> Result<(), ConfigError> {
        {
            let mut data = self.storage.data.write();
            data.storage.recording.enabled = value;
        }
        self.storage.save_to_user_profile()
    }

    pub fn recording_path<S: Into<String>>(self, value: S) -> Result<(), ConfigError> {
        {
            let mut data = self.storage.data.write();
            data.storage.recording.path = value.into();
        }
        self.storage.save_to_user_profile()
    }

    // Batch update method for multiple changes
    pub fn batch<F>(self, updater: F) -> Result<(), ConfigError>
    where
        F: FnOnce(&mut StorageData),
    {
        {
            let mut data = self.storage.data.write();
            updater(&mut *data);
        }
        self.storage.save_to_user_profile()
    }
}

// Global Storage instance - what you'll use in your app
static STORAGE_INTERNAL: Lazy<StorageAccess> =
    Lazy::new(|| StorageAccess::new().expect("Failed to initialize Storage configuration"));

pub static STORAGE: Lazy<&'static StorageAccess> = Lazy::new(|| &STORAGE_INTERNAL);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_storage_data() {
        // Test 1: Get storage configuration data
        println!("=== Storage Configuration Data ===");
        println!("Enabled: {}", STORAGE.enabled());
        println!("Recording Enabled: {}", STORAGE.recording_enabled());
        println!("Recording Path: {}", STORAGE.recording_path());
        println!("Snapshots Enabled: {}", STORAGE.snapshots_enabled());
        println!("Snapshots Path: {}", STORAGE.snapshots_path());

        // Verify we can access the data without panicking
        assert!(!STORAGE.recording_path().is_empty());
        assert!(!STORAGE.snapshots_path().is_empty());
    }

    #[test]
    fn test_update_storage_data() -> Result<(), ConfigError> {
        // Test 2: Update storage configuration data
        println!("=== Updating Storage Configuration ===");

        // Test individual updates
        STORAGE.update().enabled(true)?;
        println!("Updated enabled to: {}", STORAGE.enabled());
        assert!(STORAGE.enabled());

        STORAGE.update().recording_enabled(true)?;
        println!("Updated recording enabled to: {}", STORAGE.recording_enabled());
        assert!(STORAGE.recording_enabled());

        STORAGE.update().recording_path("/tmp/recordings")?;
        println!("Updated recording path to: {}", STORAGE.recording_path());
        assert_eq!(STORAGE.recording_path(), "/tmp/recordings");

        // Test batch update
        STORAGE.update().batch(|storage| {
            storage.storage.enabled = true;
            storage.storage.recording.enabled = true;
            storage.storage.recording.path = "/home/recordings".to_string();
            storage.storage.snapshots.enabled = true;
        })?;

        println!("Batch updated - Enabled: {}, Recording Path: {}, Snapshots Enabled: {}",
                STORAGE.enabled(), STORAGE.recording_path(), STORAGE.snapshots_enabled());

        assert!(STORAGE.enabled());
        assert_eq!(STORAGE.recording_path(), "/home/recordings");
        assert!(STORAGE.snapshots_enabled());

        Ok(())
    }
}
