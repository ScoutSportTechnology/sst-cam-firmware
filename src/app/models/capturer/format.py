from enum import Enum


class CaptureFormat(Enum):
    XBGR = "XBGR888"
    XRGB = "XRGB888"
    BGR = "BGR888"
    RGB = "RGB888"
    YUV420 = "YUV420"

    def __str__(self) -> str:
        return super().__str__()