import time

from numpy import uint8
from numpy.typing import NDArray

from app.interfaces.camera import ICamera
from app.models import Frame
from app.services.logger import Logger
from config.settings import settings


class Picamera2Adapter(ICamera):
	def __init__(self, camera_index: int) -> None:
		self.active = False
		self.camera_index = camera_index
		self.logger = Logger(name='picamera2_adapter')

	def init_camera(self) -> None:
		from picamera2 import Picamera2

		self.picam = Picamera2(camera_num=self.camera_index)
		self.picam.set_logging(Picamera2.ERROR)
		self.video_config = self.picam.create_video_configuration(
			main={'size': settings.camera.resolution, 'format': settings.camera.format},
			controls={'FrameRate': settings.camera.frame_rate},
		)
		self.focus()
		self.picam.configure(self.video_config)
		self.active = False
		# print(self.picam.camera_properties_)

	def start(self) -> None:
		if not self.active:
			try:
				self.init_camera()
				self.picam.start()
				self.active = True
			except Exception as e:
				self.logger.error(f'Failed to start camera {self.camera_index}: {e}')

	def stop(self) -> None:
		if self.active:
			try:
				self.picam.stop()
				self.picam.close()
				self.active = False
			except Exception as e:
				self.logger.error(f'Error stopping/closing camera {self.camera_index}: {e}')

	def status(self) -> str:
		return 'active' if self.active else 'inactive'

	def focus(self) -> None:
		self.picam.set_controls({'AfMode': 2})
		# self.picam.set_controls({'AfTrigger': 0})
		# print(self.picam.camera_controls)

	def get_frame(self) -> Frame:
		data: NDArray[uint8] = self.picam.capture_array()
		#self.logger.debug(f'Captured frame from camera {self.camera_index}: {data.shape}')
		return Frame(data=data, timestamp=time.time())

	def restart(self) -> None:
		self.stop()
		self.start()
