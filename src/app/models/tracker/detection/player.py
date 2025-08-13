from dataclasses import dataclass

from app.models.tracker.boundaries import Point

from .ball import BallDetection


@dataclass
class PlayerDetection:
	position: Point
	confidence: float

	def distance_to_ball(self, ball: BallDetection) -> int:
		distance = int(
			((ball.center.x - self.position.x) ** 2 + (ball.center.y - self.position.y) ** 2) ** 0.5
		)
		return distance
