import queue
import threading
import time

import cv2
from numpy import uint8
from numpy.typing import NDArray

from app.infra.logger import Logger
from app.interfaces.capturer import ICamera
from app.models.capturer import Frame
from config.settings import Settings


class GStreamerAdapter(ICamera):
	def __init__(self, camera_index: int) -> None:
		self.active = False
		self.camera_index = camera_index
		self.logger = Logger(name='gstreamer_adapter')
		self._frame_queue = queue.Queue(maxsize=1)
		self._thread: threading.Thread | None = None
		self.timeout = 0.5  # seconds
		self.settings = Settings()

	def init(self) -> None:
		_flip_method = 0
		_framerate = self.settings.camera.fps
		_capture_width = self.settings.camera.resolution[0]
		_capture_height = self.settings.camera.resolution[1]
		_format = self.settings.camera.format.sensor
		_pipeline = (
			f'nvarguscamerasrc sensor-id={self.camera_index} ! '
			f'video/x-raw(memory:NVMM), width=(int){_capture_width}, height=(int){_capture_height}, framerate=(fraction){_framerate}/1 ! '
			f'nvvidconv flip-method={_flip_method} ! '
			f'video/x-raw, format=(string){_format} ! '
			'appsink drop=true max-buffers=1'
		)
		self.cap = cv2.VideoCapture(_pipeline, cv2.CAP_GSTREAMER)
		if not self.cap.isOpened():
			raise RuntimeError(f'Failed to open GStreamer pipeline for camera {self.camera_index}')

	def start(self) -> None:
		if not self.active:
			try:
				self.init()
				self.active = True
				self._thread = threading.Thread(target=self._capture, daemon=True)
				self._thread.start()
			except Exception as e:
				self.logger.error(f'Failed to start camera {self.camera_index}: {e}')

	def stop(self) -> None:
		if self.active:
			self.active = False
			try:
				if hasattr(self, 'cap') and self.cap:
					self.cap.release()
				if self._thread:
					self._thread.join()
			except Exception as e:
				self.logger.error(f'Error stopping/closing camera {self.camera_index}: {e}')

	def status(self) -> bool:
		return self.active

	def focus(self) -> None:
		self.logger.info('Focus method not implemented for GStreamerAdapter')

	def frame(self) -> Frame:
		frame = self._frame_queue.get(timeout=self.timeout)
		return frame

	def restart(self) -> None:
		self.stop()
		self.start()

	def _capture(self) -> None:
		if not hasattr(self, 'cap') or not self.cap:
			self.logger.error('Camera not initialized')
			return

		while self.active:
			t0 = time.time()
			retval, data = self.cap.read()

			if not retval:
				self.logger.error('Failed to capture frame')
				continue

			frame = Frame(data=data, timestamp=t0)

			try:
				self._frame_queue.put(frame, timeout=self.timeout)
			except queue.Full:
				_ = self._frame_queue.get_nowait()
				self._frame_queue.put(frame, timeout=self.timeout)