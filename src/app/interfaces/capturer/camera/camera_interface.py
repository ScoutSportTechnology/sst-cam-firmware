import queue
import threading
import time
from abc import ABC, abstractmethod
from typing import Final

import cv2 as cv

from app.infra.logger import Logger
from app.models.capturer import CaptureFormat, Frame
from config.settings import Settings

from .lense_interface import ILense
from .sensor_interface import ISensor


class ICamera(ABC):
	def __init__(
		self,
		*,
		camera_index: int,
		flip_method: int,
		logger: Logger,
		sensor: ISensor,
		lense: ILense,
		timeout: float = 0.5,
	) -> None:
		self._active: bool = False
		self._camera_index: int = camera_index
		self._logger: Final[Logger] = logger
		self._sensor: Final[ISensor] = sensor
		self._lense: Final[ILense] = lense
		self._settings: Final[Settings] = Settings()
		self._flip_method: int = flip_method

		self._framerate: int = self._sensor.fps
		self._capture_width: int = self._sensor.resolution[0]
		self._capture_height: int = self._sensor.resolution[1]
		self._format: CaptureFormat = self._sensor.format

		self._timeout: float = timeout
		self._frame_queue: queue.Queue[Frame] = queue.Queue(maxsize=1)
		self._thread: threading.Thread | None = None
		self._capturer = cv.VideoCapture(self._pipeline(), cv.CAP_GSTREAMER)
		self._logger.info(f'Camera {self._camera_index} initialized')

	def start(self) -> None:
		if not self._active:
			self._active = True
			self._thread = threading.Thread(target=self._capture, daemon=True)
			self._thread.start()
			self._logger.info(f'Camera {self._camera_index} started')

	def stop(self) -> None:
		self._active = False
		if self._capturer.isOpened():
			self._capturer.release()
		if self._thread and self._thread.is_alive():
			self._thread.join()
		self._logger.info(f'Camera {self._camera_index} stopped')

	def restart(self) -> None:
		self.stop()
		self.start()
		self._logger.info(f'Camera {self._camera_index} restarted')

	def status(self) -> bool:
		return self._active

	@abstractmethod
	def focus(self) -> None: ...

	def capture(self) -> Frame:
		return self._frame_queue.get(timeout=self._timeout)

	def _capture(self) -> None:
		while self._active:
			retval, data = self._capturer.read()
			if not retval:
				self._logger.error(f'Failed to capture frame from camera {self._camera_index}')
				time.sleep(1 / self._framerate)
				continue

			frame = Frame(data=data, timestamp=time.time())

			try:
				self._frame_queue.put(frame, timeout=self._timeout)
			except queue.Full:
				try:
					_ = self._frame_queue.get_nowait()
					self._frame_queue.put(frame, timeout=self._timeout)
				except queue.Empty:
					pass
				self._logger.warning(f'Frame queue is empty for camera {self._camera_index}')

	@abstractmethod
	def _pipeline(self) -> str: ...
