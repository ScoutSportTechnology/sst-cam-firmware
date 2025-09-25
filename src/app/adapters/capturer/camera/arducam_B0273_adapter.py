import gi

gi.require_version('Gst', '1.0')

from gi.repository import Gst

from app.infra.logger import Logger
from app.interfaces.capturer.camera import ICamera
from app.models.capturer import Frame


class ArducamB0273Camera(ICamera):
	def __init__(self) -> None:
		self._logger = Logger(name='arducam_B0273_adapter')

	def init(self) -> None:
		self._logger.info('Arducam B0273 camera initialized')

	def start(self) -> None:
		self._active = True
		self._logger.info('Arducam B0273 camera started')

	def stop(self) -> None:
		self._active = False
		self._logger.info('Arducam B0273 camera stopped')

	def focus(self) -> None:
		self._logger.info('Focus method not implemented for Arducam B0273 camera')
