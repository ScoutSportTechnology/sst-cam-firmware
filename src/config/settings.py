from dataclasses import dataclass, field
from enum import Enum
from math import ceil, radians, tan


class CaptureMode(Enum):
	"""
	Predefined capture modes mapping to resolution, fps, and HDR flag.
	"""

	NATIVE_2304x1296_56 = ((2304, 1296), 56, False)
	NATIVE_2304x1296_30 = ((2304, 1296), 30, False)
	HDR_1536x864_120 = ((1536, 864), 120, True)

	@property
	def resolution(self) -> tuple[int, int]:
		return self.value[0]

	@property
	def fps(self) -> int:
		return self.value[1]

	@property
	def hdr(self) -> bool:
		return self.value[2]


@dataclass(frozen=True)
class CameraSettings:
	"""
	Camera parameters: field of view, capture mode, and pixel format.
	Mode drives resolution, fps, and HDR automatically.
	Access via Settings.camera
	"""

	fov: int = 120
	mode: CaptureMode = CaptureMode.NATIVE_2304x1296_56
	format: str = 'YUV420'
	pixel_format: str = 'yuv420p'
	sensor_resolution: tuple[int, int] = (4608, 2592)
	pixel_size: tuple[float, float] = (1.4, 1.4)
	shutter_type: str = 'Rolling'
	color_filter: str = 'Quad-Bayer Coded'

	@property
	def resolution(self) -> tuple[int, int]:
		return self.mode.resolution

	@property
	def fps(self) -> int:
		return self.mode.fps

	@property
	def hdr(self) -> bool:
		return self.mode.hdr

	# All supported modes for UI or validation
	@property
	def supported_modes(self) -> dict[str, tuple[tuple[int, int], int, bool]]:
		return {mode.name: (mode.resolution, mode.fps, mode.hdr) for mode in CaptureMode}


@dataclass(frozen=True)
class StreamSettings:
	"""
	Streaming parameters: URL, key, bitrate, and preset.
	Access via Settings.stream
	"""

	resolution: tuple[int, int] = (1920, 1080)
	fps: int = 60
	vcodec: str = 'libx264'
	buffer_seconds: int = 5
	buffer_size = buffer_seconds * fps


@dataclass(frozen=True)
class FOVSettings:
	"""
	Field-of-view related calcs. Use Settings.fov.
	"""

	focal_distance_c: float = 75
	focal_angle_acb: float = 60
	camera_settings: CameraSettings = field(default_factory=CameraSettings)

	@property
	def focal_distance_ab(self) -> float:
		# lens triangle calculation
		return 2 * tan(radians(self.focal_angle_acb / 2)) * self.focal_distance_c

	def focal_pixel_overlap(self) -> int:
		# pixel-based overlap between cameras
		return int(
			self.camera_settings.resolution[0]
			* tan(radians(self.focal_angle_acb / 2))
			/ tan(radians(self.camera_settings.fov / 2))
		)


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
