from collections.abc import Generator
from typing import Any

import cv2

from app.interfaces.stream import IStream
from app.models import Frame
from app.models.stream import EncodeType, StreamProvider
from app.services.api.stream import StreamProviderService
from app.services.logger import Logger
from app.services.video import VideoService
from config.settings import Settings


class StreamService(IStream):
	def __init__(self, video_service: VideoService) -> None:
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

	def get_status(self) -> str:
		return 'active' if self.active else 'inactive'

	def focus(self) -> None:
		if self.active:
			self.video_service.focus()
		else:
			self.logger.warning('Cannot adjust focus, stream is not active')

	def feed(
		self,
		stream_provider: StreamProvider,
		url: str,
	) -> None:
		self.provider = stream_provider
		self.logger.debug(f'Starting feed for provider: {self.provider.value}')
		if self.active and self.video_service.status() == 'active':
			feed: Generator[Frame, None, None] = self.video_service.frames()
			self.stream_provider_service = StreamProviderService(self.provider, self.active)
			self.stream_provider_service.start(feed, url)
