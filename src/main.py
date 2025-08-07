import uvicorn

from app.adapters.hardware.camera import Picamera2Adapter
from app.adapters.software.api import FastAPIAdapter
from app.models.stream import StreamProvider
from app.services.api import ApiService
from app.services.api.stream import StreamController
from app.services.video import VideoService
from app.services.video.stream import StreamService
from app.services.web import WebService


def main() -> None:
	camera_0 = Picamera2Adapter(0)
	camera_1 = Picamera2Adapter(1)

	video_service = VideoService(cam0=camera_0, cam1=camera_1)

	stream_service = StreamService(video_service)

	stream_controller = StreamController(stream_service, provider=StreamProvider.RTMP)

	stream_controller.start()
	gen = stream_controller.feed()

	for chunk in gen:
		print(chunk)

	# api_adapter = FastAPIAdapter()
	# ApiService(api=api_adapter, stream_service=stream_service)
	# WebService(api=api_adapter)
	# app = api_adapter.expose()
	# uvicorn.run(app, host='0.0.0.0', port=8000, log_level='info', access_log=True)


if __name__ == '__main__':
	main()
