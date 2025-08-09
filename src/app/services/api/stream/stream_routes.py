from app.interfaces import IStream
from app.interfaces.api import IApi, IApiRoutes
from app.models.api import ResponseModel
from app.models.stream import EncodeType, StreamProvider
from app.services.api.stream.stream_controller import StreamController


class StreamRoutes(IApiRoutes):
	def __init__(self, api: IApi, stream_service: IStream) -> None:
		self.api = api
		self.controller = StreamController(
			stream_service=stream_service, provider=StreamProvider.RTMP
		)

		self.register()

	def register(self) -> None:
		self.api.add_route(
			'/api/stream/start',
			self.controller.start,
			methods=['GET'],
			response_model=ResponseModel.empty,
		)
		self.api.add_route(
			'/api/stream/stop',
			self.controller.stop,
			methods=['GET'],
			response_model=ResponseModel.empty,
		)
		self.api.add_route(
			'/api/stream/status',
			self.controller.status,
			methods=['GET'],
			response_model=ResponseModel.empty,
		)
		self.api.add_route(
			'/api/stream/feed',
			self.controller.feed,
			methods=['GET'],
			response_model=ResponseModel.stream_status,
		)
		self.api.add_route(
			'/api/stream/focus',
			self.controller.focus,
			methods=['GET'],
			response_model=ResponseModel.empty,
		)
