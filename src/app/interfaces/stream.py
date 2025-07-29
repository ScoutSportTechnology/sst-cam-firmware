from abc import ABC, abstractmethod
from collections.abc import Generator
from typing import Any


class IStream(ABC):
	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def get_status(self) -> str: ...

	@abstractmethod
	def feed(self) -> Generator[bytes, Any, None]: ...
