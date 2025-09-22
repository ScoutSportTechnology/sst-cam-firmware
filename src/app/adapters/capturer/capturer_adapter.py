from app.interfaces.capturer import ICamera
from config.device import DeviceSettings


def CapturerAdapter(camera_index: int) -> ICamera:
	_device = DeviceSettings().device
	match _device:
		case 'jetson':
			from app.adapters.capturer.gstreamer_adapter import GStreamerAdapter

			return GStreamerAdapter(camera_index)
		case 'raspberrypi':
			from app.adapters.capturer.picamera_adapter import Picamera2Adapter

			return Picamera2Adapter(camera_index)
		case _:
			raise ValueError(f'Unsupported device: {_device}')
