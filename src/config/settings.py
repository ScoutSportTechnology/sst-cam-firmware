from dataclasses import dataclass, field
from math import radians, tan


@dataclass(frozen=True)
class CameraSettings:
	"""
	Camera parameters: field of view, resolution, frame rate, and pixel format.
	Access via Settings.camera
	"""

	fov: int = 120
	resolution: tuple[int, int] = (1920, 1080)
	frame_rate: int = 30
	format: str = 'RGB888'


@dataclass(frozen=True)
class StreamSettings:
	"""
	Streaming parameters: URL, key, bitrate, and preset.
	Access via Settings.stream
	"""

	resolution: tuple[int, int] = (3840, 1080)
	fps: int = 60
	thread_queue_size: int = 1
	realtime: bool = True
	fflags: str = 'nobuffer'
	vcodec: str = 'h264_v4l2m2m'
	format: str = 'flv'
	preset: str = 'ultrafast'
	tune: str = 'zerolatency'
	threads: int = 4
	gop: int = 60
	pixel_format: str = 'yuv420p'
	bitrate: int = 4000
	buffer_seconds: int = 3
	@property
	def buffer_size(self) -> int:
		return self.buffer_seconds * self.fps


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
