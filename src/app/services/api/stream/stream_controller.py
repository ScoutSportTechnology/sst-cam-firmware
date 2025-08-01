import threading
from collections.abc import Generator
from typing import Any

from app.interfaces import IStream
from app.interfaces.api.controller import IApiController
from app.models import frame
from app.models.stream import EncodeType, StreamProvider
from app.services.logger import Logger
from app.services.video.stream import StreamProviderService
from config.settings import Settings


class StreamController(IApiController):
	def __init__(self, stream_service: IStream) -> None:
		self.stream_service = stream_service
		self.stream_provider = stream_service.provider
		self.settings = Settings
		self.logger = Logger(name='stream_controller')

	def start(self) -> dict:
		self.stream_service.start()
		self.logger.debug('Stream started with provider: %s', self.stream_provider.value)
		return {'status': 'Stream started'}

	def stop(self) -> dict:
		self.stream_service.stop()
		self.logger.debug('Stream stopped with provider: %s', self.stream_provider.value)
		return {'status': 'Stream stopped'}

	def status(self) -> dict:
		return {'status': self.stream_service.get_status()}

	def focus(self) -> dict:
		self.logger.debug('Adjusting focus for stream provider: %s', self.stream_provider.value)
		self.stream_service.focus()
		return {'status': 'Focus adjusted'}

	def feed(self) -> Generator[bytes, Any, None]:
		stream_provider_service = StreamProviderService(self.stream_provider)
		if self.stream_provider == StreamProvider.HTTP:
			feed = self.stream_service.feed(format=EncodeType.JPEG)
			provided = stream_provider_service.provide(feed)
			yield from provided or [b'']
		elif self.stream_provider == StreamProvider.RTMP:
			self.logger.debug('Starting to provide RTMP stream with feed...')
			feed = self.stream_service.feed()
			if not isinstance(feed, Generator):
				self.logger.debug('RTMP stream feed is empty, not starting stream.')
				yield b''
			url = self.settings.stream.url or 'rtmp://192.168.101.191/live/livestream'
			self.logger.debug(f'Starting RTMP stream with URL: {url}')
			threading.Thread(
				target=stream_provider_service.provide,
				args=(),
				kwargs={'feed': feed, 'url': url, 'format': EncodeType.H264},
				daemon=True,
			).start()
			self.logger.debug('RTMP stream started with URL: %s', url)
			yield b'RTMP streaming...'
