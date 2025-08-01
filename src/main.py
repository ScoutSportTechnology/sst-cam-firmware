import logging
import subprocess
from subprocess import PIPE, Popen

import uvicorn

from app.adapters.hardware.camera import Picamera2Adapter
from app.adapters.software.api import FastAPIAdapter
from app.models.stream import StreamProvider
from app.services.api import ApiService
from app.services.video import VideoService
from app.services.video.stream import StreamService
from app.services.web import WebService


def main() -> None:
	# camera_0 = Picamera2Adapter(0)
	# camera_1 = Picamera2Adapter(1)
	#
	# video_service = VideoService(cam0=camera_0, cam1=camera_1)
	#
	# stream_service = StreamService(video_service, provider=StreamProvider.FFMPEG)
	# api_adapter = FastAPIAdapter()
	#
	# ApiService(api=api_adapter, stream_service=stream_service)
	# WebService(api=api_adapter)
	#
	# app = api_adapter.expose()
	# uvicorn.run(app, host='0.0.0.0', port=8000, log_level='info', access_log=True)

	cmd = [
		'ffmpeg',
		'-re',
		'-f',
		'lavfi',
		'-i',
		'testsrc=duration=30:size=1920x1080:rate=15',
		'-c:v',
		'libx264',
		'-preset',
		'ultrafast',
		'-tune',
		'zerolatency',
		'-f',
		'flv',
		'rtmp://192.168.101.191/live/livestream',
	]
	process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

	stdout, stderr = process.communicate()
	print(f'Process Logs {stdout} {stderr}')


if __name__ == '__main__':
	main()
