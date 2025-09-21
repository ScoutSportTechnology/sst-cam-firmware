from app.interfaces.config.camera.sensor import ISensor, ISensorFormat, ISensorMode
from app.models.config.camera import CaptureFormat


class _IMX477Modes(ISensorMode):
	MODE_4032x3040_20 = ((2304, 1296), 56, False)
	MODE_3840x2160_30 = ((2304, 1296), 30, False)
	MODE_1920x1080_60 = ((1536, 864), 120, True)


class _IMX477Formats(ISensorFormat):
	RG10 = CaptureFormat.RG10.value


class IMX477(ISensor):
	def __init__(self, format=_IMX477Formats.RG10, mode=_IMX477Modes.MODE_3840x2160_30) -> None:
		super().__init__(format, mode)
