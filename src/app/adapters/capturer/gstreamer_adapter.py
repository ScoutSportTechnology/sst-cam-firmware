import logging
import queue
import threading
import time

from numpy import uint8
from numpy.typing import NDArray

from app.infra.logger import Logger
from app.interfaces.capturer import ICamera
from app.models.capturer import Frame
from config.settings import settings


class GStreamerAdapter(ICamera):
	def __init__(self, camera_index: int) -> None:
		self.active = False
		self.camera_index = camera_index
		self.logger = Logger(name='gstreamer_adapter')
		self._frame_q = queue.Queue(maxsize=1)
		self._thread: threading.Thread | None = None
		self.timeout = 0.5  # seconds

	def init_camera(self) -> None:
		
	
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
		pass

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
			frame = Frame(data=data, timestamp=t0)

			try:
				self._frame_q.put(frame, timeout=self.timeout)
			except queue.Full:
				_ = self._frame_q.get_nowait()
				self._frame_q.put(frame, timeout=self.timeout)
	
	def _gstreamer_pipeline(
			sensor_id=0,
			capture_width=1920,
			capture_height=1080,
			display_width=960,
			display_height=540,
			framerate=30,
			flip_method=0,
	):
			
			return (
					"nvarguscamerasrc sensor-id=%d ! "
					"video/x-raw(memory:NVMM), width=(int)%d, height=(int)%d, framerate=(fraction)%d/1 ! "
					"nvvidconv flip-method=%d ! "
					"video/x-raw, width=(int)%d, height=(int)%d, format=(string)BGRx ! "
					"videoconvert ! "
					"video/x-raw, format=(string)BGR ! appsink"
					% (
							sensor_id,
							capture_width,
							capture_height,
							framerate,
							flip_method,
							display_width,
							display_height,
					)
			)