from dataclasses import dataclass, field
from math import radians, tan

from config.camera import CameraSettings


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
		return 2 * tan(radians(self.focal_angle_acb / 2)) * self.focal_distance_c

	def focal_pixel_overlap(self) -> int:
		return int(self.camera_settings.resolution[0] * tan(radians(self.focal_angle_acb / 2)) / tan(radians(self.camera_settings.fov / 2)))


class Settings:
	camera: CameraSettings = CameraSettings()
	stream: StreamSettings = StreamSettings()
	fov: FOVSettings = FOVSettings(camera_settings=camera)
