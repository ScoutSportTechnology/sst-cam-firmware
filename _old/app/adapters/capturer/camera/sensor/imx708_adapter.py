from enum import Enum

from app.models.capturer import CaptureFormat
from app.models.capturer.camera import Sensor


class _IMX708Modes(Enum):
	MODE_2304x1296_56 = ((2304, 1296), 56, False)
	MODE_2304x1296_30 = ((2304, 1296), 30, False)
	MODE_1536x864_120 = ((1536, 864), 120, True)


class _IMX708Formats(Enum):
	RGB888 = CaptureFormat.RGB888
	BGR888 = CaptureFormat.BGR888
	YUV420 = CaptureFormat.YUV420


IMX708 = Sensor(format=_IMX708Formats.RGB888.value, mode=_IMX708Modes.MODE_2304x1296_30.value)
