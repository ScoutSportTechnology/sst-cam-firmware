from enum import Enum


class ISensorFormat(Enum):
	_value_: tuple[str, str]

	@property
	def sensor(self) -> str:
		return self.value[0]

	@property
	def ffmpeg(self) -> str:
		return self.value[1]