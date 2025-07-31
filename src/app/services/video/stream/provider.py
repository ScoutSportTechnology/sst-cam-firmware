from collections.abc import Generator
from subprocess import Popen
from typing import Any

import ffmpeg

from app.models.stream import EncodeType, StreamProvider
from config.settings import Settings


class StreamProviderService:
	def __init__(self, provider: StreamProvider) -> None:
		self.provider = provider
		self.settings = Settings

	def provide(
		self,
		feed: Generator[bytes, Any, None],
		url: str | None = None,
		format: EncodeType | None = None,
	) -> Generator[bytes, Any, None] | None:
		if self.provider == StreamProvider.HTTP:
			for frame in feed:
				yield b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + frame + b'\r\n'
		elif format is not None:
			if self.provider == StreamProvider.RTMP:
				width, height = self.settings.stream.resolution
				fps = self.settings.stream.fps

				input_stream = ffmpeg.input(
					'pipe:0',
					format=EncodeType.RAW.value,
					pix_fmt='bgr24',
					s=f'{width}x{height}',
					r=fps,
				)
				output_stream = ffmpeg.output(
					input_stream,
					url,
					vcodec='libx264',
					format=format.value,
					bitrate=self.settings.stream.bitrate,
					preset=self.settings.stream.preset,
				)
				process = ffmpeg.run_async(
					output_stream, pipe_stdin=True, pipe_stdout=True, pipe_stderr=True
				)
				try:
					for frame in feed:
						if process.stdin:
							process.stdin.write(frame)
				finally:
					if process.stdin:
						process.stdin.close()
					process.wait()
		else:
			for frame in feed:
				yield frame
