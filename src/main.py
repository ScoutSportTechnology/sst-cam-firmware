from app.adapters.capturer import Picamera2Adapter
from app.models.streamer import StreamProtocol
from app.services.streamer import StreamService
from app.services.video import VideoService


def main() -> None:
	camera_0 = Picamera2Adapter(0)
	camera_1 = Picamera2Adapter(1)

	video_service = VideoService(cam0=camera_0, cam1=camera_1)

	stream_service = StreamService(video_service)
	stream_service.start()
	stream_service.stream(
		stream_protocol=StreamProtocol.RTMP, url='rtmp://192.168.101.191/live/stream'
	)


if __name__ == '__main__':
	main()
