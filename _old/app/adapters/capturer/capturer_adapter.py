from app.interfaces.capturer.camera import ICamera
from config.device import get_device


def CapturerAdapter(camera_index: int, flip_method: int) -> ICamera:
	_device = get_device()
	match _device:
		case 'jetson':
			from app.adapters.capturer.camera import ArducamB0273Camera

			return ArducamB0273Camera(camera_index, flip_method)
		case 'raspberrypi':
			from app.adapters.capturer.camera import ArducamB0311Camera

			return ArducamB0311Camera(camera_index, flip_method)
		case _:
			raise ValueError(f'Unsupported device: {_device}')
