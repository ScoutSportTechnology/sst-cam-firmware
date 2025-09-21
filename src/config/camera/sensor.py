from dataclasses import dataclass

from app.interfaces.config.camera.sensor import ISensor
from config.camera.sensors import IMX477, IMX708
from config.device import DeviceSettings


@dataclass()
class Sensor(ISensor):
	def __getattr__(self, name):
		_device = DeviceSettings().device
		match _device:
			case 'jetson':
				return getattr(IMX477(), name)
			case 'raspberrypi':
				return getattr(IMX708(), name)
			case _:
				raise ValueError(f'Unsupported device: {_device}')
