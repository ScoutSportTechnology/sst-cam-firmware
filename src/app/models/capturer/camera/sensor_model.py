from app.interfaces.capturer.camera import ISensor
from app.models.capturer import FPS, HDR, CaptureFormat, CaptureMode, Resolution


class Sensor(ISensor):
	@property
	def resolution(self) -> Resolution:
		return self.__mode[0]

	@property
	def fps(self) -> FPS:
		return self.__mode[1]

	@property
	def hdr(self) -> HDR:
		return self.__mode[2]

	@property
	def format(self) -> CaptureFormat:
		return self.__format
