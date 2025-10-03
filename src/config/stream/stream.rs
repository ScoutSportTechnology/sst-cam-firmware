use crate::domains::common::config::{ConfigError, ConfigFilesEnums, ConfigUtils, ProfilesEnum};
use once_cell::sync::Lazy;
use parking_lot::RwLock;
use serde::{Deserialize, Serialize};

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StreamSettings {
    pub width: u32,
    pub height: u32,
    pub framerate: u32,
    pub bitrate_kbps: u32,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StreamRecipientsSettings {
    pub enabled: bool,
    pub url: String,
    pub key: String,
    pub format: String,
    pub codec: String,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StreamRecipients {
    pub youtube: StreamRecipientsSettings,
    pub facebook: StreamRecipientsSettings,
    pub twitch: StreamRecipientsSettings,
    pub instagram: StreamRecipientsSettings,
    pub tik_tok: StreamRecipientsSettings,
    pub custom: StreamRecipientsSettings,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StreamConfig {
    pub enabled: bool,
    pub settings: StreamSettings,
    pub recipients: StreamRecipients,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StreamData {
    pub stream: StreamConfig,
}

// Direct access wrapper for Stream configuration
pub struct StreamAccess {
    data: RwLock<StreamData>,
}

impl StreamAccess {
    fn new() -> Result<Self, ConfigError> {
        let data = Self::load_stream_config()?;
        Ok(StreamAccess {
            data: RwLock::new(data),
        })
    }

    fn load_stream_config() -> Result<StreamData, ConfigError> {
        let profile = ConfigUtils::load_profile_config()?;

        let path = if profile.user.stream {
            ConfigFilesEnums::Stream.to_file(&ProfilesEnum::User)
        } else {
            ConfigFilesEnums::Stream.to_file(&ProfilesEnum::Default)
        };

        let content = ConfigUtils::load_config_file(&path)?;
        toml::from_str(&content).map_err(|e| ConfigError::ParseError(format!("stream.toml: {}", e)))
    }

    fn save_to_user_profile(&self) -> Result<(), ConfigError> {
        let path = ConfigFilesEnums::Stream.to_file(&ProfilesEnum::User);
        let data = self.data.read().clone();
        ConfigUtils::save_config_file(&path, &data)?;
        ConfigUtils::update_profile_flag("stream", true)?;
        Ok(())
    }

    // Direct access properties - what you wanted: Stream.enabled, Stream.width, etc.
    pub fn enabled(&self) -> bool {
        self.data.read().stream.enabled
    }

    pub fn width(&self) -> u32 {
        self.data.read().stream.settings.width
    }

    pub fn height(&self) -> u32 {
        self.data.read().stream.settings.height
    }

    pub fn framerate(&self) -> u32 {
        self.data.read().stream.settings.framerate
    }

    pub fn bitrate_kbps(&self) -> u32 {
        self.data.read().stream.settings.bitrate_kbps
    }

    // Update methods - what you wanted: Stream.update.framerate(60)
    pub fn update(&self) -> StreamUpdate {
        StreamUpdate { stream: self }
    }

    pub fn reset_to_default(&self) -> Result<(), ConfigError> {
        let default_path = ConfigFilesEnums::Stream.to_file(&ProfilesEnum::Default);
        let content = ConfigUtils::load_config_file(&default_path)?;
        let default_data: StreamData = toml::from_str(&content)
            .map_err(|e| ConfigError::ParseError(format!("stream.toml: {}", e)))?;

        {
            let mut data = self.data.write();
            *data = default_data;
        }

        ConfigUtils::update_profile_flag("stream", false)?;

        // Remove user file if it exists
        let user_path = ConfigFilesEnums::Stream.to_file(&ProfilesEnum::User);
        let _ = std::fs::remove_file(user_path);

        Ok(())
    }

    // Get full config for complex operations
    pub fn get_full_config(&self) -> StreamData {
        self.data.read().clone()
    }
}

// Update interface - what you wanted: Stream.update.framerate(60)
pub struct StreamUpdate<'a> {
    stream: &'a StreamAccess,
}

impl<'a> StreamUpdate<'a> {
    pub fn enabled(self, value: bool) -> Result<(), ConfigError> {
        {
            let mut data = self.stream.data.write();
            data.stream.enabled = value;
        }
        self.stream.save_to_user_profile()
    }

    pub fn width(self, value: u32) -> Result<(), ConfigError> {
        {
            let mut data = self.stream.data.write();
            data.stream.settings.width = value;
        }
        self.stream.save_to_user_profile()
    }

    pub fn height(self, value: u32) -> Result<(), ConfigError> {
        {
            let mut data = self.stream.data.write();
            data.stream.settings.height = value;
        }
        self.stream.save_to_user_profile()
    }

    pub fn framerate(self, value: u32) -> Result<(), ConfigError> {
        {
            let mut data = self.stream.data.write();
            data.stream.settings.framerate = value;
        }
        self.stream.save_to_user_profile()
    }

    pub fn bitrate_kbps(self, value: u32) -> Result<(), ConfigError> {
        {
            let mut data = self.stream.data.write();
            data.stream.settings.bitrate_kbps = value;
        }
        self.stream.save_to_user_profile()
    }

    // Batch update method for multiple changes
    pub fn batch<F>(self, updater: F) -> Result<(), ConfigError>
    where
        F: FnOnce(&mut StreamData),
    {
        {
            let mut data = self.stream.data.write();
            updater(&mut *data);
        }
        self.stream.save_to_user_profile()
    }
}

// Global Stream instance - what you'll use in your app
static STREAM_INTERNAL: Lazy<StreamAccess> =
    Lazy::new(|| StreamAccess::new().expect("Failed to initialize Stream configuration"));

pub static STREAM: Lazy<&'static StreamAccess> = Lazy::new(|| &STREAM_INTERNAL);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_stream_data() {
        // Test 1: Get stream configuration data
        println!("=== Stream Configuration Data ===");
        println!("Enabled: {}", STREAM.enabled());
        println!("Width: {}", STREAM.width());
        println!("Height: {}", STREAM.height());
        println!("Framerate: {}", STREAM.framerate());
        println!("Bitrate: {} kbps", STREAM.bitrate_kbps());

        // Verify we can access the data without panicking
        assert!(STREAM.width() > 0);
        assert!(STREAM.height() > 0);
        assert!(STREAM.framerate() > 0);
    }

    #[test]
    fn test_update_stream_data() -> Result<(), ConfigError> {
        // Test 2: Update stream configuration data
        println!("=== Updating Stream Configuration ===");

        // Test individual updates
        STREAM.update().framerate(30)?;
        println!("Updated framerate to: {}", STREAM.framerate());
        assert_eq!(STREAM.framerate(), 30);

        STREAM.update().width(1280)?;
        println!("Updated width to: {}", STREAM.width());
        assert_eq!(STREAM.width(), 1280);

        STREAM.update().height(720)?;
        println!("Updated height to: {}", STREAM.height());
        assert_eq!(STREAM.height(), 720);

        // Test batch update
        STREAM.update().batch(|stream| {
            stream.stream.settings.width = 1920;
            stream.stream.settings.height = 1080;
            stream.stream.settings.framerate = 60;
            stream.stream.enabled = true;
        })?;

        println!(
            "Batch updated - Width: {}, Height: {}, Framerate: {}, Enabled: {}",
            STREAM.width(),
            STREAM.height(),
            STREAM.framerate(),
            STREAM.enabled()
        );

        assert_eq!(STREAM.width(), 1920);
        assert_eq!(STREAM.height(), 1080);
        assert_eq!(STREAM.framerate(), 60);
        assert!(STREAM.enabled());

        Ok(())
    }
}
