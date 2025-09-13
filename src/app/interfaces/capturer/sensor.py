from abc import ABC, abstractmethod
from enum import Enum


class Isensor(ABC, Enum):
	@abstractmethod
	@property
	def resolution(self) -> tuple[int, int]:
		return self.value[0]
		
	@abstractmethod
	@property
	def fps(self) -> int:
		return self.value[1]

	@abstractmethod
	@property
	def hdr(self) -> bool:
		return self.value[2]
