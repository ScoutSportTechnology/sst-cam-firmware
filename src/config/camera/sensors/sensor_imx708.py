from app.interfaces.capturer import ISensor, ISensorFormat, ISensorMode
from config.camera import CaptureFormat


class _IMX708Modes(ISensorMode):
	MODE_2304x1296_56 = ((2304, 1296), 56, False)
	MODE_2304x1296_30 = ((2304, 1296), 30, False)
	MODE_1536x864_120 = ((1536, 864), 120, True)


class _IMX708Formats(ISensorFormat):
	RGB888 = CaptureFormat.RGB888.value
	BGR888 = CaptureFormat.BGR888.value
	YUV420 = CaptureFormat.YUV420.value


class IMX708(ISensor):
	def __init__(self, format=_IMX708Formats.RGB888, mode=_IMX708Modes.MODE_2304x1296_56) -> None:
		super().__init__(format, mode)
