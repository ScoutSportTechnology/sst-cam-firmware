from collections.abc import Generator

from app.infra.logger import Logger
from app.interfaces.capturer import IVideoService
from app.interfaces.streamer import IStreamService
from app.models.capturer import Frame
from app.models.streamer import StreamProtocol
from app.services.streamer.provider import StreamProviderService
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
		if self.active and self.video_service.status:
			self.logger.debug(f'Starting feed for provider: {stream_protocol.value}')
			feed: Generator[Frame, None, None] = self.video_service.frames()
			self.stream_provider_service = StreamProviderService(stream_protocol, self.active)
			self.stream_provider_service.start(feed, url)
