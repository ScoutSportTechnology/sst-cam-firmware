from app.adapters.capturer import CapturerAdapter
from app.models.streamer import StreamProtocol
from app.services.streamer import StreamService
from app.services.video import VideoService


def main() -> None:
	camera_0 = CapturerAdapter(0)
	camera_1 = CapturerAdapter(1)

	video_service = VideoService(cam0=camera_0, cam1=camera_1)

	stream_service = StreamService(video_service)
	stream_service.start()
	stream_service.stream(stream_protocol=StreamProtocol.RTMP, url='rtmp://192.168.101.191/live/stream')
	while True:
		stream_service.status()


if __name__ == '__main__':
	main()