from abc import ABC, abstractmethod

from app.models.capturer.frame_model import Frame

from .lense_interface import ILense
from .sensor_interface import ISensor


class ICamera(ILense, ISensor, ABC):
	@abstractmethod
	def __init__(self, camera_index: int) -> None: ...

	@abstractmethod
	def init(self) -> None: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def restart(self) -> None: ...

	@abstractmethod
	def status(self) -> bool: ...

	@abstractmethod
	def focus(self) -> None: ...
	
	@abstractmethod
	def capture(self) -> Frame: ...

	@abstractmethod
	def __capture(self) -> None: ...
