from collections.abc import Callable

import cv2

from app.interfaces import ICamera, IStream
from app.models import Frame


class StreamService(IStream):
	def __init__(
		self,
		image_format: str,
		camera_adapter: Callable[[int], ICamera],
		camera_indices: tuple[int, int] = (0, 1),
	):
		self.image_format = image_format
		self.camera_adapter = camera_adapter
		self.camera_indices = camera_indices

		self.active = False
		self.cam_0: ICamera | None = None
		self.cam_1: ICamera | None = None

	def start(self):
		self.active = True

		self.cam_0 = self.camera_adapter(self.camera_indices[0])
		self.cam_1 = self.camera_adapter(self.camera_indices[1])

		self.cam_0.start()
		self.cam_1.start()

	def stop(self):
		self.active = False
		if self.cam_0:
			self.cam_0.stop()
		if self.cam_1:
			self.cam_1.stop()

	def get_status(self) -> str:
		return 'active' if self.active else 'inactive'

	def get_feed(self) -> list[bytes]:
		if not self.active:
			raise RuntimeError('Stream is not active. Please start the stream first.')
		if not self.cam_0 or not self.cam_1:
			raise RuntimeError('Cameras are not initialized. Please start the stream first.')

		frame_0 = self.cam_0.get_frame()
		data_0, timestamp_0 = frame_0.data, frame_0.timestamp
		success_0, buffer_0 = cv2.imencode(self.image_format, frame_0.data)
		if not success_0:
			raise RuntimeError("Failed to encode camera 0 frame")
		encoded_0 = buffer_0.tobytes()

		frame_1 = self.cam_1.get_frame()
		data_1, timestamp_1 = frame_1.data, frame_1.timestamp
		success_1, buffer_1 = cv2.imencode(self.image_format, data_1)
		if not success_1:
			raise RuntimeError("Failed to encode camera 1 frame")
		encoded_1 = buffer_1.tobytes()

		combined_data = cv2.hconcat((data_0, data_1))
		combined_timestamp = max(timestamp_0, timestamp_1)
		combined = Frame(data=combined_data, timestamp=combined_timestamp)
		success_combined, buffer_combined = cv2.imencode(self.image_format, combined.data)
		if not success_combined:
			raise RuntimeError("Failed to encode combined frame")
		encoded_combined = buffer_combined.tobytes()

		return [encoded_0, encoded_1, encoded_combined]
