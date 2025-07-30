import time

from numpy import uint8
from numpy.typing import NDArray

from app.interfaces.camera import ICamera
from app.models import Frame
from config.settings import settings


class Picamera2Adapter(ICamera):
	def __init__(self, camera_index: int) -> None:
		self.active = False
		self.camera_index = camera_index

	def init_camera(self) -> None:
		from picamera2 import Picamera2

		# Always try to stop any existing camera instance
		if hasattr(self, 'picam') and self.picam:
			try:
				self.stop()
			except Exception as e:
				print(f'Warning: Failed to stop/close previous camera: {e}')

		# Now safely create and configure new camera
		self.picam = Picamera2(camera_num=self.camera_index)
		self.picam.set_logging(Picamera2.ERROR)
		self.video_config = self.picam.create_video_configuration(
			main={'size': settings.camera.resolution, 'format': settings.camera.format},
			controls={'FrameRate': settings.camera.frame_rate},
		)
		self.picam.configure(self.video_config)
		self.picam.set_controls({'AfMode': 'Auto'})
		self.focus()
		self.active = False
		print(self.picam.camera_properties_)

	def start(self) -> None:
		if not hasattr(self, 'picam') or self.picam is None:
			self.init_camera()

		if not self.active:
			try:
				self.picam.start()
				self.active = True
			except Exception as e:
				print(f'Failed to start camera {self.camera_index}: {e}')
				self.active = False

	def stop(self) -> None:
		if hasattr(self, 'picam') and self.picam:
			try:
				self.picam.stop()
			except Exception as e:
				print(f'Warning: error stopping camera {self.camera_index}: {e}')

			try:
				self.picam.close()
			except Exception as e:
				print(f'Warning: error closing camera {self.camera_index}: {e}')
			self.active = False

	def status(self) -> str:
		return 'active' if self.active else 'inactive'

	def focus(self) -> None:
		#self.picam.set_controls({'AfTrigger': 0})
		pass

	def get_frame(self) -> Frame:
		data: NDArray[uint8] = self.picam.capture_array()
		return Frame(data=data, timestamp=time.time())

	def restart(self) -> None:
		self.stop()
		self.start()
