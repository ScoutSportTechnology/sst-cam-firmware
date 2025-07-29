from app.interfaces.camera import ICamera
from app.models.frame import Frame
from app.services.video import processor,streaming


class VideoService:
	def __init__(self, cam0: ICamera, cam1: ICamera) -> None:
		self.cam0 = cam0
		self.cam1 = cam1

		self.cam0.start()
		self.cam1.start()

		self.frame0 = self.cam0.get_frame()
		self.frame1 = self.cam1.get_frame()

		self.frame: Frame = self.frame0

	def calibrate(self) -> None:
		# Placeholder for calibration logic
		pass

	def overlay(self) -> None:
		# Placeholder for overlay logic
		pass

	def preprocess(self) -> None:
		self.frame0 = processor.VideoPreProcessorService().process(self.frame0)
		self.frame1 = processor.VideoPreProcessorService().process(self.frame1)

	def postprocess(self) -> None:
		self.frame = processor.VideoPostProcessorService().process(self.frame0, self.frame1)

	def store(self) -> None:
		# Placeholder for storage logic
		pass

	def stream(self) -> None:
		stream_service = streaming.StreamService('.jpg', self.frame)
		stream_service.start()

	def track(self) -> None:
		# Placeholder for tracking logic
		pass
