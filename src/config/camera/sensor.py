from enum import Enum

from app.interfaces.capturer import ISensor, ISensorMode
from config.camera.sensors import IMX477, IMX708
from config.device import DeviceSettings


class SensorEnum(ISensor):
	IMX477 = IMX477()
	IMX708 = IMX708()


class Sensor:
	def __init__(self) -> None:
		self._device = DeviceSettings().device

	@property
	def sensor(self) -> ISensor:
		match self._device:
			case 'jetson':
				return SensorEnum.IMX477
			case 'raspberrypi':
				return SensorEnum.IMX708
			case _:
				raise ValueError(f'Unsupported device: {self._device}')

	@property
	def sensor_mode(self) -> ISensorMode:
		return self.sensor.mode

	@property
	def sensor_resolution(self) -> tuple[int, int]:
		return self.sensor_mode.resolution

	@property
	def sensor_fps(self) -> int:
		return self.sensor_mode.fps

	@property
	def sensor_hdr(self) -> bool:
		return self.sensor_mode.hdr

	@property
	def sensor_format(self) -> str:
		return self.sensor.format.sensor_format

	@property
	def ffmpeg_format(self) -> str:
		return self.sensor.format.ffmpeg_format
