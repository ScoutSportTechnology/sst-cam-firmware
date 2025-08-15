import threading
import time
from collections import deque
from collections.abc import Generator, Iterable

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

		# Mantener el último frame para repetirlo si no llega uno nuevo
		self._last_frame: Frame | None = None

		# Métricas
		self._dropped_total = 0

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
					# Si está llena, el append descarta el más viejo -> contamos drop
					if len(self.buffer_deque) == self.buffer_deque.maxlen:
						self._dropped_total += 1
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
		Emite a FPS objetivo. Si no hay frame nuevo a tiempo,
		repite el último frame emitido para mantener continuidad visual.
		Loguea NEW/REPEAT/SKIP y resumen por segundo.
		"""
		self._feed = feed
		self.start()
		self._producer_thread = threading.Thread(
			target=self._producer, name='buffer_producer', daemon=True
		)
		self._producer_thread.start()

		next_deadline = time.perf_counter()

		# Acumuladores de métricas por segundo
		new_count = 0
		repeat_count = 0
		skip_count = 0
		last_report = time.perf_counter()

		try:
			while True:
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
						qlen = len(self.buffer_deque)
						latest = self.buffer_deque[-1]
						self.buffer_deque.clear()
						self._last_frame = latest

						self.logger.debug(
							f'bufferer: EMIT=NEW ts={latest.timestamp:.6f} '
							f'queue_before_clear={qlen} drops_total={self._dropped_total}'
						)
						new_count += 1
						yield latest
						emitted = True

					else:
						# Repite solo si seguimos activos y ya hubo al menos un frame
						if self._active and self._last_frame is not None:
							self.logger.debug(
								f'bufferer: EMIT=REPEAT ts={self._last_frame.timestamp:.6f} '
								f'queue=0 drops_total={self._dropped_total}'
							)
							repeat_count += 1
							yield self._last_frame
							emitted = True
						elif self._active:
							# Activo pero aún no hay frame -> saltamos este tick
							self.logger.debug(
								f'bufferer: EMIT=SKIP (no frame yet) queue=0 drops_total={self._dropped_total}'
							)
							skip_count += 1
							emitted = False
						else:
							# No activo y sin frames -> terminar
							emitted = False

				# Reporte agregado por segundo
				now = time.perf_counter()
				if now - last_report >= 1.0:
					total = new_count + repeat_count + skip_count
					self.logger.debug(
						f'bufferer: 1s summary -> total_ticks={total}, new={new_count}, '
						f'repeat={repeat_count}, skip={skip_count}, '
						f'drops_total={self._dropped_total}, deque_len={len(self.buffer_deque)}'
					)
					new_count = repeat_count = skip_count = 0
					last_report = now

				# Condición de salida
				if not emitted and not self._active and not self.buffer_deque:
					break

		finally:
			self.stop()
