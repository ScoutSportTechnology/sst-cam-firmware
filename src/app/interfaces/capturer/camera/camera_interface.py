import queue
import threading
from abc import ABC, abstractmethod

from app.infra.logger import Logger
from app.models.capturer.frame_model import Frame

from .lense_interface import ILense
from .sensor_interface import ISensor


class ICamera(ILense, ISensor, ABC):
	@abstractmethod
	def __init__(self, camera_index: int) -> None:
		self.__active = False
		self.__camera_index = camera_index
		self.__frame_queue = queue.Queue(maxsize=1)
		self.__thread: threading.Thread | None = None
		self.__timeout = 0.5
		self.__logger: Logger
		...

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
