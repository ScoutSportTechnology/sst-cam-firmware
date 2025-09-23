from enum import Enum

from app.models.config.capturer import CaptureFormat

from .sensor_config import Sensor


class _IMX708Modes(Enum):
	MODE_2304x1296_56 = ((2304, 1296), 56, False)
	MODE_2304x1296_30 = ((2304, 1296), 30, False)
	MODE_1536x864_120 = ((1536, 864), 120, True)


class _IMX708Formats(Enum):
	RGB888 = CaptureFormat.RGB888.value
	BGR888 = CaptureFormat.BGR888.value
	YUV420 = CaptureFormat.YUV420.value


IMX708 = Sensor(format=_IMX708Formats.RGB888.value, mode=_IMX708Modes.MODE_2304x1296_30.value)