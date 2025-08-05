import subprocess
from collections.abc import Generator
from fractions import Fraction
from subprocess import DEVNULL, PIPE
from typing import Any

import av
import ffmpeg
import numpy as np

from app.models import Frame
from app.models.stream import StreamProvider
from app.services.logger import Logger
from config.settings import Settings


class StreamProviderService:
	def __init__(self, provider: StreamProvider, active: bool) -> None:
		self.provider = provider
		self.settings = Settings
		self.logger = Logger(name='stream_provider_service')
		self.active = active

	def provide(
		self,
		feed: Generator[Frame, Any | None, None],
		url: str | None = None,
	) -> Generator[bytes, Any, None]:
		if self.active:
			if self.provider == StreamProvider.RTMP:
				width, height = self.settings.stream.resolution
				fps = self.settings.stream.fps
				cmd = [
					'ffmpeg',
					'-re',
					'-f',
					'rawvideo',
					'-pix_fmt',
					'bgr24',
					'-s',
					f'{width}x{height}',
					'-i',
					'pipe:0',
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
				self.logger.debug(f'StreamProvider: spawning ffmpeg → {url}')
				process = subprocess.Popen(
					cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
				)
				if process.stdin is not None:
					try:
						yield b'RTMP streaming...'
						for frame in feed:
							frame_data = frame.data.tobytes()
							process.stdin.write(frame_data)
					finally:
						process.terminate()
						process.wait()

			elif (
				self.provider == StreamProvider.RTSP
			):  # RTSP streaming is not implemented in this example, placeholder for RTMP using ffmpeg
				width, height = self.settings.stream.resolution
				fps = self.settings.stream.fps
				self.logger.debug(f'StreamProvider: spawning ffmpeg → {url}')
				stream = ffmpeg.input(
					'pipe:0',
					**({'re': None} if self.settings.stream.realtime else {}),
					**(
						{'fflags': self.settings.stream.fflags}
						if self.settings.stream.fflags
						else {}
					),
					format='rawvideo',
					pix_fmt='bgr24',
					s=f'{width}x{height}',
					r=Fraction(fps),
					thread_queue_size=self.settings.stream.thread_queue_size,
				)
				stream = ffmpeg.output(
					stream,
					url,
					format=self.settings.stream.format,
					vcodec=self.settings.stream.vcodec,
					preset=self.settings.stream.preset,
					tune=self.settings.stream.tune,
					threads=self.settings.stream.threads,
					g=self.settings.stream.gop,
					pix_fmt=self.settings.stream.pixel_format,
					b=f'{self.settings.stream.bitrate}k',
					maxrate=f'{self.settings.stream.bitrate}k',
					bufsize=f'{self.settings.stream.bitrate // 2}k',
				)
				self.logger.debug(f'StreamProvider: spawning ffmpeg → {url}')
				args = stream.get_args()
				self.logger.debug(f'FFMPEG command: {" ".join(args)}')
				process = ffmpeg.run_async(
					stream,
					pipe_stdin=True,
					pipe_stdout=True,
					pipe_stderr=True,
				)
				if process.stdin is not None:
					try:
						yield b'RTMP streaming...'
						for frame in feed:
							process.stdin.write(frame.data.tobytes())
					finally:
						process.stdin.close()
						process.wait()

			else:
				for frame in feed:
					yield frame.data.tobytes()
		else:
			yield b'Streaming is not active'
