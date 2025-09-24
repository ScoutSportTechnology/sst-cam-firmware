from app.interfaces.capturer.camera import ICamera
from app.models.capturer.frame_model import Frame


class Camera(ICamera):
	def __init__(self, camera_index: int) -> None:
		self.camera_index = camera_index

	def init(self) -> None:
		pass

	def start(self) -> None:
		pass

	def stop(self) -> None:
		pass

	def restart(self) -> None:
		pass

	def status(self) -> bool:
		return False

	def focus(self) -> None:
		pass

	def capture(self) -> Frame:
		pass

	def __capture(self) -> None:
		pass
