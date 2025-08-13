from collections.abc import Generator
from typing import Any

import cv2

from app.interfaces.capturer import IVideoService
from app.interfaces.streamer import IStreamProvider, IStreamService
from app.models.capturer import Frame
from app.models.streamer import StreamProtocol
from app.services.logger import Logger
from app.services.streamer.stream_provider import StreamProviderService
from app.services.video import VideoService
from config.settings import Settings


class StreamService(IStreamService):
	def __init__(self, video_service: IVideoService) -> None:
		self.active = False
		self.video_service = video_service
		self.settings = Settings
		self.logger = Logger(name='stream_service')

	def start(self) -> None:
		if not self.active:
			self.video_service.start()
			self.active = True

	def stop(self) -> None:
		if self.active:
			self.video_service.stop()
			self.stream_provider_service.stop()
			self.active = False

	def status(self) -> bool:
		return self.active

	def focus(self) -> None:
		if self.active:
			self.video_service.focus()
		else:
			self.logger.warning('Cannot adjust focus, stream is not active')

	def stream(
		self,
		stream_protocol: StreamProtocol,
		url: str,
	) -> None:
		self.logger.debug(f'Starting feed for provider: {stream_protocol.value}')
		if self.active and self.video_service.status() == 'active':
			feed: Generator[Frame, None, None] = self.video_service.frames()
			self.stream_provider_service: IStreamProvider = StreamProviderService(
				stream_protocol, self.active
			)
			self.stream_provider_service.start(feed, url)
