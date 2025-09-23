from app.interfaces.config.camera.sensor import ISensor
from config.device import get_device


def __get_sensor() -> ISensor:
	__device = get_device()
	match __device:
		case 'jetson':
			from config.capturer.sensors import IMX477

			return IMX477
		case 'raspberrypi':
			from config.capturer.sensors import IMX708

			return IMX708
		case _:
			raise ValueError(f'Unsupported device: {__device}')


class Sensor(ISensor):
	@property
	def resolution(self) -> tuple[int, int]:
		return self.__mode[0]

	@property
	def fps(self) -> int:
		return self.__mode[1]

	@property
	def hdr(self) -> bool:
		return self.__mode[2]

	@property
	def capture_format(self) -> str:
		return self.__format[0]

	@property
	def stream_format(self) -> str:
		return self.__format[1]
