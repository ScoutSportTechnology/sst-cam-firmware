from app.models.tracker import DetectionData
from app.models.tracker.boundaries import Point
from app.models.tracker.motion_data import MotionData
from config.settings import Settings


class MotionService:
	def __init__(
		self,
	):
		self.detection_data_old: DetectionData | None = None
		self.motion_data_old: MotionData | None = None
		self.dt: float = 1 / Settings.camera.fps

	def calculate_motion(
		self,
		detection_data: DetectionData,
	) -> MotionData:
		position: Point = detection_data.ball.center
		x: int = int(position.x)
		y: int = int(position.y)
		if self.detection_data_old is None:
			x_old, y_old = x, y
		else:
			position_old: Point = self.detection_data_old.ball.center
			x_old: int = int(position_old.x)
			y_old: int = int(position_old.y)

		if self.motion_data_old is None:
			vx = vy = ax = ay = 0.0
		else:
			vx_old: float = self.motion_data_old.velocity.x
			vy_old: float = self.motion_data_old.velocity.y
			vx: float = (x - x_old) / self.dt
			vy: float = (y - y_old) / self.dt
			ax: float = (vx - vx_old) / self.dt
			ay: float = (vy - vy_old) / self.dt
		motion = MotionData(position=position, velocity=Point(vx, vy), acceleration=Point(ax, ay))
		self.detection_data_old = detection_data
		self.motion_data_old = motion
		return motion
