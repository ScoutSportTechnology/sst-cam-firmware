import threading
import time
from collections.abc import Generator
from fractions import Fraction

import av
from av.container import Flags as ContainerFlags

from app.infra.logger import Logger
from app.interfaces.streamer import IStreamProvider
from app.models.capturer import Frame
from app.models.streamer import StreamProtocol
from app.services.bufferer import BufferService
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
			buffered_feed = BufferService().buffer(feed)
			if self._worker and self._worker.is_alive():
				self.logger.warning('Stream provider worker is already running')
				return
			self._stop.clear()
			match self.protocol:
				case StreamProtocol.HTTP:
					pass
				case StreamProtocol.RTMP:
					self._worker = threading.Thread(target=self._rtmp, args=(buffered_feed, url), daemon=True)
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

			width = self.settings.stream.resolution[0]
			height = self.settings.stream.resolution[1]

			self.logger.debug(f'RTMP stream resolution: {width}x{height}')

			ticks_per_frame = 1
			fps = int(self.settings.stream.fps)
			time_base = Fraction(1, fps * ticks_per_frame)
			frame_rate = Fraction(fps, 1)

			bitrate = 15_000_000  # 25 Mbps
			gop_size = int(fps)

			container = av.open(
				url,
				'w',
				format='flv',
				options={'rtmp_live': 'live', 'flvflags': 'no_duration_filesize'},
			)
			container.flags |= ContainerFlags.no_buffer.value

			stream: av.VideoStream = container.add_stream(
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

			stream.width = width
			stream.height = height

			stream.pix_fmt = 'yuv420p'
			stream.profile = 'high'

			stream.gop_size = gop_size
			stream.bit_rate = bitrate

			stream.codec_context.time_base = time_base
			stream.codec_context.framerate = frame_rate
			stream.time_base = time_base

			try:
				self.logger.debug('RTMP streaming started')
				pts = 0
				for i, frame in enumerate(feed):
					# t0 = time.perf_counter()
					av_frame = av.VideoFrame.from_ndarray(frame.data, format=self.settings.camera.ffmpeg_fmt)

					av_frame.pts = pts
					av_frame.time_base = time_base

					# if pts <= 40:
					# self.logger.debug(f'PTS={pts} t={float(pts * time_base):.6f}s')

					pts += ticks_per_frame

					for packet in stream.encode(av_frame):
						container.mux(packet)
					# t1 = time.perf_counter()
					if i < 100 or (i % 30 == 0):
						pass  # self.logger.debug(f'encode+mux_ms={(t1 - t0) * 1000:.1f}')
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
