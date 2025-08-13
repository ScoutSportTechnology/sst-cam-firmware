import threading
import time
from collections.abc import Generator
from fractions import Fraction
from typing import Any

import av
import numpy as np
from av.container import Flags as ContainerFlags
from sympy import EX

from app.interfaces.streamer import IStreamProvider
from app.models.capturer import Frame
from app.models.streamer import StreamProtocol
from app.services.logger import Logger
from config.settings import Settings


class StreamProviderService(IStreamProvider):
	def __init__(self, protocol: StreamProtocol, active: bool) -> None:
		self.protocol = protocol
		self.settings = Settings
		self.logger = Logger(name='stream_provider_service')
		self.active = active
		self._worker: threading.Thread | None = None
		self._stop = threading.Event()

	def start(
		self,
		feed: Generator[Frame, None, None],
		url: str,
	) -> None:
		if self.active:
			self.logger.debug(f'Starting feed for provider: {self.protocol.value}')
			if self._worker and self._worker.is_alive():
				self.logger.warning('Stream provider worker is already running')
				return
			self._stop.clear()
			match self.protocol:
				case StreamProtocol.HTTP:
					pass
				case StreamProtocol.RTMP:
					self._worker = threading.Thread(
						target=self._rtmp, args=(feed, url), daemon=True
					)
					self._worker.start()
				case _:
					pass
		else:
			self.logger.warning('Stream is inactive, cannot provide feed')
			return

	def stop(self) -> None:
		self._stop.set()
		if self._worker and self._worker.is_alive():
			self._worker.join(timeout=2.0)
		self._worker = None

	def _rtmp(
		self,
		feed: Generator[Frame, None, None],
		url: str,
	) -> None:
		try:
			self.logger.debug(f'Starting RTMP stream to {url}')
			width, height = (
				2 * self.settings.stream.resolution[0],
				self.settings.stream.resolution[1],
			)
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
			stream.color_primaries = 1  # BT.709
			stream.color_trc = 1  # BT.709
			stream.colorspace = 1  # BT.709
			stream.color_range = 0  # Full range
			stream.max_b_frames = 2

			try:
				self.logger.debug('RTMP streaming started')
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
			except av.EOFError:
				self.logger.error('RTMP streaming ended unexpectedly (EOFError)')
				pass
			finally:
				container.close()
				self.logger.debug('RTMP streaming ended')
		except Exception as e:
			self.logger.error(f'RTMP streaming error: {e}')
			pass
