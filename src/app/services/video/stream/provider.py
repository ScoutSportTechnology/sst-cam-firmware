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
	def __init__(self, provider: StreamProvider) -> None:
		self.provider = provider
		self.settings = Settings
		self.logger = Logger(name='stream_provider_service')

	def provide(
		self,
		feed: Generator[Frame, Any | None, None],
		url: str | None = None,
	) -> Generator[bytes, Any, None] | None:
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
				'-r',
				str(fps),
				'-i',
				'-',
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
			process = subprocess.Popen(cmd, stdin=PIPE, stdout=DEVNULL, stderr = DEVNULL)
			if process.stdin is not None:
				for frame in feed:
					try:
						process.stdin.write(frame.data.tobytes())
					except Exception as e:
						self.logger.error(f'Error writing frame data: {e}')
						break
				process.stdin.close()
				process.wait()
			else:
				self.logger.error('process.stdin is None, cannot close stdin.')

		elif (
			self.provider == StreamProvider.RTSP
		):  # RTSP streaming is not implemented in this example, placeholder for RTMP using av
			width, height = self.settings.stream.resolution
			fps = self.settings.stream.fps
			time_base = Fraction(1, self.settings.stream.fps)
			container = av.open(url, 'w', format='flv')
			stream = container.add_stream(codec_name='h264', rate=fps, GOP_size=1)
			stream.width, stream.height = (width, height)
			stream.pix_fmt = 'yuv420p'
			stream.gop_size = 1
			for frame_idx, frame in enumerate(feed):
				av_frame = av.VideoFrame.from_ndarray(frame.data, format='bgr24')
				av_frame.pts = frame_idx
				av_frame.time_base = time_base

				for packet in stream.encode(av_frame):
					container.mux(packet)
			for packet in stream.encode():
				container.mux(packet)
			self.logger.debug('Closing container after encoding frame')
			container.close()
			return

		else:
			for frame in feed:
				yield frame.data.tobytes()
