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
	) -> Generator[Frame, None, None]:
		fps = self.settings.stream.fps
		interval =  (1.0 / fps)
		next_send = time.perf_counter()
		feed_done = False
		while (not feed_done) or self.buffer:
				now = time.perf_counter()
				to_sleep = next_send - now
				if to_sleep > 0:
						time.sleep(to_sleep)
				next_send += interval
				if self.buffer:
						yield self.buffer.popleft()
				if not feed_done:
						try:
								self.buffer.append(next(feed))
						except StopIteration:
								feed_done = True