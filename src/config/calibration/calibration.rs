use crate::domains::common::config::{ConfigError, ConfigFilesEnums, ConfigUtils, ProfilesEnum};
use once_cell::sync::Lazy;
use parking_lot::RwLock;
use serde::{Deserialize, Serialize};

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct CalibrationCameraParameters {
    pub last_calibration_date: String,
    pub exposure: u32,
    pub gain: f32,
    pub white_balance: String,
    pub focus: String,
    pub intrinsic_matrix: [f32; 9],
    pub distortion_coefficients: [f32; 5],
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct CalibrationMicrophoneParameters {
    pub last_calibration_date: String,
    pub sensitivity: f32,
    pub noise_reduction: bool,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct CalibrationParameters {
    pub camera: Vec<CalibrationCameraParameters>,
    pub microphone: Vec<CalibrationMicrophoneParameters>,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct CalibrationConfig {
    pub enabled: bool,
    pub last_total_calibration_date: String,
    pub parameters: CalibrationParameters,
}

#[derive(Deserialize, Serialize, Debug, Clone)]
pub struct CalibrationData {
    pub calibration: CalibrationConfig,
}

// Direct access wrapper for Calibration configuration
pub struct CalibrationAccess {
    data: RwLock<CalibrationData>,
}

impl CalibrationAccess {
    fn new() -> Result<Self, ConfigError> {
        let data = Self::load_calibration_config()?;
        Ok(CalibrationAccess {
            data: RwLock::new(data),
        })
    }

    fn load_calibration_config() -> Result<CalibrationData, ConfigError> {
        let profile = ConfigUtils::load_profile_config()?;

        let path = if profile.user.calibration {
            ConfigFilesEnums::Calibration.to_file(&ProfilesEnum::User)
        } else {
            ConfigFilesEnums::Calibration.to_file(&ProfilesEnum::Default)
        };

        let content = ConfigUtils::load_config_file(&path)?;
        toml::from_str(&content)
            .map_err(|e| ConfigError::ParseError(format!("calibration.toml: {}", e)))
    }

    fn save_to_user_profile(&self) -> Result<(), ConfigError> {
        let path = ConfigFilesEnums::Calibration.to_file(&ProfilesEnum::User);
        let data = self.data.read().clone();
        ConfigUtils::save_config_file(&path, &data)?;
        ConfigUtils::update_profile_flag("calibration", true)?;
        Ok(())
    }

    // Direct access properties - what you wanted: Calibration.enabled, etc.
    pub fn enabled(&self) -> bool {
        self.data.read().calibration.enabled
    }

    pub fn last_total_calibration_date(&self) -> String {
        self.data
            .read()
            .calibration
            .last_total_calibration_date
            .clone()
    }

    pub fn camera_count(&self) -> usize {
        self.data.read().calibration.parameters.camera.len()
    }

    pub fn get_camera_exposure(&self, index: usize) -> Option<u32> {
        let data = self.data.read();
        data.calibration
            .parameters
            .camera
            .get(index)
            .map(|cam| cam.exposure)
    }

    pub fn get_camera_gain(&self, index: usize) -> Option<f32> {
        let data = self.data.read();
        data.calibration
            .parameters
            .camera
            .get(index)
            .map(|cam| cam.gain)
    }

    // Update methods - what you wanted: Calibration.update.enabled(true)
    pub fn update(&self) -> CalibrationUpdate {
        CalibrationUpdate { calibration: self }
    }

    pub fn reset_to_default(&self) -> Result<(), ConfigError> {
        let default_path = ConfigFilesEnums::Calibration.to_file(&ProfilesEnum::Default);
        let content = ConfigUtils::load_config_file(&default_path)?;
        let default_data: CalibrationData = toml::from_str(&content)
            .map_err(|e| ConfigError::ParseError(format!("calibration.toml: {}", e)))?;

        {
            let mut data = self.data.write();
            *data = default_data;
        }

        ConfigUtils::update_profile_flag("calibration", false)?;

        // Remove user file if it exists
        let user_path = ConfigFilesEnums::Calibration.to_file(&ProfilesEnum::User);
        let _ = std::fs::remove_file(user_path);

        Ok(())
    }

    // Get full config for complex operations
    pub fn get_full_config(&self) -> CalibrationData {
        self.data.read().clone()
    }
}

// Update interface - what you wanted: Calibration.update.enabled(true)
pub struct CalibrationUpdate<'a> {
    calibration: &'a CalibrationAccess,
}

impl<'a> CalibrationUpdate<'a> {
    pub fn enabled(self, value: bool) -> Result<(), ConfigError> {
        {
            let mut data = self.calibration.data.write();
            data.calibration.enabled = value;
        }
        self.calibration.save_to_user_profile()
    }

    pub fn set_camera_exposure(self, index: usize, exposure: u32) -> Result<(), ConfigError> {
        {
            let mut data = self.calibration.data.write();
            if let Some(camera) = data.calibration.parameters.camera.get_mut(index) {
                camera.exposure = exposure;
            } else {
                return Err(ConfigError::ParseError(format!(
                    "Camera index {} not found",
                    index
                )));
            }
        }
        self.calibration.save_to_user_profile()
    }

    pub fn set_camera_gain(self, index: usize, gain: f32) -> Result<(), ConfigError> {
        {
            let mut data = self.calibration.data.write();
            if let Some(camera) = data.calibration.parameters.camera.get_mut(index) {
                camera.gain = gain;
            } else {
                return Err(ConfigError::ParseError(format!(
                    "Camera index {} not found",
                    index
                )));
            }
        }
        self.calibration.save_to_user_profile()
    }

    // Batch update method for multiple changes
    pub fn batch<F>(self, updater: F) -> Result<(), ConfigError>
    where
        F: FnOnce(&mut CalibrationData),
    {
        {
            let mut data = self.calibration.data.write();
            updater(&mut *data);
        }
        self.calibration.save_to_user_profile()
    }
}

// Global Calibration instance - what you'll use in your app
static CALIBRATION_INTERNAL: Lazy<CalibrationAccess> =
    Lazy::new(|| CalibrationAccess::new().expect("Failed to initialize Calibration configuration"));

pub static CALIBRATION: Lazy<&'static CalibrationAccess> = Lazy::new(|| &CALIBRATION_INTERNAL);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_calibration_data() {
        // Test 1: Get calibration configuration data
        println!("=== Calibration Configuration Data ===");
        println!("Enabled: {}", CALIBRATION.enabled());
        println!("Last Calibration Date: {}", CALIBRATION.last_total_calibration_date());
        println!("Camera Count: {}", CALIBRATION.camera_count());

        // Test camera-specific data
        for i in 0..CALIBRATION.camera_count() {
            if let (Some(exposure), Some(gain)) = (
                CALIBRATION.get_camera_exposure(i),
                CALIBRATION.get_camera_gain(i),
            ) {
                println!("Camera {}: exposure={}, gain={}", i, exposure, gain);
            }
        }

        // Verify we can access the data without panicking
        assert!(CALIBRATION.camera_count() >= 0);
        assert!(!CALIBRATION.last_total_calibration_date().is_empty());
    }

    #[test]
    fn test_update_calibration_data() -> Result<(), ConfigError> {
        // Test 2: Update calibration configuration data
        println!("=== Updating Calibration Configuration ===");

        // Test individual updates
        CALIBRATION.update().enabled(true)?;
        println!("Updated enabled to: {}", CALIBRATION.enabled());
        assert!(CALIBRATION.enabled());

        // Test camera-specific updates (assuming at least one camera exists)
        if CALIBRATION.camera_count() > 0 {
            CALIBRATION.update().set_camera_exposure(0, 1000)?;
            println!("Updated camera 0 exposure to: {:?}", CALIBRATION.get_camera_exposure(0));
            assert_eq!(CALIBRATION.get_camera_exposure(0), Some(1000));

            CALIBRATION.update().set_camera_gain(0, 2.5)?;
            println!("Updated camera 0 gain to: {:?}", CALIBRATION.get_camera_gain(0));
            assert_eq!(CALIBRATION.get_camera_gain(0), Some(2.5));
        }

        // Test batch update
        CALIBRATION.update().batch(|calibration| {
            calibration.calibration.enabled = true;
            calibration.calibration.last_total_calibration_date = "2025-10-03T12:00:00Z".to_string();
        })?;

        println!("Batch updated - Enabled: {}, Last Calibration: {}",
                CALIBRATION.enabled(), CALIBRATION.last_total_calibration_date());

        assert!(CALIBRATION.enabled());
        assert_eq!(CALIBRATION.last_total_calibration_date(), "2025-10-03T12:00:00Z");

        Ok(())
    }
}
