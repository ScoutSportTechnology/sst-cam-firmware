from app.adapters.capturer.camera.lense import LENSE_120
from app.infra.logger import Logger
from app.interfaces.capturer.camera import ICamera
from app.models.capturer.formats import NVArgusCameraSrcFormats, NVVidConvFormats, VideoConvertFormats

from .sensor import IMX477


class ArducamB0273Camera(ICamera):
	def __init__(self, camera_index: int, flip_method: int) -> None:
		sensor = IMX477
		logger = Logger(name='arducam_B0273_adapter')
		lense = LENSE_120
		super().__init__(
			camera_index=camera_index,
			flip_method=flip_method,
			logger=logger,
			sensor=sensor,
			lense=lense,
		)

	def _pipeline(self) -> str:
		return (
			f'nvarguscamerasrc sensor-id={self._camera_index} ! '
			f'video/x-raw(memory:NVMM), '
			f'width={self._capture_width}, height={self._capture_height}, '
			f'format={NVArgusCameraSrcFormats.NV12.value}, framerate={self._framerate}/1 ! '
			f'nvvidconv flip-method={self._flip_method} ! '
			f'video/x-raw, format={NVVidConvFormats.NV12.value} ! '
			f'videoconvert ! '
			f'video/x-raw, format={VideoConvertFormats.BGR.value} ! '
			f'appsink drop=1 max-buffers=1'
		)

	def focus(self) -> None:
		self._logger.info('Focus method not implemented for Arducam B0273 camera')
