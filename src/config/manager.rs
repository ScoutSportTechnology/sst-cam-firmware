use crate::domains::common::logger::Logger;
use serde::Deserialize;
use std::fs;
use std::path::Path;
use std::fmt;

static LOGGER: Logger = Logger::new(Some("Configuration Manager"));

#[derive(Debug)]
enum ConfigFiles {
		Calibration,
		Device,
		Storage,
		Stream
}

impl ConfigFiles {
	fn to_filename(&self) -> &str {
		match self {
			ConfigFiles::Calibration => "calibration.toml",
			ConfigFiles::Device => "device.toml",
			ConfigFiles::Storage => "storage.toml",
			ConfigFiles::Stream => "stream.toml",
		}
	}
}

#[derive(Debug)]
enum Profiles {
	Default,
	User,
}

impl Profiles {
	fn to_foldername(&self) -> &Path {
		match self {
			Profiles::Default => Path::new("default"),
			Profiles::User => "user",
		}
	}
}
