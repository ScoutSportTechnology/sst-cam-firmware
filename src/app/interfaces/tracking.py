from abc import ABC, abstractmethod

from app.models.frame import Frame
from app.models.tracking.detection import DetectionData


class ITracker(ABC):
	@abstractmethod
	def start(self) -> None:
		pass

	@abstractmethod
	def stop(self) -> None:
		pass

	@abstractmethod
	def reset(self) -> None:
		pass

	@abstractmethod
	def detect(self, frame: Frame) -> DetectionData:
		pass
