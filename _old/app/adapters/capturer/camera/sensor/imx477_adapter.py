from enum import Enum

from app.models.capturer import CaptureFormat
from app.models.capturer.camera import Sensor


class __IMX477Modes(Enum):
	MODE_4032x3040_20 = ((4032, 3040), 21, False)
	MODE_3840x2160_30 = ((3840, 2160), 30, False)
	MODE_1920x1080_60 = ((1920, 1080), 60, True)


class __IMX477Formats(Enum):
	RG10 = CaptureFormat.RG10


IMX477 = Sensor(format=__IMX477Formats.RG10.value, mode=__IMX477Modes.MODE_3840x2160_30.value)
