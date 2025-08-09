from pathlib import Path

from app.interfaces import IStream
from app.interfaces.api import IApi, IWebRoutes
from app.models.api import ResponseModel
from app.services.web.stream.stream_controller import StreamController


class StreamRoutes(IWebRoutes):
	def __init__(self, api: IApi) -> None:
		self.api = api
		self.controller = StreamController(base_dir=Path('app', 'services'))

		self.register()

	def register(self) -> None:
		self.api.add_route(
			'/web/stream',
			self.controller.index,
			methods=['GET'],
			response_model=ResponseModel.html,
		)
