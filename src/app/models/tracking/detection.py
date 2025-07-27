from dataclasses import dataclass

from app.models.general.court import Court
from app.models.general.point import Point


@dataclass
class BallDetection:
	center: Point
	confidence: float


@dataclass
class CourtDetection:
	court: Court
	confidence: float


@dataclass
class PlayerDetection:
	position: Point
	confidence: float

	def distance_to_ball(self, ball: BallDetection) -> int:
		distance = (ball.center.x - self.position.x) ** 2 + (ball.center.y - self.position.y) ** 2
		return int(distance**0.5)


@dataclass
class DetectionData:
	ball: BallDetection
	court: CourtDetection
	players: list[PlayerDetection]
