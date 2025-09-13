from enum import Enum

from app.interfaces.capturer import ISensor

from .sensor_imx477 import IMX477
from .sensor_imx708 import IMX708


class Sensors(Enum):
	IMX477 = IMX477
	IMX708 = IMX708
