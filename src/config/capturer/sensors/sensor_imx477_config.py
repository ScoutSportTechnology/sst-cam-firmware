from enum import Enum

from app.models.config.capturer import CaptureFormat

from .sensor_config import Sensor


class __IMX477Modes(Enum):
	MODE_4032x3040_20 = ((2304, 1296), 56, False)
	MODE_3840x2160_30 = ((2304, 1296), 30, False)
	MODE_1920x1080_60 = ((1536, 864), 120, True)


class __IMX477Formats(Enum):
	RG10 = CaptureFormat.RG10.value


IMX477 = Sensor(format=__IMX477Formats.RG10.value, mode=__IMX477Modes.MODE_3840x2160_30.value)
