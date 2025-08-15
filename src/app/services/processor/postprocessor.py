import cv2
import numpy as np

from app.infra.logger.loger import Logger
from app.models.capturer import Frame
from config.settings import Settings


class VideoPostProcessorService:
	def __init__(self) -> None:
		self.settings = Settings
		self.logger = Logger(name='video_post_processor_service')

	def process(self, frame_0: Frame, frame_1: Frame) -> Frame:
		frame_0_data = frame_0.data
		frame_1_data = frame_1.data
		frame_data = frame_1_data  # cv2.hconcat((frame_0_data, frame_1_data))
		return Frame(data=frame_data, timestamp=frame_0.timestamp)
