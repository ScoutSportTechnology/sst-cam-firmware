from abc import ABC, abstractmethod
from collections.abc import Generator
from typing import Any

from app.models import Frame
from app.models.stream import EncodeType
from app.models.stream.stream_provider import StreamProvider
from app.services.video.video import VideoService


class IStream(ABC):
	@abstractmethod
	def __init__(self, video_service: VideoService, provider: StreamProvider) -> None: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def get_status(self) -> str: ...

	@abstractmethod
	def focus(self) -> None: ...

	@abstractmethod
	def feed(
		self,
		stream_provider: StreamProvider,
		url: str,
	) -> None: ...
