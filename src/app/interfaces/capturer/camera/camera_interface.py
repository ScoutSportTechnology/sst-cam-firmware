import queue
import threading
from abc import ABC, abstractmethod

from app.infra.logger import Logger
from app.models.capturer.frame_model import Frame

from .lense_interface import ILense
from .sensor_interface import ISensor


class ICamera(ABC):
	def __init__(self, camera_index: int) -> None:
		self._active: bool = False
		self._camera_index: int = camera_index
		self._frame_queue: queue.Queue[Frame] = queue.Queue(maxsize=1)
		self._thread: threading.Thread | None = None
		self._timeout: float = 0.5
		self._logger: Logger
		self._sensor: ISensor
		self._lense: ILense

	@abstractmethod
	def init(self) -> None: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...

	def restart(self) -> None:
		self.stop()
		self.start()
		self._logger.info(f'Camera {self._camera_index} restarted')

	def status(self) -> bool:
		return self._active

	@abstractmethod
	def focus(self) -> None: ...

	def capture(self) -> Frame:
		frame = self._frame_queue.get(timeout=self._timeout)
		return frame

	@abstractmethod
	def _capture(self) -> None: ...
