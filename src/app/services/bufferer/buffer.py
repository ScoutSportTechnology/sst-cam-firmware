import threading
import time
from collections import deque
from collections.abc import Generator, Iterable
from typing import Optional

from app.infra.logger import Logger
from app.interfaces.bufferer import IBufferService
from app.models.capturer import Frame
from config.settings import Settings


class BufferService(IBufferService):
	def __init__(self) -> None:
		self.settings = Settings
		self.logger = Logger(name='buffer_service')

		self.buffer_size: int = self.settings.stream.buffer_size
		self.buffer_deque: deque[Frame] = deque(maxlen=self.buffer_size)

		self.fps: float = float(self.settings.stream.fps)
		self._interval: float = 1.0 / self.fps if self.fps > 0 else 0.0

		self._active = False
		self._producer_thread: threading.Thread | None = None
		self._producer_exc: BaseException | None = None
		self._feed: Iterable[Frame] | None = None
		self._lock = threading.Lock()
		self._new_frame = threading.Condition(self._lock)

		# Nuevo: mantener el último frame para repetirlo si no llega uno nuevo
		self._last_frame: Frame | None = None

	def start(self) -> None:
		self._active = True

	def stop(self) -> None:
		self._active = False
		with self._new_frame:
			self._new_frame.notify_all()
		if self._producer_thread and self._producer_thread.is_alive():
			self._producer_thread.join(timeout=1.0)
		self._producer_thread = None

	def _producer(self) -> None:
		try:
			assert self._feed is not None
			for frame in self._feed:
				if not self._active:
					break
				with self._new_frame:
					self.buffer_deque.append(frame)  # maxlen descarta los más viejos
					self._new_frame.notify()
		except BaseException as exc:
			self._producer_exc = exc
			with self._new_frame:
				self._new_frame.notify_all()
		finally:
			with self._new_frame:
				self._active = False
				self._new_frame.notify_all()

	def buffer(self, feed: Generator[Frame, None, None]) -> Generator[Frame, None, None]:
		"""
		Yields at target FPS. Si no hay frame nuevo a tiempo,
		repite el último frame emitido para mantener continuidad visual.
		"""
		self._feed = feed
		self.start()
		self._producer_thread = threading.Thread(
			target=self._producer, name='buffer_producer', daemon=True
		)
		self._producer_thread.start()

		next_deadline = time.perf_counter()

		try:
			while self._active:
				now = time.perf_counter()
				if self._interval > 0:
					sleep_for = next_deadline - now
					if sleep_for > 0:
						time.sleep(sleep_for)
					next_deadline += self._interval

				if self._producer_exc is not None:
					raise self._producer_exc

				emitted = False
				with self._new_frame:
					if not self.buffer_deque and self._active:
						self._new_frame.wait(timeout=self._interval if self._interval > 0 else 0.01)

					if self.buffer_deque:
						latest = self.buffer_deque[-1]
						self.buffer_deque.clear()
						self._last_frame = latest
						yield latest
						emitted = True
					else:
						if self._last_frame is not None and (self._active or not self._active):
							yield self._last_frame
							emitted = True

				if not emitted and not self._active:
					break
		finally:
			self.stop()
