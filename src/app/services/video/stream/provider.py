from collections.abc import Generator
from fractions import Fraction
from subprocess import Popen
from typing import Any

import av
import ffmpeg
import numpy as np

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
		feed: Generator[bytes, Any, None],
		url: str | None = None,
		format: EncodeType | None = None,
	) -> Generator[bytes, Any, None] | None:
		self.logger.debug('Treath started for provider() ----------------------------------')
		if self.provider == StreamProvider.HTTP:
			for frame in feed:
				yield b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + frame + b'\r\n'
		elif format is not None:
			if self.provider == StreamProvider.RTMP:
				width, height = self.settings.stream.resolution
				fps = self.settings.stream.fps
				time_base = Fraction(1, self.settings.stream.fps)
				container = av.open(url, 'w', format='flv')
				stream = container.add_stream(codec_name="h264", rate=fps, GOP_size=1)
				stream.width, stream.height = (width, height)
				stream.pix_fmt = 'yuv420p'
				stream.gop_size = 1
				for frame_idx, frame in enumerate(feed):
					frame_array = np.frombuffer(frame, dtype=np.uint8)
					frame_array = frame_array.reshape((height, width, 3))
					av_frame = av.VideoFrame.from_ndarray(frame_array, format='bgr24')
					av_frame.pts = frame_idx
					av_frame.time_base = time_base

					for packet in stream.encode(av_frame):
						container.mux(packet)
				for packet in stream.encode():
					if packet:
						container.mux(packet)
				self.logger.debug('Closing container after encoding frame')
				container.close()
				yield b'RTMP streaming...'
		else:
			for frame in feed:
				yield frame
