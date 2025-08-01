from enum import Enum


class StreamProvider(Enum):
	"""
	Enum representing the service provider for video streaming.
	"""

	RTMP = 'rtmp'
	RTSP = 'rtsp'
	HTTP = 'http'
	FFMPEG = 'ffmpeg'

	def __str__(self) -> str:
		return self.value
