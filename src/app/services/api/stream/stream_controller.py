from io import BytesIO

from app.interfaces import IStream
from app.interfaces.api.controller import IApiController


class StreamController(IApiController):
	def __init__(self, stream_service: IStream):
		self.stream_service = stream_service

	def start(self) -> dict:
		self.stream_service.start()
		return {'status': 'Stream started'}

	def stop(self) -> dict:
		self.stream_service.stop()
		return {'status': 'Stream stopped'}

	def status(self) -> dict:
		return {'status': self.stream_service.get_status()}

	def feed(self) -> Generator[bytes, None, None]:
		while True:
			frame_list = self.stream_service.get_feed()
			yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + frame_list[2] + b'\r\n')
