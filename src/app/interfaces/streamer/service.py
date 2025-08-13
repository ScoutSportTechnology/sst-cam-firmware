from abc import ABC, abstractmethod

from app.interfaces.capturer import IVideoService
from app.models.streamer import StreamProtocol

from .provider import IStreamProvider


class IStreamService(ABC):
	@abstractmethod
	def __init__(self, video_service: IVideoService, protocol: StreamProtocol) -> None: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def status(self) -> bool: ...

	@abstractmethod
	def focus(self) -> None: ...

	@abstractmethod
	def stream(
		self,
		stream_protocol: StreamProtocol,
		url: str,
	) -> None: ...
