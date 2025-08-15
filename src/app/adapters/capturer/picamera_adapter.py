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
		self.timeout = 0.5  # seconds

	def init_camera(self) -> None:
		self.picam = Picamera2(camera_num=self.camera_index)
		self.picam.set_logging(logging.ERROR)
		self.video_config = self.picam.create_video_configuration(
			main={'size': settings.camera.resolution, 'format': settings.camera.pc_fmt},
			controls={'FrameRate': settings.camera.fps},
		)
		self.focus()
		self.picam.configure(self.video_config)
		self.active = False
		"""self.logger.debug(f'Initialized Picamera2 camera {self.camera_index}')
		self.logger.debug('Picamera Camera Properties:', self.picam.camera_properties_)
		self.logger.debug('Picamera Options:', self.picam.options)
		self.logger.debug('Picamera Controls:', self.picam.controls)
		self.logger.debug('Picamera Sensor Format:', self.picam.sensor_format)
		self.logger.debug('Picamera Sensor Modes:', self.picam.sensor_modes_)
		self.logger.debug('Picamera Sensor Resolution:', self.picam.sensor_resolution)"""

	def start(self) -> None:
		if not self.active:
			try:
				self.init_camera()
				self.picam.start()
				self.active = True
				self._thread = threading.Thread(target=self._capture_loop, daemon=True)
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
		# self.picam.set_controls({'AfTrigger': 0})
		# print(self.picam.camera_controls)

	def get_frame(self) -> Frame:
		frame = self._frame_q.get(timeout=self.timeout)
		return frame

	def restart(self) -> None:
		self.stop()
		self.start()

	def _capture_loop(self) -> None:
		while self.active:
			t0 = time.time()
			request = self.picam.capture_request()
			data: NDArray[uint8] = request.make_array('main')
			request.release()
			"""elapsed = time.time() - t0
			self.logger.debug(
					f"Camera {self.camera_index} capture {elapsed:.3f}s → {1/elapsed:.1f}fps"
			)"""
			frame = Frame(data=data, timestamp=t0)

			try:
				self._frame_q.put(frame, timeout=self.timeout)
			except queue.Full:
				_ = self._frame_q.get_nowait()
				self._frame_q.put(frame, timeout=self.timeout)
