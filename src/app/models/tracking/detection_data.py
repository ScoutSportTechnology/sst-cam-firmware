from dataclasses import dataclass

from src.app.models.tracking.detection import BallDetection, CourtDetection, PlayerDetection


@dataclass
class DetectionData:
    ball: BallDetection
    court: CourtDetection
    players: list[PlayerDetection]