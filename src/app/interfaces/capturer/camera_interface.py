from abc import ABC, abstractmethod

from app.models.capturer.frame_model import Frame


class ICamera(ABC):
	@abstractmethod
	def __init__(self, camera_index: int) -> None: ...

	@abstractmethod
	def init(self) -> None: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def frame(self) -> Frame: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def restart(self) -> None: ...

	@abstractmethod
	def status(self) -> bool: ...

	@abstractmethod
	def focus(self) -> None: ...

	@abstractmethod
	def _capture(self) -> None: ...
