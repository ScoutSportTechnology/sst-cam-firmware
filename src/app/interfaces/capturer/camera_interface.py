from abc import ABC, abstractmethod

from app.models.capturer.format_model import CaptureFormat
from app.models.capturer.frame_model import Frame


class ICamera(ABC):
	@abstractmethod
	def __init__(
		self, camera_index: int, resolution: tuple[int, int], format: CaptureFormat, fps: int
	) -> None: ...

	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def get_frame(self) -> Frame: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def restart(self) -> None: ...

	@abstractmethod
	def status(self) -> bool: ...

	@abstractmethod
	def _capture_loop(self) -> None: ...

	@abstractmethod
	def focus(self) -> None: ...
