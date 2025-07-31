import threading
from collections.abc import Generator
from typing import Any

from app.interfaces import IStream
from app.interfaces.api.controller import IApiController
from app.models.stream import EncodeType, StreamProvider
from app.services.video.stream import StreamProviderService
from config.settings import Settings


class StreamController(IApiController):
	def __init__(self, stream_service: IStream) -> None:
		self.stream_service = stream_service
		self.stream_provider = stream_service.provider
		self.settings = Settings

	def start(self) -> dict:
		self.stream_service.start()
		return {'status': 'Stream started'}

	def stop(self) -> dict:
		self.stream_service.stop()
		return {'status': 'Stream stopped'}

	def status(self) -> dict:
		return {'status': self.stream_service.get_status()}

	def focus(self) -> dict:
		self.stream_service.focus()
		return {'status': 'Focus adjusted'}

	def feed(self) -> Generator[bytes, Any, None]:
		stream_provider_service = StreamProviderService(self.stream_provider)
		if self.stream_provider == StreamProvider.HTTP:
			feed = self.stream_service.feed(format=EncodeType.JPEG)
			provided = stream_provider_service.provide(feed)
			yield from provided or [b'']
		elif self.stream_provider == StreamProvider.RTMP:
			feed = self.stream_service.feed()
			threading.Thread(
				target=stream_provider_service.provide,
				args=(feed,),
				kwargs={'url': self.settings.stream.url, 'format': EncodeType.H264},
				daemon=True,
			).start()
			yield b'RTMP stream started'
