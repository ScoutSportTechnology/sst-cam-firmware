from app.infra.logger import Logger
from app.interfaces.capturer.camera import ICamera
from app.models.capturer import Frame


class ArducamIX708RPICamera(ICamera):
	def __init__(self) -> None:
		self.__logger = Logger(name='arducam_imx708_rpi_adapter')

	def init(self) -> None:
		self.__logger.info('Initializing Arducam IMX708 RPI Camera')

	def start(self) -> None:
		self.__logger.info('Starting Arducam IMX708 RPI Camera')

	def stop(self) -> None:
		self.__logger.info('Stopping Arducam IMX708 RPI Camera')

	def status(self) -> bool:
		self.__logger.info('Checking status of Arducam IMX708 RPI Camera')
		return self.__active

	def focus(self) -> None:
		self.__logger.info('Focusing Arducam IMX708 RPI Camera')

	def capture(self) -> Frame:
		self.__logger.info('Capturing frame from Arducam IMX708 RPI Camera')


	def __capture(self) -> None:
		self.__logger.info('Internal capture method for Arducam IMX708 RPI Camera')

	def restart(self) -> None:
		self.__logger.info('Restarting Arducam IMX708 RPI Camera')
		self.stop()
		self.start()
		
ArducamIX708RPICamera(camera_index=1,format='BGR')
