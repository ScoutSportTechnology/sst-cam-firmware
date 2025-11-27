from abc import ABC, abstractmethod
from collections.abc import Generator

from app.interfaces.capturer.camera import ICamera
from app.models.capturer.frame_model import Frame


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
	def focus(self) -> None: ...

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
	def frames(self) -> Generator[Frame, None, None]: ...

	@abstractmethod
	def feed(self) -> Generator[Frame, None, None]: ...
