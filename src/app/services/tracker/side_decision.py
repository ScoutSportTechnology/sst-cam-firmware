from app.models.general import Sides
from app.models.tracker.motion_data import MotionData
from app.models.tracker.side_decision_data import SideDecisionData
from config.settings import Settings


def _sign(x: float) -> float:
	return 1.0 if x >= 0.0 else -1.0


class SideDecisionService:
	def __init__(self):
		self.frame_width = Settings.camera.resolution[0]
		self.overlap = Settings.fov.focal_pixel_overlap()
		self.max_speed = 2000
		self.max_acc = 5000
		self.weight_speed = 0.33
		self.weight_acc = 0.33
		self.weight_overlap = 0.33

	def decide_side(
		self, motion_data_left_frame: MotionData, motion_data_right_frame: MotionData
	) -> SideDecisionData:
		left_frame_limit = self.frame_width - self.overlap
		right_frame_limit = self.overlap
		x_left = motion_data_left_frame.position.x
		vx_left = motion_data_left_frame.velocity.x
		ax_left = motion_data_left_frame.acceleration.x
		x_right = motion_data_right_frame.position.x
		vx_right = motion_data_right_frame.velocity.x
		ax_right = motion_data_right_frame.acceleration.x
		if x_left < left_frame_limit:
			decision = SideDecisionData(side=Sides.LEFT, confidence=1.0)
		elif x_right > right_frame_limit:
			decision = SideDecisionData(side=Sides.RIGHT, confidence=1.0)
		else:
			v_diff = vx_right - vx_left
			v_norm = min(abs(v_diff) / self.max_speed, 1.0)
			v_factor = _sign(v_diff) * v_norm
			a_diff = ax_right - ax_left
			a_norm = min(abs(a_diff) / self.max_acc, 1.0)
			a_factor = _sign(a_diff) * a_norm

			dist_left_outer = x_left - left_frame_limit
			dist_left_norm = min(dist_left_outer / self.overlap, 1.0)
			dist_right_outer = right_frame_limit - x_right
			dist_right_norm = min(dist_right_outer / self.overlap, 1.0)
			d_factor = dist_right_norm - dist_left_norm

			combined = (
				self.weight_speed * v_factor
				+ self.weight_acc * a_factor
				+ self.weight_overlap * d_factor
			)
			combined = max(-1, min(combined, 1))
			side = Sides.RIGHT if combined >= 0 else Sides.LEFT
			confidence = abs(combined)
			decision = SideDecisionData(side=side, confidence=confidence)

		return decision
