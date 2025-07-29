import cv2
import numpy as np

from app.models.frame import Frame
from config.settings import Settings


class VideoPreProcessorService:
	def __init__(self) -> None:
		self.settings = Settings

	def process(self, frame: Frame) -> Frame:
		# resized = cv2.resize(frame.data, (1920, 1080), interpolation=cv2.INTER_LINEAR)
		colored = cv2.cvtColor(frame.data, cv2.COLOR_RGB2BGR)
		processed_frame = colored
		return Frame(data=processed_frame, timestamp=frame.timestamp)
