from enum import Enum
from typing import Generic, TypeVar


class ISensorMode(Enum):
	_value_: tuple[tuple[int, int], int, bool]

	@property
	def resolution(self) -> tuple[int, int]:
		return self.value[0]

	@property
	def fps(self) -> int:
		return self.value[1]

	@property
	def hdr(self) -> bool:
		return self.value[2]


class ISensorFormat(Enum):
	_value_: tuple[str, str]

	@property
	def sensor_format(self) -> str:
		return self.value[0]

	@property
	def ffmpeg_format(self) -> str:
		return self.value[1]


TMode = TypeVar('TMode', bound=ISensorMode)
TFormat = TypeVar('TFormat', bound=ISensorFormat)


class ISensor(Generic[TMode, TFormat]):
	def __init__(self, format: TFormat, mode: TMode) -> None:
		self._format = format
		self._mode = mode

	@property
	def mode(self) -> ISensorMode:
		return self._mode

	@property
	def format(self) -> ISensorFormat:
		return self._format
