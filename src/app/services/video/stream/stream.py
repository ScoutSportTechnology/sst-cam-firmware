import subprocess
import threading
from collections.abc import Generator
from typing import Any, final

import cv2
import ffmpeg

from app.interfaces.stream import IStream
from app.models import Frame
from app.models.stream import EncodeType, StreamProvider
from app.services.logger import Logger
from app.services.video import VideoService
from app.services.video.stream import StreamProviderService
from config.settings import Settings


class StreamService(IStream):
	def __init__(self, video_service: VideoService) -> None:
		self.active = False
		self.video_service = video_service
		self.settings = Settings
		self.logger = Logger(name='stream_service')

	def start(self) -> None:
		if not self.active:
			self.video_service.start()
			self.active = True

	def stop(self) -> None:
		if self.active:
			self.video_service.stop()
			self.active = False

	def get_status(self) -> str:
		return 'active' if self.active else 'inactive'

	def focus(self) -> None:
		if self.active:
			self.video_service.focus()
		else:
			self.logger.warning('Cannot adjust focus, stream is not active')

	def feed(
		self,
		stream_provider: StreamProvider,
		url: str | None = None,
	) -> Generator[bytes, Any, None]:
		self.provider = stream_provider
		
		if self.active and self.video_service.status() == 'active':
			feed: Generator[Frame, None, None] = self.video_service.frames()
			stream_provider_service = StreamProviderService(self.provider)
			if self.provider == StreamProvider.HTTP:
				for frame in feed:
					success, buffer = cv2.imencode(
						f'.{EncodeType.JPEG.value}',
						frame.data,
						(int(cv2.IMWRITE_JPEG_QUALITY), 100),
					)
					if not success:
						self.logger.error('Failed to encode frame, skipping...')
						continue
					bytes = buffer.tobytes()
					yield b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + bytes + b'\r\n'
				return

			elif self.provider == StreamProvider.RTMP:
				self.logger.debug(f'Starting RTMP stream with URL: {url}')
				threading.Thread(
					target=stream_provider_service.provide,
					args=(),
					kwargs={'feed': feed, 'url': url},
					daemon=True,
				).start()
				self.logger.debug(f'RTMP stream started with URL: {url}')
				while self.active:
					yield b'RTMP streaming...'
				return

			elif self.provider == StreamProvider.RTSP:
				self.logger.debug(f'Starting RTSP stream with URL: {url}')
				threading.Thread(
					target=stream_provider_service.provide,
					args=(),
					kwargs={'feed': feed, 'url': url},
					daemon=True,
				).start()
				self.logger.debug(f'RTSP stream started with URL: {url}')
				while self.active:
					yield b'RTSP streaming...'
				return

			elif self.provider == StreamProvider.FFMPEG:
				self.logger.debug('Starting to provide FFMPEG stream with feed...')
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
					url,
				]
				process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
				self.logger.debug(f'Process Logs {process.stdout} {process.stderr}')
				yield b'FFMPEG streaming...'
				return

			else:
				for frame in feed:
					self.logger.debug('Processing frame to bytes')
					yield frame.data.tobytes()
				return
		else:
			self.logger.warning('Stream is not active, cannot provide feed')
			yield b'Stream is inactive, cannot provide feed'
			return
