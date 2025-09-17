from dataclasses import dataclass, field
from math import radians, tan

from config.camera import CaptureFormat, Sensors


@dataclass(frozen=True)
class CameraSettings:
	fov: int = 120
	mode: CaptureMode = Sensors.IMX477.NATIVE_2304x1296_56
	format: CaptureFormat = CaptureFormat.BGR888
	sensor_resolution: tuple[int, int] = (4608, 2592)
	pixel_size: tuple[float, float] = (1.4, 1.4)
	shutter_type: str = 'Rolling'
	color_filter: str = 'Quad-Bayer Coded'

	@property
	def sensor_format(self) -> str:
		return self.format.sensor_format

	@property
	def ffmpeg_format(self) -> str:
		return self.format.ffmpeg_format

	@property
	def resolution(self) -> tuple[int, int]:
		return self.mode.resolution

	@property
	def fps(self) -> int:
		return self.mode.fps

	@property
	def hdr(self) -> bool:
		return self.mode.hdr

	@property
	def supported_modes(self) -> dict[str, tuple[tuple[int, int], int, bool]]:
		return {mode.name: (mode.resolution, mode.fps, mode.hdr) for mode in CaptureMode}


@dataclass(frozen=True)
class StreamSettings:
	resolution: tuple[int, int] = (1920, 1080)  # (3840, 1080)
	buffer_seconds: int = 2

	@property
	def fps(self) -> int:
		return 30

	@property
	def buffer_size(self) -> int:
		return self.buffer_seconds * self.fps


@dataclass(frozen=True)
class FOVSettings:
	focal_distance_c: float = 75
	focal_angle_acb: float = 60
	camera_settings: CameraSettings = field(default_factory=CameraSettings)

	@property
	def focal_distance_ab(self) -> float:
		# lens triangle calculation
		return 2 * tan(radians(self.focal_angle_acb / 2)) * self.focal_distance_c

	def focal_pixel_overlap(self) -> int:
		# pixel-based overlap between cameras
		return int(self.camera_settings.resolution[0] * tan(radians(self.focal_angle_acb / 2)) / tan(radians(self.camera_settings.fov / 2)))


class Settings:
	"""
	Global settings container with class-level access:
	- Settings.camera
	- Settings.stream
	- Settings.fov
	"""

	# class variables initialized once
	camera: CameraSettings = CameraSettings()
	stream: StreamSettings = StreamSettings()
	fov: FOVSettings = FOVSettings(camera_settings=camera)


# Optionally, expose a singleton if you prefer instance-based use:
settings = Settings()
