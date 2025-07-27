from abc import ABC, abstractmethod

from app.models.frame import Frame


class ICamera(ABC):
	@abstractmethod
	def __init__(self, camera_index: int) -> None: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def get_frame(self) -> Frame: ...

	@abstractmethod
	def stop(self) -> None: ...
