from app.interfaces import IStream
from app.interfaces.api import IApi, IApiRoutes

from .stream_controller import StreamController


class StreamRoutes(IApiRoutes):
	def __init__(self, api: IApi, stream_service: IStream) -> None:
		self.api = api
		self.controller = StreamController(stream_service)

	def register(self) -> None:
		self.api.add_route('/api/stream/start', self.controller.start, methods=['GET'])
		self.api.add_route('/api/stream/stop', self.controller.stop, methods=['GET'])
		self.api.add_route('/api/stream/status', self.controller.status, methods=['GET'])
		self.api.add_route('/api/stream/feed', self.controller.feed, methods=['GET'])
