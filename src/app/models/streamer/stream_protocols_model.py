from enum import Enum


class StreamProtocol(Enum):
    RTMP = "rtmp"
    RTSP = "rtsp"
    HTTP = "http"
    FFMPEG = "ffmpeg"

    def __str__(self) -> str:
        return super().__str__()