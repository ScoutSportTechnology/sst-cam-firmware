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
						'preset': 'veryfast',
						'tune': 'zerolatency',
						'profile': 'high',
						'level': '4.2',
						'b': f'{bitrate}',
						'x264-params': (
							f'bitrate={bitrate / 1000}:'
							f'vbv-maxrate={bitrate / 1000}:'
							f'vbv-bufsize={bitrate / 1000}:'
							f'nal-hrd=cbr:'
							f'filler=1:'
							f'keyint={gop_size}:'
							f'min-keyint={gop_size}:'
							f'scenecut=0:'
							f'bframes=2:'
							f'ref=1:'
							f'rc-lookahead=0:'
							f'colorprim=bt709:'
							f'transfer=bt709:'
							f'colormatrix=bt709:'
						),
					},
				)
				ctx = stream.codec_context
				ctx.time_base = time_base
				ctx.width = width
				ctx.height = height
				#ctx.pix_fmt = self.settings.camera.pix_fmt
				ctx.bit_rate = bitrate
				ctx.gop_size = gop_size
				ctx.max_b_frames = 2

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

			else:
				for frame in feed:
					yield frame.data.tobytes()
		else:
			yield b'Streaming is not active'
