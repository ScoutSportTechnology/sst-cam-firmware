from enum import Enum


class CaptureFormat(Enum):
	RGB888 = ('RGB888', 'bgr24')
	BGR888 = ('BGR888', 'rgb24')
	YUV420 = ('YUV420', 'yuv420p')
	RG10 = ('RG10', 'gbrp10le')
