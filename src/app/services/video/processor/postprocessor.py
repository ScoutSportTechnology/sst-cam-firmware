import time
from math import ceil

import cv2
import numpy as np

from app.models.frame import Frame
from app.services.logger.loger import Logger
from config.settings import Settings


class VideoPostProcessorService:
	def __init__(self) -> None:
		self.settings = Settings
		self._last_frame_time = 0
		self._target_fps = 60
		self._target_frame_duration = 1.0 / self._target_fps
		self.logger = Logger(name='video_post_processor_service')

	def process(self, frame_0: Frame, frame_1: Frame) -> Frame:
		now = time.time()
		elapsed = now - self._last_frame_time

		if elapsed < self._target_frame_duration:
			time.sleep(self._target_frame_duration - elapsed)

		self._last_frame_time = time.time()
		frame_0_data = frame_0.data
		frame_1_data = frame_1.data

		# frame_data = cv2.hconcat((frame_0_data, frame_1_data))
		frame_data = frame_0_data
		frame_data = cv2.resize(
			frame_data,
			(
				self.settings.stream.resolution[0],
				ceil(self.settings.stream.resolution[1]),
			),
			interpolation=cv2.INTER_LINEAR,
		)
		if self.settings.camera.pix_fmt == 'h264':
			frame_data = cv2.cvtColor(frame_data, cv2.COLOR_BGR2YUV_I420)

		return Frame(data=frame_data, timestamp=self._last_frame_time)
