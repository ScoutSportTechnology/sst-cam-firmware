from enum import Enum


class EncodeType(Enum):
	RAW = 'rawvideo'
	H264 = 'H264'
	H265 = 'H265'
	MJPEG = 'MJPEG'
	JPEG = 'JPEG'
	AV1 = 'AV1'
	HEVC = 'HEVC'
	VP8 = 'VP8'
	VP9 = 'VP9'
	FLV = 'flv'
	BGR24 = 'bgr24'

	def __str__(self) -> str:
		return self.value
