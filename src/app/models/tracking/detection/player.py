from dataclasses import dataclass

from .ball import BallDetection
from src.app.models.tracking.boundaries import Point


@dataclass
class PlayerDetection:
    position: Point
    confidence: float

    def distance_to_ball(self, ball: BallDetection) -> int:
        distance = int(
            (
                (ball.center.x - self.position.x) ** 2
                + (ball.center.y - self.position.y) ** 2
            )
            ** 0.5
        )
        return distance