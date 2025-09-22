import time
from collections.abc import Generator

from app.interfaces.capturer.ivideo import IVideoService

from app.infra.logger import Logger
from app.interfaces.capturer import ICamera
from app.models.capturer import Frame
from app.models.tracker import DetectionData, MotionData, SideDecisionData, ZoomData
from app.services.bufferer import BufferService
from app.services.processor import (
	VideoPostProcessorService,
	VideoPreProcessorService,
	VideoTransformationService,
)
from app.services.tracker import MotionService, SideDecisionService, ZoomService


class VideoService(IVideoService):
	def __init__(self, cam0: ICamera, cam1: ICamera) -> None:
		self.cam0 = cam0
		self.cam1 = cam1
		self.active = False
		self.motion_service = MotionService()
		self.zoom_service = ZoomService()
		self.side_decider = SideDecisionService()
		self.logger = Logger(name='video_service')

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

	def status(self) -> bool:
		return self.active

	def focus(self) -> None:
		self.cam0.focus()
		self.cam1.focus()

	def _get_frames(self) -> None:
		self.frame0 = self.cam0.frame()
		self.frame1 = self.cam1.frame()

	def _preprocess(self) -> list[Frame]:
		self.frame0 = VideoPreProcessorService().process(self.frame0)
		self.frame1 = VideoPreProcessorService().process(self.frame1)
		return [self.frame0, self.frame1]

	def _postprocess(self) -> Frame:
		self.frame = VideoPostProcessorService().process(self.frame0, self.frame1)
		return self.frame

	def _track(self) -> None:
		detection_data_0: DetectionData | None = None  # i havent done this yet
		detection_data_1: DetectionData | None = None

		if detection_data_0 is not None and detection_data_1 is not None:
			motion_data_0: MotionData = self.motion_service.calculate_motion(detection_data_0)
			motion_data_1: MotionData = self.motion_service.calculate_motion(detection_data_1)

			side: SideDecisionData = self.side_decider.decide_side(motion_data_0, motion_data_1)
			if side == SideDecisionData.side.LEFT:
				self.motion_data: MotionData = motion_data_0
				self.detection_data: DetectionData = detection_data_0
				self.frame = self.frame0
			else:
				self.motion_data: MotionData = motion_data_1
				self.detection_data: DetectionData = detection_data_1
				self.frame = self.frame1

			self.zoom_data: ZoomData = self.zoom_service.calculate_zoom(self.detection_data, self.motion_data)

	def _transform(self) -> None:
		self.frame = VideoTransformationService().process(self.frame, self.zoom_data)

	def frames(self) -> Generator[Frame, None, None]:
		while self.active:
			self._get_frames()
			self._preprocess()
			# self._track()
			# self._transform()
			# self._overlay()
			self._postprocess()
			yield self.frame

	def feed(self) -> Generator[Frame, None, None]:
		while self.active:
			raw_feed = self.frames()
			buffered_frames = BufferService().feed(raw_feed)
			yield from buffered_frames
