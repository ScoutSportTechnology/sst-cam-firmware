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
				time_base = Fraction(1, self.settings.stream.fps)
				container = av.open(
					url,
					'w',
					format='flv',
				)
				container.flags |= ContainerFlags.no_buffer.value
				stream = container.add_stream(
					codec_name='h264',
					rate=fps,
				)
				ctx = stream.codec_context
				ctx.time_base = time_base
				ctx.width = width
				ctx.height = height
				ctx.pix_fmt = 'yuv420p'
				ctx.bit_rate = 4_000_000  # 4 Mbps
				ctx.flags |= CodecFlags.low_delay.value
				ctx.gop_size = self.settings.stream.buffer_size
				ctx.options = {
					'maxrate': '4M',
					'bufsize': '2M',
					'profile': 'high',
					'level': '4.2',
					'tune': 'zerolatency',
					'crf': '23',
				}

				self.logger.debug('StreamProvider: RTMP streaming started')
				yield b'RTMP streaming...'
				for frame_idx, frame in enumerate(feed):
					av_frame = av.VideoFrame.from_ndarray(
						frame.data, format=self.settings.camera.pixel_format
					)
					av_frame.pts = frame_idx
					av_frame.time_base = time_base
					for packet in stream.encode(av_frame):
						container.mux(packet)
				for packet in stream.encode():
					container.mux(packet)
				self.logger.debug('Closing container after encoding frame')
				container.close()
				self.logger.debug('StreamProvider: RTMP streaming completed')
				yield b'RTMP streaming completed'
				return

			else:
				for frame in feed:
					yield frame.data.tobytes()
		else:
			yield b'Streaming is not active'
