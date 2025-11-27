import cv2
import numpy as np

from app.models.capturer import Frame
from app.models.tracker import ZoomData


class VideoTransformationService:
	def __init__(self, target_size: tuple[int, int] = (1920, 1080)) -> None:
		self.target_size: tuple[int, int] = target_size

	def process(self, frames: Frame, zoom: ZoomData) -> Frame:
		frame_width, frame_height = frames.data.shape[1], frames.data.shape[0]
		crop_w: int = max(16, int(frame_width * (1 - zoom.zoom_level)))
		crop_h: int = max(9, int(frame_height * (1 - zoom.zoom_level)))
		x1 = int(zoom.center.x - crop_w / 2)
		y1 = int(zoom.center.y - crop_h / 2)
		x1: int = max(0, min(x1, frame_width - crop_w))
		y1: int = max(0, min(y1, frame_height - crop_h))
		x2: int = x1 + crop_w
		y2: int = y1 + crop_h

		cropped = frames.data[y1:y2, x1:x2]
		resized = cv2.resize(cropped, self.target_size, interpolation=cv2.INTER_AREA)
		return Frame(
			data=resized,
			timestamp=frames.timestamp,
		)
