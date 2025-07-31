from collections.abc import Generator
from typing import Any, final

import cv2
import ffmpeg

from app.interfaces.stream import IStream
from app.models import Frame
from app.models.stream import EncodeType, StreamProvider
from app.services.video import VideoService
from config.settings import Settings


class StreamService(IStream):
	def __init__(self, video_service: VideoService, provider: StreamProvider):
		self.active = False
		self.video_service = video_service
		self.settings = Settings
		self._provider = provider

	@property
	def provider(self) -> StreamProvider:
		return self._provider

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

	def focus(self) -> None:
		self.video_service.focus()

	def feed(
		self,
		format: EncodeType | None = None,
	) -> Generator[bytes, Any, None]:
		frame_gen: Generator[Frame, None, None] = self.video_service.frames()

		if format is not None:
			if self.provider == StreamProvider.HTTP:
				for frame in frame_gen:
					if not self.active:
						break
					success, buffer = cv2.imencode(
						f'.{format}', frame.data, (int(cv2.IMWRITE_JPEG_QUALITY), 100)
					)
					if not success:
						continue
					bytes = buffer.tobytes()

					yield bytes
		elif self.provider == StreamProvider.RTMP:
			for frame in frame_gen:
				if not self.active:
					break
				yield frame.data.astype('uint8').tobytes()
		else:
			for frame in frame_gen:
				if not self.active:
					break
				yield frame.data.tobytes()
