import subprocess
from collections.abc import Generator
from fractions import Fraction
from subprocess import PIPE, Popen
from typing import Any, cast

import av
import ffmpeg
import numpy as np

from app.models import Frame
from app.models.stream import EncodeType, StreamProvider
from app.services.logger import Logger
from config.settings import Settings


class StreamProviderService:
	def __init__(self, provider: StreamProvider) -> None:
		self.provider = provider
		self.settings = Settings
		self.logger = Logger(name='stream_provider_service')

	def provide(
		self,
		feed: Generator[bytes | Frame, Any | None, None],
		url: str | None = None,
	) -> Generator[bytes, Any, None] | None:
		if self.provider == StreamProvider.HTTP:
			feed = cast(Generator[bytes, Any, None], feed)
			for frame in feed:
				yield b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + frame + b'\r\n'
		elif self.provider == StreamProvider.RTMP:
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
				'-',  # read from stdin
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
			process = subprocess.Popen(
				cmd,
				# stdin=PIPE,
			)
			try:
				for frame in cast(Generator[Frame, Any, None], feed):
					if process.stdin is not None:
						process.stdin.write(frame.data.tobytes())
					else:
						self.logger.error('process.stdin is None, cannot write frame data.')
						break
			finally:
				if process.stdin is not None:
					process.stdin.close()
					process.wait()
				else:
					self.logger.error('process.stdin is None, cannot close stdin.')
		elif self.provider == StreamProvider.RTSP:
			feed = cast(Generator[Frame, Any, None], feed)
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
			yield b'RTMP streaming...'
		else:
			for frame in feed:
				frame = cast(Frame, frame) if not isinstance(frame, bytes) else frame
				yield frame if isinstance(frame, bytes) else frame.data.tobytes()
