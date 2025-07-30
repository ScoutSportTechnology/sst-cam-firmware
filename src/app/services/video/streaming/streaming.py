from collections.abc import Generator
from typing import Any

import cv2

from app.interfaces.stream import IStream
from app.models import Frame
from app.services.video.video import VideoService


class StreamService(IStream):
	def __init__(self, video_service: VideoService):
		self.active = False
		self.video_service = video_service

	def start(self) -> None:
		if not self.active:
			self.video_service.start()
			self.active = True

	def stop(self):
		if self.active:
			self.video_service.stop()
			self.active = False

	def get_status(self) -> str:
		return 'active' if self.active else 'inactive'

	def feed(
		self,
		image_format: str,
	) -> Generator[bytes, Any, None]:
		frame_gen: Generator[Frame, None, None] = self.video_service.frames()
		for frame in frame_gen:
			if not self.active:
				break

			success, buffer = cv2.imencode(
				image_format, frame.data, (int(cv2.IMWRITE_JPEG_QUALITY), 100)
			)
			if not success:
				continue
			bytes = buffer.tobytes()

			yield bytes
