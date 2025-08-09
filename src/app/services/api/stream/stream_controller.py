import subprocess
import threading
from collections.abc import Generator
from typing import Any

from sympy import true

from app.interfaces import IStream
from app.interfaces.api.controller import IApiController
from app.models import frame
from app.models.stream import EncodeType, StreamProvider
from app.services.logger import Logger
from config.settings import Settings


class StreamController(IApiController):
	def __init__(self, stream_service: IStream, provider: StreamProvider) -> None:
		self.stream_service = stream_service
		self.stream_provider = provider
		self.settings = Settings
		self.logger = Logger(name='stream_controller')
		self.active = False
		self._worker: threading.Thread | None = None

	@property
	def provider(self) -> StreamProvider:
		return self.stream_provider

	def start(self) -> dict:
		if not self.active:
			self.stream_service.start()
			self.active = True

		return {'status': 'Stream started'}

	def stop(self) -> dict:
		if self.active:
			self.stream_service.stop()
			self.logger.debug(f'Stream stopped with provider: {self.stream_provider.value}')
			self.active = False
		return {'status': 'Stream stopped'}

	def status(self) -> dict:
		return {'status': 'active' if self.active else 'inactive'}

	def focus(self) -> dict:
		if self.active:
			self.logger.debug(f'Adjusting focus for stream provider: {self.stream_provider.value}')
			self.stream_service.focus()
			return {'status': 'Focus adjusted'}
		else:
			self.logger.warning('Cannot adjust focus, stream is not active')
			return {'status': 'Stream is inactive, cannot adjust focus'}

	def feed(self) -> dict:
		if self.active:
			self.stream_service.feed(
				stream_provider=self.stream_provider,
				url='rtmp://192.168.101.191/live/livestream',
			)
			return {'status': 'Feed started'}
		else:
			self.logger.warning('Cannot provide feed, stream is not active')
			return {'status': 'Stream is inactive, cannot provide feed'}
