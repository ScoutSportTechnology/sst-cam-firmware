import cv2

from app.models import Frame
from app.models.frame import Frame


class StreamService():
	def __init__(
		self,
		image_format: str,
		frame: Frame,
	):
		self.image_format = image_format
		self.frame = frame
		self.active = False

	def start(self) -> bytes:
		if self.active:
			self.stop()
		self.active = True
		if not self.active:
			raise RuntimeError('Stream is not active. Please start the stream first.')
		success, buffer = cv2.imencode(self.image_format, self.frame.data)
		if not success:
			raise RuntimeError('Failed to encode frame data.')
		return buffer.tobytes()

	def stop(self):
		self.active = False

	def get_status(self) -> str:
		return 'active' if self.active else 'inactive'