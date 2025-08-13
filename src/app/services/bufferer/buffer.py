import threading
import time
from collections import deque
from collections.abc import Generator
from typing import Any, Optional

from app.models.capturer import Frame
from app.services.logger import Logger
from config.settings import Settings

ALPHA = 0.2  # EMA para fps de entrada


class BufferService:
	def __init__(self) -> None:
		self.settings = Settings
		self.logger = Logger(name='buffer_service')
		self.buffer_size = self.settings.stream.buffer_size
		self.buffer: deque[Frame] = deque(maxlen=self.buffer_size)
		self._producer_thread: threading.Thread | None = None
		self._stop = threading.Event()
		self._in_fps_ema: float | None = None

	def _producer(self, feed: Generator[Frame, None, None]) -> None:
		last_ts = None
		while not self._stop.is_set():
			try:
				frame = next(feed)  # bloquea hasta que haya frame
			except StopIteration:
				break
			# Actualiza EMA de FPS de entrada
			if hasattr(frame, 'timestamp'):
				if last_ts is not None:
					dt = max(1e-6, frame.timestamp - last_ts)
					inst_fps = 1.0 / dt
					self._in_fps_ema = (
						inst_fps
						if self._in_fps_ema is None
						else (ALPHA * inst_fps + (1 - ALPHA) * self._in_fps_ema)
					)
				last_ts = frame.timestamp
			self.buffer.append(frame)
		# Señal de fin
		self._stop.set()

	def feed(self, feed: Generator[Frame, None, None]) -> Generator[Frame, None, None]:
		target_fps = self.settings.stream.fps
		interval = 1.0 / target_fps
		low_wm = max(1, int(0.25 * self.buffer_size))  # low watermark
		high_wm = max(low_wm + 1, int(0.75 * self.buffer_size))  # high watermark

		# Arranca productor
		self._stop.clear()
		self._producer_thread = threading.Thread(target=self._producer, args=(feed,), daemon=True)
		self._producer_thread.start()

		next_send = time.perf_counter()
		last_out: Frame | None = None

		try:
			while not self._stop.is_set() or self.buffer:
				now = time.perf_counter()
				to_sleep = next_send - now
				if to_sleep > 0:
					time.sleep(to_sleep)
				next_send += interval  # ritmo fijo de salida

				# Control por watermarks
				size = len(self.buffer)
				if size >= high_wm:
					# Salto de frames: quedarse con el más reciente (drena rápido)
					# Vacía todo menos el último para cortar latencia
					while len(self.buffer) > 1:
						self.buffer.pop()  # descarta antiguos desde el final es O(1) pero queremos el último => mejor popleft rápido
					# Nota: si prefiere, reemplace por una estrategia: popleft hasta 1 elemento
					# while len(self.buffer) > 1: self.buffer.popleft()

				if self.buffer:
					last_out = self.buffer.popleft()
					yield last_out
				else:
					# Buffer seco: mantener el último frame para conservar FPS constante
					if last_out is not None:
						yield last_out
					else:
						# Aún no hay nada: espere al siguiente ciclo
						continue

				# (Opcional) micro-ajuste del intervalo según EMA de entrada para reducir deriva
				if self._in_fps_ema is not None:
					# Si la cámara viene claramente más lenta, suavice el intervalo para no acumular "holds" visibles
					# limite ±10% del target para no romper la constancia
					desired = 1.0 / max(1.0, min(10_000.0, self._in_fps_ema))
					interval = max(0.9 * (1.0 / target_fps), min(1.1 * (1.0 / target_fps), desired))
		finally:
			self._stop.set()
			if self._producer_thread:
				self._producer_thread.join(timeout=1.0)
