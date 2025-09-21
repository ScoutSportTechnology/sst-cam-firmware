from .sensor_format import ISensorFormat
from .sensor_mode import ISensorMode


class ISensor:
	def __init__(self, format: ISensorFormat, mode: ISensorMode) -> None:
		self._format = format
		self._mode = mode

	@property
	def mode(self) -> ISensorMode:
		return self._mode

	@property
	def format(self) -> ISensorFormat:
		return self._format
