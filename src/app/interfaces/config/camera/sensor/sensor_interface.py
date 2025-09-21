from .sensor_format_interface import ISensorFormat
from .sensor_mode_interface import ISensorMode


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
