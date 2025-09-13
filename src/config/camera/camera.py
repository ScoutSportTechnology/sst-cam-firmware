from dataclasses import dataclass, field
from math import radians, tan

from config.camera import CaptureFormat, Sensors


@dataclass(frozen=True)
class CameraSettings:
	fov: int = 120
	mode = Sensors.IMX477.value.HDR_1536x864_120
	format: CaptureFormat = CaptureFormat.BGR
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
