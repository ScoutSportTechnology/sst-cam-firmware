from collections.abc import Generator
from typing import Any

from app.interfaces import IStream
from app.interfaces.api.controller import IApiController


class StreamController(IApiController):
	def __init__(self, stream_service: IStream) -> None:
		self.stream_service = stream_service

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
		for frame_bytes in self.stream_service.feed('.jpeg'):
			yield b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n'
