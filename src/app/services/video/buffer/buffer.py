import time
from collections import deque
from collections.abc import Generator
from typing import Any

from app.models import Frame
from app.services.logger import Logger
from config.settings import Settings


class BufferService:
	def __init__(self) -> None:
		self.settings = Settings
		self.logger = Logger(name='buffer_service')
		self.buffer_size = self.settings.stream.buffer_size
		self.buffer = deque(maxlen=self.buffer_size)

	def feed(
		self,
		feed: Generator[Frame, None, None],
	) -> Generator[Frame, Any, None]:
		buffer = deque(maxlen=self.settings.stream.buffer_size)
		start = time.time()
		next_log = start + 1
		frames_buffered = 0
		for _ in range(self.settings.stream.buffer_size):
			try:
				f = next(feed)
			except StopIteration:
				break
			buffer.append(f)
			frames_buffered += 1
			now = time.time()

			if now >= next_log:
				elapse = now - start
				self.logger.debug(f'Buffering... {frames_buffered} frames in {elapse:.2f} seconds')
				next_log = now + 1
		total_time = time.time() - start
		self.logger.debug(
			f'Buffer filled in {total_time:.2f} seconds with {frames_buffered} frames'
		)

		for frame in feed:
			buffer.append(frame)
			yield buffer.popleft()
		while buffer:
			yield buffer.popleft()
