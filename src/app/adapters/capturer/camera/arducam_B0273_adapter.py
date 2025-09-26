import gi

gi.require_version('Gst', '1.0')  # type: ignore
from gi.repository import Gst  # type: ignore

from app.infra.logger import Logger
from app.interfaces.capturer.camera import ICamera
from app.models.capturer import Frame

from .sensor import IMX477


class ArducamB0273Camera(ICamera):
	def __init__(self) -> None:
		self._logger = Logger(name='arducam_B0273_adapter')
		self._sensor = IMX477

	def init(self) -> None:
		self._logger.info('Arducam B0273 camera initialized')
		_flip_method = 0
		_framerate = self._sensor.fps
		_capture_width = self._sensor.resolution[0]
		_capture_height = self._sensor.resolution[1]
		_format = self._sensor.format
		Gst.init()

	def start(self) -> None:
		self._active = True
		self._logger.info('Arducam B0273 camera started')

	def stop(self) -> None:
		self._active = False
		self._logger.info('Arducam B0273 camera stopped')

	def focus(self) -> None:
		self._logger.info('Focus method not implemented for Arducam B0273 camera')
