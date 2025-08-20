from typing import Any
from unittest import case

from app.interfaces.controller import IController
from app.interfaces.streamer import IStreamService
from app.models.controller import ControllerPayload
from app.models.streamer import StreamCommands, StreamProtocol


class StreamController(IController):
	def __init__(self, service: IStreamService) -> None:
		self.service = service
		self.active = False

	def _on_command(self, payload: ControllerPayload) -> None:
		command = payload.command
		if payload.data is None:
			return
		data = payload.data

		match command:
			case StreamCommands.START:
				self.service.start()
			case StreamCommands.STOP:
				self.service.stop()
			case StreamCommands.RESTART:
				self.service.stop()
				self.service.start()
			case StreamCommands.STATUS:
				self.service.status()
			case StreamCommands.FOCUS:
				self.service.focus()
			case StreamCommands.STREAM:
				url: str = data.url
				stream_protocol: StreamProtocol = data.stream_protocol
				self.service.stream(url=url, stream_protocol=stream_protocol)
