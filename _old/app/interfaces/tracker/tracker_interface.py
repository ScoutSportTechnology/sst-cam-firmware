from abc import ABC, abstractmethod

from app.models.capturer import Frame
from app.models.tracker import DetectionData


class ITracker(ABC):
	@abstractmethod
	def start(self) -> None: ...

	@abstractmethod
	def stop(self) -> None: ...

	@abstractmethod
	def reset(self) -> None: ...

	@abstractmethod
	def detect(self, frame: Frame) -> DetectionData: ...
