use crate::domains::common::config::{ConfigError, ConfigFilesEnums, ConfigUtils, ProfilesEnum};
use once_cell::sync::Lazy;
use parking_lot::RwLock;
use serde::{Deserialize, Serialize};

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct StaticIpSettings {
    pub enabled: bool,
    pub ip_address: String,
    pub subnet_mask: String,
    pub gateway: String,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct WifiStationSettings {
    pub wifi_ssid: String,
    pub wifi_password: String,
    pub static_ip: StaticIpSettings,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct WifiAccessPointSettings {
    pub ssid: String,
    pub password: String,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct WifiSettings {
    pub enabled: bool,
    pub wifi_type: String,
    pub station: WifiStationSettings,
    pub access_point: WifiAccessPointSettings,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct EthernetSettings {
    #[serde(rename = "ethernet_enabled")]
    pub enabled: bool,
    pub static_ip: StaticIpSettings,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct BluetoothSettings {
    pub enabled: bool,
    pub name: String,
    pub password: String,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct NetworkSettings {
    #[serde(rename = "wiFi")]
    pub wifi: WifiSettings,
    pub ethernet: EthernetSettings,
    pub bluetooth: BluetoothSettings,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct DeviceConfig {
    pub enabled: bool,
    pub name: String,
    pub model: String,
    pub version: String,
    pub serial_number: String,
    pub manufacturer: String,
    pub timezone: String,
    pub timestamp: String,
    pub network: NetworkSettings,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct DeviceData {
    pub device: DeviceConfig,
}

// Direct access wrapper for Device configuration
pub struct DeviceAccess {
    data: RwLock<DeviceData>,
}

impl DeviceAccess {
    fn new() -> Result<Self, ConfigError> {
        let data = Self::load_device_config()?;
        Ok(DeviceAccess {
            data: RwLock::new(data),
        })
    }

    fn load_device_config() -> Result<DeviceData, ConfigError> {
        let profile = ConfigUtils::load_profile_config()?;

        let path = if profile.user.device {
            ConfigFilesEnums::Device.to_file(&ProfilesEnum::User)
        } else {
            ConfigFilesEnums::Device.to_file(&ProfilesEnum::Default)
        };

        let content = ConfigUtils::load_config_file(&path)?;
        toml::from_str(&content).map_err(|e| ConfigError::ParseError(format!("device.toml: {}", e)))
    }

    fn save_to_user_profile(&self) -> Result<(), ConfigError> {
        let path = ConfigFilesEnums::Device.to_file(&ProfilesEnum::User);
        let data = self.data.read().clone();
        ConfigUtils::save_config_file(&path, &data)?;
        ConfigUtils::update_profile_flag("device", true)?;
        Ok(())
    }

    // Direct access properties - what you wanted: Device.name, Device.enabled, etc.
    pub fn enabled(&self) -> bool {
        self.data.read().device.enabled
    }

    pub fn name(&self) -> String {
        self.data.read().device.name.clone()
    }

    pub fn model(&self) -> String {
        self.data.read().device.model.clone()
    }

    pub fn version(&self) -> String {
        self.data.read().device.version.clone()
    }

    pub fn serial_number(&self) -> String {
        self.data.read().device.serial_number.clone()
    }

    pub fn manufacturer(&self) -> String {
        self.data.read().device.manufacturer.clone()
    }

    pub fn timezone(&self) -> String {
        self.data.read().device.timezone.clone()
    }

    // Update methods - what you wanted: Device.update.name("new-name")
    pub fn update(&self) -> DeviceUpdate {
        DeviceUpdate { device: self }
    }

    pub fn reset_to_default(&self) -> Result<(), ConfigError> {
        let default_path = ConfigFilesEnums::Device.to_file(&ProfilesEnum::Default);
        let content = ConfigUtils::load_config_file(&default_path)?;
        let default_data: DeviceData = toml::from_str(&content)
            .map_err(|e| ConfigError::ParseError(format!("device.toml: {}", e)))?;

        {
            let mut data = self.data.write();
            *data = default_data;
        }

        ConfigUtils::update_profile_flag("device", false)?;

        // Remove user file if it exists
        let user_path = ConfigFilesEnums::Device.to_file(&ProfilesEnum::User);
        let _ = std::fs::remove_file(user_path);

        Ok(())
    }

    // Get full config for complex operations
    pub fn get_full_config(&self) -> DeviceData {
        self.data.read().clone()
    }
}

// Update interface - what you wanted: Device.update.name("camera")
pub struct DeviceUpdate<'a> {
    device: &'a DeviceAccess,
}

impl<'a> DeviceUpdate<'a> {
    pub fn enabled(self, value: bool) -> Result<(), ConfigError> {
        {
            let mut data = self.device.data.write();
            data.device.enabled = value;
        }
        self.device.save_to_user_profile()
    }

    pub fn name<S: Into<String>>(self, value: S) -> Result<(), ConfigError> {
        {
            let mut data = self.device.data.write();
            data.device.name = value.into();
        }
        self.device.save_to_user_profile()
    }

    pub fn model<S: Into<String>>(self, value: S) -> Result<(), ConfigError> {
        {
            let mut data = self.device.data.write();
            data.device.model = value.into();
        }
        self.device.save_to_user_profile()
    }

    pub fn version<S: Into<String>>(self, value: S) -> Result<(), ConfigError> {
        {
            let mut data = self.device.data.write();
            data.device.version = value.into();
        }
        self.device.save_to_user_profile()
    }

    pub fn serial_number<S: Into<String>>(self, value: S) -> Result<(), ConfigError> {
        {
            let mut data = self.device.data.write();
            data.device.serial_number = value.into();
        }
        self.device.save_to_user_profile()
    }

    pub fn timezone<S: Into<String>>(self, value: S) -> Result<(), ConfigError> {
        {
            let mut data = self.device.data.write();
            data.device.timezone = value.into();
        }
        self.device.save_to_user_profile()
    }

    // Batch update method for multiple changes
    pub fn batch<F>(self, updater: F) -> Result<(), ConfigError>
    where
        F: FnOnce(&mut DeviceData),
    {
        {
            let mut data = self.device.data.write();
            updater(&mut *data);
        }
        self.device.save_to_user_profile()
    }
}

// Global Device instance - what you'll use in your app
static DEVICE_INTERNAL: Lazy<DeviceAccess> =
    Lazy::new(|| DeviceAccess::new().expect("Failed to initialize Device configuration"));

pub static DEVICE: Lazy<&'static DeviceAccess> = Lazy::new(|| &DEVICE_INTERNAL);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_device_data() {
        // Test 1: Get device configuration data
        println!("=== Device Configuration Data ===");
        println!("Enabled: {}", DEVICE.enabled());
        println!("Name: {}", DEVICE.name());
        println!("Model: {}", DEVICE.model());
        println!("Version: {}", DEVICE.version());
        println!("Serial Number: {}", DEVICE.serial_number());
        println!("Manufacturer: {}", DEVICE.manufacturer());
        println!("Timezone: {}", DEVICE.timezone());

        // Verify we can access the data without panicking
        assert!(!DEVICE.name().is_empty());
        assert!(!DEVICE.model().is_empty());
    }

    #[test]
    fn test_update_device_data() -> Result<(), ConfigError> {
        // Test 2: Update device configuration data
        println!("=== Updating Device Configuration ===");

        // Test individual updates
        DEVICE.update().name("test-camera")?;
        println!("Updated name to: {}", DEVICE.name());
        assert_eq!(DEVICE.name(), "test-camera");

        DEVICE.update().model("SST-CAM-v2")?;
        println!("Updated model to: {}", DEVICE.model());
        assert_eq!(DEVICE.model(), "SST-CAM-v2");

        DEVICE.update().enabled(true)?;
        println!("Updated enabled to: {}", DEVICE.enabled());
        assert!(DEVICE.enabled());

        // Test batch update
        DEVICE.update().batch(|device| {
            device.device.name = "batch-updated-camera".to_string();
            device.device.version = "2.0.0".to_string();
            device.device.timezone = "UTC".to_string();
        })?;

        println!(
            "Batch updated - Name: {}, Version: {}, Timezone: {}",
            DEVICE.name(),
            DEVICE.version(),
            DEVICE.timezone()
        );

        assert_eq!(DEVICE.name(), "batch-updated-camera");
        assert_eq!(DEVICE.version(), "2.0.0");
        assert_eq!(DEVICE.timezone(), "UTC");

        Ok(())
    }
}
