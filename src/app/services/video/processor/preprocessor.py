import cv2
import numpy as np

from app.models.frame import Frame
from config.settings import Settings


class VideoPreProcessorService:
	def __init__(self) -> None:
		self.settings = Settings

	def process(self, frame: Frame) -> Frame:
		timestamp = frame.timestamp
		data = frame.data
		resized = cv2.resize(
			data,
			(self.settings.stream.resolution[0], self.settings.stream.resolution[1]),
			interpolation=cv2.INTER_AREA,
		)
		size = (5, 5)
		reduced_blur = cv2.GaussianBlur(
			resized, ksize=size, sigmaX=0, borderType=cv2.BORDER_DEFAULT
		)
		amount = 0.18
		sharpen = cv2.addWeighted(resized, 1 + amount, reduced_blur, -amount, 0)
		processed_frame = sharpen
		return Frame(data=processed_frame, timestamp=timestamp)
