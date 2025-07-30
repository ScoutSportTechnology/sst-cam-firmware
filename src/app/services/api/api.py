from app.interfaces.api import IApi
from app.interfaces.stream import IStream
from app.services.api.stream import StreamRoutes


class ApiService:
	def __init__(self, api: IApi, stream_service: IStream) -> None:
		self.api = api
		self.stream_service = stream_service
		self.stream_routes = StreamRoutes(api, stream_service)
		
