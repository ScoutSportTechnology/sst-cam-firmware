from abc import ABC, abstractmethod

from app.models.capturer import FPS, HDR, CaptureFormat, CaptureMode, Resolution


class ISensor(ABC):
	def __init__(self, format: CaptureFormat, mode: CaptureMode) -> None:
		self.__format = format
		self.__mode = mode

	@property
	@abstractmethod
	def resolution(self) -> Resolution: ...

	@property
	@abstractmethod
	def fps(self) -> FPS: ...

	@property
	@abstractmethod
	def hdr(self) -> HDR: ...

	@property
	@abstractmethod
	def format(self) -> CaptureFormat: ...
