from collections.abc import Generator
from typing import Any, final

import cv2
import ffmpeg

from app.interfaces.stream import IStream
from app.models import Frame
from app.models.stream import EncodeType, StreamProvider
from app.services.logger import Logger
from app.services.video import VideoService
from config.settings import Settings


class StreamService(IStream):
	def __init__(self, video_service: VideoService, provider: StreamProvider):
		self.active = False
		self.video_service = video_service
		self.settings = Settings
		self._provider = provider
		self.logger = Logger(name='stream_service')

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
	) -> Generator[bytes | Frame, Any, None]:
		frame_gen: Generator[Frame, None, None] = self.video_service.frames()

		if format is not None:
			self.logger.debug(f'Processing frames with format: {format}')
			if self.provider == StreamProvider.HTTP:
				for frame in frame_gen:
					if not self.active:
						self.logger.error('Stream is inactive, breaking feed loop.')
						break
					self.logger.debug(f'Processing frame for HTTP with format: {format}')
					success, buffer = cv2.imencode(
						f'.{format}', frame.data, (int(cv2.IMWRITE_JPEG_QUALITY), 100)
					)
					if not success:
						self.logger.error('Failed to encode frame, skipping...')
						continue
					bytes = buffer.tobytes()

					yield bytes
		elif self.provider == StreamProvider.RTMP:
			for frame in frame_gen:
				self.logger.debug('Processing frame for RTMP')
				if not self.active:
					self.logger.error('Stream is inactive, breaking feed loop.')
					break
				yield frame
		else:
			for frame in frame_gen:
				self.logger.debug('Processing frame to bytes')
				if not self.active:
					self.logger.error('Stream is inactive, breaking feed loop.')
					break
				yield frame.data.tobytes()
