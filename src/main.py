import uvicorn

from app.adapters.hardware.camera import Picamera2Adapter
from app.adapters.software.api import FastAPIAdapter
from app.services.api.stream import StreamRoutes
from app.services.video import VideoService
from app.services.video.streaming import StreamService


def main() -> None:
	camera_0 = Picamera2Adapter(0)
	camera_1 = Picamera2Adapter(1)

	video_service = VideoService(cam0=camera_0, cam1=camera_1)
	
	video_service.preprocess()
	video_service.postprocess()
	stream_service = video_service.stream('.jpg')

	api_adapter = FastAPIAdapter()
	StreamRoutes(api=api_adapter, stream_service=stream_service).register()

	# Expose the FastAPI app
	app = api_adapter.expose()

	uvicorn.run(app, host='0.0.0.0', port=8000)


if __name__ == '__main__':
	main()
