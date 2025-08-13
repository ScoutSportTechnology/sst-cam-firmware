from abc import ABC, abstractmethod
from collections.abc import Generator

from app.models.capturer.frame import Frame

from .camera import ICamera


class IVideoService(ABC):
	@abstractmethod
	def __init__(self, cam0: ICamera, cam1: ICamera) -> None: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def status(self) -> bool: ...

	@abstractmethod
	def _focus(self) -> None: ...

	@abstractmethod
	def _get_frames(self) -> None: ...

	@abstractmethod
	def _preprocess(self) -> list[Frame]: ...

	@abstractmethod
	def _postprocess(self) -> Frame: ...

	@abstractmethod
	def _transform(self) -> None: ...

	@abstractmethod
	def _track(self) -> None: ...

	@abstractmethod
	def _frames() -> Generator[Frame, None, None]: ...

	@abstractmethod
	def feed(self) -> Generator[Frame, None, None]: ...
