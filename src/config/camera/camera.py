import os
import platform
from dataclasses import dataclass, field
from math import radians, tan

from app.interfaces.capturer import ISensor, ISensorMode
from config.camera import CaptureFormat, Sensor
from config.device import DeviceSettings


@dataclass(frozen=True)
class CameraSettings:
	fov: int = 120
	sensor_resolution: tuple[int, int] = (4608, 2592)
	pixel_size: tuple[float, float] = (1.4, 1.4)
	shutter_type: str = 'Rolling'
	color_filter: str = 'Quad-Bayer Coded'

	@property
	def sensor(self) -> ISensor:
		return Sensor().sensor

	@property
	def sensor_mode(self) -> ISensorMode:
		match self.sensor:
			case 'IMX477':
				return self.sensor.value.MODE_3840x2160_30
			case 'IMX708':
				return self.sensor.value.MODE_2304x1296_56
			case _:
				raise ValueError(f'Unsupported sensor: {self.sensor.name}')

	@property
	def sensor_format(self) -> str:
		return self.format.sensor_format

	@property
	def ffmpeg_format(self) -> str:
		return self.format.ffmpeg_format

	@property
	def resolution(self) -> tuple[int, int]:
		return self.sensor_mode.resolution

	@property
	def fps(self) -> int:
		return self.sensor_mode.fps

	@property
	def hdr(self) -> bool:
		return self.sensor_mode.hdr
