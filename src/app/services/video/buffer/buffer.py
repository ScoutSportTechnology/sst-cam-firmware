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
		for _ in range(self.buffer_size):
			try:
				frame = next(feed)
			except StopIteration:
				break
			self.buffer.append(frame)

			fps = self.settings.stream.fps
			interval = 1.0 / fps
			next_send = time.time()

			while feed or self.buffer:
				try:
					frame = next(feed)
					self.buffer.append(frame)
				except StopIteration:
					self.logger.debug('Feed exhausted, waiting for more frames...')
					frame = None
				now = time.time()
				to_sleep = next_send - now
				if to_sleep > 0:
					time.sleep(to_sleep)
				next_send += interval
				if self.buffer:
					yield self.buffer.popleft()
				else:
					self.logger.debug('Buffer empty, waiting for frames...')
					continue
