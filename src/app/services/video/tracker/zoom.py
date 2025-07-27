from models.tracking.detection import DetectionData
from models.tracking.motion import MotionData
from models.tracking.zoom import ZoomData


class ZoomService:
	def __init__(self):
		self.zoom_level_old: float | None = None
		self.near_ball_threshold: int = 100
		self.max_speed: float = 2000
		self.max_acc: float = 5000
		self.weight_speed: float = 0.33
		self.weight_acc: float = 0.33
		self.weight_players_count: float = 0.33
		self.smooth_factor: float = 0.7

	def calculate_zoom(self, detection_data: DetectionData, motion_data: MotionData) -> ZoomData:
		ball = detection_data.ball
		speed = motion_data.speed()
		acc = motion_data.acc()
		players = detection_data.players
		players_count_near_ball = sum(
			1 for p in players if p.distance_to_ball(ball) < self.near_ball_threshold
		)
		players_count_in_frame = len(players)
		norm_speed = min(speed / self.max_speed, 1)
		norm_acc = min(abs(acc) / self.max_acc, 1)
		norm_player_count = (
			min(players_count_near_ball / players_count_in_frame, 1)
			if players_count_in_frame
			else 0.0
		)
		zoom_level_raw = (
			self.weight_speed * norm_speed
			+ self.weight_acc * norm_acc
			+ self.weight_players_count * norm_player_count
		)
		zoom_level = max(0, min(zoom_level_raw, 1))
		if self.zoom_level_old is not None:
			zoom_level = (
				self.smooth_factor * zoom_level + (1 - self.smooth_factor) * self.zoom_level_old
			)
		zoom = ZoomData(center=ball.center, zoom_level=zoom_level)
		self.zoom_level_old = zoom_level
		return zoom
