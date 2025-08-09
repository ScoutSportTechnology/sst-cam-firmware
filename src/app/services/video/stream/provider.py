import time
from collections.abc import Generator
from fractions import Fraction
from typing import Any

import av
import numpy as np
from av.codec.context import Flags as CodecFlags
from av.container import Flags as ContainerFlags

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
				time_base = Fraction(1, fps)
				bitrate = 25_000_000  # 25 Mbps
				gop_size = fps * self.settings.stream.buffer_seconds
				container = av.open(
					url,
					'w',
					format='flv',
				)
				container.flags |= ContainerFlags.no_buffer.value
				stream = container.add_stream(
					codec_name='h264',
					rate=fps,
					options={
						'preset': 'ultrafast',
						'tune': 'zerolatency',
						'level': '4.2',
					},
				)

				stream.pix_fmt = 'yuv420p'
				stream.width = width
				stream.height = height
				stream.profile = 'high422'
				stream.gop_size = gop_size
				stream.bit_rate = bitrate
				stream.color_primaries = 1 # BT.709
				stream.color_trc = 1 # BT.709
				stream.colorspace = 1 # BT.709
				stream.color_range = 0 # Full range
				stream.max_b_frames = 2
				
				try:
					self.logger.debug('StreamProvider: RTMP streaming started')
					yield b'RTMP streaming...'
					for frame_idx, frame in enumerate(feed):
						av_frame = av.VideoFrame.from_ndarray(
							frame.data, format=self.settings.camera.pix_fmt
						)
						av_frame.pts = frame_idx
						av_frame.time_base = time_base
						
						for packet in stream.encode(av_frame):
							container.mux(packet)

					for packet in stream.encode():
						container.mux(packet)

					container.close()
					
					yield b'RTMP streaming completed'
					return
				except av.EOFError:
					self.logger.error('RTMP streaming ended unexpectedly (EOFError)')
					pass

			else:
				for frame in feed:
					yield frame.data.tobytes()
		else:
			yield b'Streaming is not active'
