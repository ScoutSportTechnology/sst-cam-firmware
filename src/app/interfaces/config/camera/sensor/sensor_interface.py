from abc import ABC, abstractmethod


class ISensor(ABC):
	def __init__(self, format: tuple[str, str], mode: tuple[tuple[int, int], int, bool]) -> None:
		self.__format = format
		self.__mode = mode

	@property
	@abstractmethod
	def resolution(self) -> tuple[int, int]: ...

	@property
	@abstractmethod
	def fps(self) -> int: ...

	@property
	@abstractmethod
	def hdr(self) -> bool: ...

	@property
	@abstractmethod
	def capture_format(self) -> str: ...

	@property
	@abstractmethod
	def stream_format(self) -> str: ...
