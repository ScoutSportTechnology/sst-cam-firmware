import logging
import queue
import threading
import time

from numpy import uint8
from numpy.typing import NDArray
from picamera2 import Picamera2

from app.infra.logger import Logger
from app.interfaces.capturer import ICamera
from app.models.capturer import Frame
from config.settings import settings


class Picamera2Adapter(ICamera):
	def __init__(self, camera_index: int) -> None:
		self.active = False
		self.camera_index = camera_index
		self.logger = Logger(name='picamera2_adapter')
		self._frame_q = queue.Queue(maxsize=1)
		self._thread: threading.Thread | None = None
		self._timeout = 0.5  # seconds

	def init(self) -> None:
		self.picam = Picamera2(camera_num=self.camera_index)
		self.picam.set_logging(logging.ERROR)
		self.video_config = self.picam.create_video_configuration(
			main={'size': settings.camera.resolution, 'format': settings.camera.sensor_format},
			controls={'FrameRate': settings.camera.fps},
		)
		self.focus()
		self.picam.configure(self.video_config)
		self.active = False

	def start(self) -> None:
		if not self.active:
			try:
				self.init()
				self.picam.start()
				self.active = True
				self._thread = threading.Thread(target=self.__capture, daemon=True)
				self._thread.start()
			except Exception as e:
				self.logger.error(f'Failed to start camera {self.camera_index}: {e}')

	def stop(self) -> None:
		if self.active:
			self.active = False
			try:
				self.picam.stop()
				self.picam.close()
				if self._thread:
					self._thread.join()
			except Exception as e:
				self.logger.error(f'Error stopping/closing camera {self.camera_index}: {e}')

	def status(self) -> bool:
		return self.active

	def focus(self) -> None:
		self.picam.set_controls({'AfMode': 2})
		self.picam.set_controls({'AfTrigger': 0})

	def frame(self) -> Frame:
		frame = self._frame_q.get(timeout=self._timeout)
		return frame

	def restart(self) -> None:
		self.stop()
		self.start()

	def __capture(self) -> None:
		while self.active:
			t0 = time.time()
			request = self.picam.capture_request()
			data: NDArray[uint8] = request.make_array('main')
			request.release()
			frame = Frame(data=data, timestamp=t0)

			try:
				self._frame_q.put(frame, timeout=self._timeout)
			except queue.Full:
				_ = self._frame_q.get_nowait()
				self._frame_q.put(frame, timeout=self._timeout)
