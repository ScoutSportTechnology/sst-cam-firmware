from enum import Enum


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
