import cv2
import numpy as np

from app.models.frame import Frame
from config.settings import Settings


class VideoPreProcessorService:
	def __init__(self) -> None:
		self.settings = Settings

	def process(self, frame: Frame) -> Frame:
		# resized = cv2.resize(frame.data, (1920, 1080), interpolation=cv2.INTER_LINEAR)
		# colored = cv2.cvtColor(frame.data, cv2.COLOR_RGB2BGR)
		data = frame.data
		timestamp = frame.timestamp
		reduced_noise = cv2.bilateralFilter(data, d=5, sigmaColor=50, sigmaSpace=50)
		processed_frame = reduced_noise
		return Frame(data=processed_frame, timestamp=timestamp)
