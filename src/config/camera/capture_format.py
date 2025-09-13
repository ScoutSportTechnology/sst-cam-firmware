from enum import Enum


class CaptureFormat(Enum):
	RGB = ('RGB888', 'bgr24')
	BGR = ('BGR888', 'rgb24')
	YUV = ('YUV420', 'yuv420p')

	@property
	def sensor_format(self) -> str:
		return self.value[0]

	@property
	def ffmpeg_format(self) -> str:
		return self.value[1]
