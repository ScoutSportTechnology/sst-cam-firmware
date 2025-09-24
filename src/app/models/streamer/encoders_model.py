from enum import Enum


class Encoders(Enum):
	RAW = 'rawvideo'
	H264 = 'H264'
	H265 = 'H265'
	MJPEG = 'MJPEG'
	VP8 = 'VP8'
	VP9 = 'VP9'
	AV1 = 'AV1'
