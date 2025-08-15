from abc import ABC, abstractmethod
from collections.abc import Generator

from app.models.capturer.frame import Frame


class IBufferService(ABC):
	@abstractmethod
	def buffer(self, feed: Generator[Frame, None, None]) -> Generator[Frame, None, None]: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...
