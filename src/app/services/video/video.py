from collections.abc import Generator

from app.interfaces.camera import ICamera
from app.models.frame import Frame
from app.models.tracking import DetectionData, FrameSideDecision, MotionData, ZoomData
from app.services.video import overlay, processor, tracker


class VideoService:
	def __init__(self, cam0: ICamera, cam1: ICamera) -> None:
		self.cam0 = cam0
		self.cam1 = cam1

		self.active = False

		self.motion_service = tracker.MotionService()
		self.zoom_service = tracker.ZoomService()
		self.side_decider = tracker.FrameSideDecisionService()

	def start(self) -> None:
		if not self.active:
			self.cam0.start()
			self.cam1.start()
			self.active = True

	def stop(self) -> None:
		if self.active:
			self.cam0.stop()
			self.cam1.stop()
			self.active = False

	def status(self) -> str:
		return 'active' if self.active else 'inactive'

	def focus(self) -> None:
		self.cam0.focus()
		self.cam1.focus()

	def get_frames(self) -> None:
		self.frame0 = self.cam0.get_frame()
		self.frame1 = self.cam1.get_frame()

	def calibrate(self) -> None:
		# Placeholder for calibration logic
		pass

	# def overlay(self) -> None:
	# Placeholder for overlay logic
	# pass

	def preprocess(self) -> list[Frame]:
		self.frame0 = processor.VideoPreProcessorService().process(self.frame0)
		self.frame1 = processor.VideoPreProcessorService().process(self.frame1)
		return [self.frame0, self.frame1]

	def postprocess(self) -> Frame:
		self.frame = processor.VideoPostProcessorService().process(self.frame0, self.frame1)
		return self.frame

	def track(self) -> None:
		detection_data_0: DetectionData | None = None  # i havent done this yet
		detection_data_1: DetectionData | None = None

		if detection_data_0 is not None and detection_data_1 is not None:
			motion_data_0: MotionData = self.motion_service.calculate_motion(detection_data_0)
			motion_data_1: MotionData = self.motion_service.calculate_motion(detection_data_1)

			side: FrameSideDecision = self.side_decider.decide_side(motion_data_0, motion_data_1)
			if side == FrameSideDecision.side.LEFT:
				self.motion_data: MotionData = motion_data_0
				self.detection_data: DetectionData = detection_data_0
				self.frame = self.frame0
			else:
				self.motion_data: MotionData = motion_data_1
				self.detection_data: DetectionData = detection_data_1
				self.frame = self.frame1

			self.zoom_data: ZoomData = self.zoom_service.calculate_zoom(
				self.detection_data, self.motion_data
			)

	def transform(self) -> None:
		self.frame = processor.VideoTransformationService().process(self.frame, self.zoom_data)

	def frames(self) -> Generator[Frame, None, None]:
		while self.active:
			self.get_frames()
			# self.calibrate()
			self.preprocess()
			# self.track()
			# self.transform()
			# self.overlay()
			self.postprocess()
			yield self.frame
