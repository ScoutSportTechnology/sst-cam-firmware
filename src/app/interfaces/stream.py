from abc import ABC, abstractmethod
from collections.abc import Generator
from typing import Any

from app.models import Frame


class IStream(ABC):
	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def get_status(self) -> str: ...

	@abstractmethod
	def feed(
		self,
		image_format: str,
	) -> Generator[bytes, Any, None]: ...
