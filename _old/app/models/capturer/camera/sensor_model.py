from app.interfaces.capturer.camera import ISensor
from app.models.capturer import FPS, HDR, CaptureFormat, Resolution


class Sensor(ISensor):
	@property
	def resolution(self) -> Resolution:
		return self._mode[0]

	@property
	def fps(self) -> FPS:
		return self._mode[1]

	@property
	def hdr(self) -> HDR:
		return self._mode[2]

	@property
	def format(self) -> CaptureFormat:
		return self._format
