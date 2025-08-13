from dataclasses import dataclass

from app.models.tracker.detection import BallDetection, CourtDetection, PlayerDetection


@dataclass
class DetectionData:
	ball: BallDetection
	court: CourtDetection
	players: list[PlayerDetection]
