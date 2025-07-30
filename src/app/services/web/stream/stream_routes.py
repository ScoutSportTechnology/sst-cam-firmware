from app.interfaces import IStream
from app.interfaces.api import IApi, IApiRoutes
from app.models.api import ResponseModel
from app.services.web.stream.stream_controller import StreamController


class StreamRoutes(IApiRoutes):
	def __init__(self, api: IApi) -> None:
		self.api = api
		self.controller = StreamController()

		self.register()

	def register(self) -> None:
		self.api.add_route(
			'/web/stream',
			self.controller.index,
			methods=['GET'],
			response_model=ResponseModel.html,
		)
