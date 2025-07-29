from collections.abc import Generator
from typing import Any

import cv2

from app.interfaces.stream import IStream
from app.models import Frame
from app.models.frame import Frame


class StreamService(IStream):
	def __init__(
		self,
		image_format: str,
		frame: Frame,
	):
		self.image_format = image_format
		self.frame = frame
		self.active = False

	def start(self) -> None:
		if self.active:
			self.stop()
		self.active = True

	def stop(self):
		self.active = False

	def get_status(self) -> str:
		return 'active' if self.active else 'inactive'

	def feed(self) -> Generator[bytes, Any, None]:
		while self.active:
			success, buffer = cv2.imencode(self.image_format, self.frame.data)
			if not success:
				continue
			yield buffer.tobytes()
