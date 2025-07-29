import time

from numpy import uint8
from numpy.typing import NDArray

from app.interfaces.camera import ICamera
from app.models import Frame
from config.settings import settings


class Picamera2Adapter(ICamera):
	def __init__(self, camera_index: int) -> None:
		self.active = False
		from picamera2 import Picamera2

		self.picam = Picamera2(camera_num=camera_index)
		video_config = self.picam.create_video_configuration(
			main={'size': settings.camera.resolution, 'format': settings.camera.format},
			controls={'FrameRate': settings.camera.frame_rate},
		)
		self.picam.configure(video_config)
		self.picam.set_logging(Picamera2.ERROR)
		print(self.picam.camera_properties_)

	def start(self) -> None:
		if self.active:
			self.stop()
		self.picam.start()
		self.active = True
		
	def status(self) -> str:
		return 'active' if self.active else 'inactive'

	def get_frame(self) -> Frame:
		data: NDArray[uint8] = self.picam.capture_array()
		return Frame(data=data, timestamp=time.time())

	def stop(self) -> None:
		self.picam.stop()
		self.picam.close()
		self.active = False
		
	def restart(self) -> None:
		self.stop()
		self.start()

	