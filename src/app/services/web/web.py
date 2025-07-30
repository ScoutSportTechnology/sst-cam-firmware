from app.interfaces.api import IApi
from app.services.web.stream import StreamRoutes


class WebService:
	def __init__(
		self,
		api: IApi,
	) -> None:
		self.api = api
		self.stream_routes = StreamRoutes(api)
