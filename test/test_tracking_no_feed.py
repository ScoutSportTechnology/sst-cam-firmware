import os
import random
import sys

import matplotlib.pyplot as plt
import pytest
from matplotlib.patches import Circle, Polygon, Rectangle

# Ensure src/app and src folders are on PYTHONPATH
project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
src_folder = os.path.join(project_root, 'src')
app_folder = os.path.join(src_folder, 'app')
sys.path.insert(0, app_folder)
sys.path.insert(0, src_folder)


from app.models.general import Court, Point, court
from app.models.tracking import (
	BallDetection,
	CourtDetection,
	DetectionData,
	MotionData,
	PlayerDetection,
)
from app.models.tracking.frame_side_decision import FrameSideDecision
from app.models.tracking.zoom import ZoomData
from app.services.video.tracker import (
	FrameSideDecisionService,
	MotionService,
	ZoomService,
)
from config.settings import Settings


def generate_random_detection_data(
	max_n_players: int = 11,
	max_player_offset: float = 200.0,
	court_jitter: float = 50,
	number_of_frames: int = 10,
) -> list[list[DetectionData]]:
	"""
	Returns [[L0…L(F−1)], [R0…R(F−1)]].
	Ensures that for each frame:
		- left.top_right.y ≃ right.top_left.y  (within small jitter)
		- left.bottom_right.y ≃ right.bottom_left.y
	"""

	fw, fh = Settings.camera.resolution
	overlap_px = Settings.fov.focal_pixel_overlap()
	left_start = fw - overlap_px
	right_end = overlap_px

	left_list: list[DetectionData] = []
	right_list: list[DetectionData] = []
	ball_location_regions: list[int] = []  # 0: left, 1: right, 2: both

	for _ in range(number_of_frames):
		# 1) global ball
		ball_location_region = random.choice([0, 1, 2])  # 0: left, 1: right, 2: both
		ball_location_regions.append(ball_location_region)
		

		match ball_location_region:
			case 0:  # ball in left camera
				bx_L = int(random.uniform(court_jitter, left_start))
				by_L = int(random.uniform(court_jitter, fh - court_jitter))
				conf_L = random.uniform(0.5, 1.0)
				bx_R, by_R, conf_R = 0, 0, 0

			case 1:  # ball in right camera
				bx_L, by_L, conf_L = 0, 0, 0
				bx_R = int(random.uniform(right_end, fw))
				by_R = int(random.uniform(court_jitter, fh - court_jitter))
				conf_R = random.uniform(0.5, 1.0)

			case _:  # ball in both cameras
				bx_L = int(random.uniform(left_start, fw))
				conf_L = random.uniform(0.5, 1.0)
				bx_R = int(random.uniform(court_jitter, right_end))
				conf_R = random.uniform(0.5, 1.0)
				y = int(random.uniform(court_jitter, fh- court_jitter))
				by_L, by_R = y, y

		ball_L = BallDetection(center=Point(bx_L, by_L), confidence=conf_L)
		ball_R = BallDetection(center=Point(bx_R, by_R), confidence=conf_R)

		# 2) players
		players_L: list[PlayerDetection] = []
		players_R: list[PlayerDetection] = []
		n_players_range: tuple[int, int] = (0, max_n_players)
		match ball_location_region:
			case 0:  # ball in left camera
				n_P_L = random.randint(*n_players_range)
				n_P_R = random.randint(0, max_n_players - n_P_L)
				pass
			case 1:  # ball in right camera
				n_P_R = random.randint(*n_players_range)
				n_P_L = random.randint(0, max_n_players - n_P_R)
			case _:  # ball in both cameras
				n_P_L = random.randint(*n_players_range)
				n_P_R = n_P_L
				pass

		for _ in range(n_P_L):
			px_L = min(
				max(int(bx_L + random.uniform(-max_player_offset, max_player_offset)), 0), fw
			)
			py_L = min(
				max(int(by_L + random.uniform(-max_player_offset, max_player_offset)), 0), fh
			)
			players_L.append(
				PlayerDetection(position=Point(px_L, py_L), confidence=random.uniform(0.5, 1.0))
			)

		for _ in range(n_P_R):
			px_R = min(
				max(int(bx_R + random.uniform(-max_player_offset, max_player_offset)), 0), fw
			)
			py_R = min(
				max(int(by_R + random.uniform(-max_player_offset, max_player_offset)), 0), fh
			)
			players_R.append(
				PlayerDetection(position=Point(px_R, py_R), confidence=random.uniform(0.5, 1.0))
			)

		# 3)  Y's
		y_top = random.uniform(0, court_jitter)
		y_bottom = random.uniform(fh - court_jitter, fh)
		y_jitter = random.uniform(0, court_jitter * 0.1)

		y_tr_L = int(y_top + random.uniform(-y_jitter, y_jitter))
		y_tl_R = int(y_top + random.uniform(-y_jitter, y_jitter))
		y_br_L = int(y_bottom + random.uniform(-y_jitter, y_jitter))
		y_bl_R = int(y_bottom + random.uniform(-y_jitter, y_jitter))

		# 4) left camera court
		court_L = Court(
			top_right=Point(int(random.uniform(fw - court_jitter, fw)), y_tr_L),
			bottom_right=Point(int(random.uniform(fw - court_jitter, fw)), y_br_L),
			top_left=Point(
				int(random.uniform(0, court_jitter)), int(random.uniform(0, court_jitter))
			),
			bottom_left=Point(
				int(random.uniform(0, court_jitter)), int(random.uniform(fh - court_jitter, fh))
			),
		)

		# 5) right camera court
		court_R = Court(
			top_left=Point(int(random.uniform(0, court_jitter)), y_tl_R),
			bottom_left=Point(int(random.uniform(0, court_jitter)), y_bl_R),
			top_right=Point(
				int(random.uniform(fw - court_jitter, fw)), int(random.uniform(0, court_jitter))
			),
			bottom_right=Point(
				int(random.uniform(fw - court_jitter, fw)),
				int(random.uniform(fh - court_jitter, fh)),
			),
		)

		det_L = DetectionData(
			ball=ball_L,
			players=list(players_L),
			court=CourtDetection(court=court_L, confidence=random.uniform(0.5, 1.0)),
		)
		det_R = DetectionData(
			ball=ball_R,
			players=list(players_R),
			court=CourtDetection(court=court_R, confidence=random.uniform(0.5, 1.0)),
		)

		left_list.append(det_L)
		right_list.append(det_R)

	print('\n')
	print(f'ball_location_region: {ball_location_regions}')

	return [left_list, right_list]


def generate_random_motion_data(
	detection_datas: list[list[DetectionData]],
) -> list[list[MotionData]]:
	"""
	Given a 2×F array of DetectionData and two MotionServices,
	returns a 2×F array of MotionData.
	"""
	motion_services: list[MotionService] = [MotionService() for _ in range(len(detection_datas))]
	return [
		[motion_services[side].calculate_motion(det) for det in detection_datas[side]]
		for side in range(len(motion_services))
	]


def generate_random_zoom_data(
	detection_datas: list[list[DetectionData]],
	motion_datas: list[list[MotionData]],
) -> list[list[ZoomData]]:
	"""
	Given a 2×F array of DetectionData and MotionData,
	returns a 2×F array of ZoomData.
	"""
	zoom_services: list[ZoomService] = [ZoomService() for _ in range(len(detection_datas))]
	return [
		[
			zoom_services[side].calculate_zoom(det, mot)
			for det, mot in zip(detection_datas[side], motion_datas[side], strict=False)
		]
		for side in range(len(zoom_services))
	]


def generate_random_frame_side_decision(
	motion_data: list[list[MotionData]],
) -> list[FrameSideDecision]:
	"""
	Compute a FrameSideDecision for each frame, pairing left/right MotionData.
	Returns a list of length = number_of_frames.
	"""
	frame_side_decision_service = FrameSideDecisionService()
	return [
		frame_side_decision_service.decide_side(left, right)
		for left, right in zip(motion_data[0], motion_data[1], strict=False)
	]


def log_data(
	detection_datas: list[list[DetectionData]],
	motion_datas: list[list[MotionData]],
	zoom_datas: list[list[ZoomData]],
	decisions: list[FrameSideDecision],
	detailed: bool = True,
) -> None:
	"""
	If detailed=True: print each frame side-by-side.
	Otherwise, print only the summary of all decisions.
	"""
	num_frames = len(decisions)

	if detailed:
		for f in range(num_frames):
			detL, detR = detection_datas[0][f], detection_datas[1][f]
			motL, motR = motion_datas[0][f], motion_datas[1][f]
			zomL, zomR = zoom_datas[0][f], zoom_datas[1][f]
			dec = decisions[f]

			print(f'\n=== Frame {f + 1} ===\n')

			# Detection
			print('Detection:')
			print(
				f'{"Entity":<12}{"L.X":>6}{"L.Y":>6}{"L.C":>6}   | {"R.X":>6}{"R.Y":>6}{"R.C":>6}'
			)
			# Ball
			print(
				f'{"Ball":<12}'
				f'{detL.ball.center.x:6d}{detL.ball.center.y:6d}{detL.ball.confidence:6.2f}   | '
				f'{detR.ball.center.x:6d}{detR.ball.center.y:6d}{detR.ball.confidence:6.2f}'
			)
			# Court corners
			corners = [
				('Court TL', detL.court.court.top_left, detR.court.court.top_left),
				('Court TR', detL.court.court.top_right, detR.court.court.top_right),
				('Court BL', detL.court.court.bottom_left, detR.court.court.bottom_left),
				('Court BR', detL.court.court.bottom_right, detR.court.court.bottom_right),
			]
			for label, pL, pR in corners:
				print(
					f'{label:<12}'
					f'{pL.x:6d}{pL.y:6d}{detL.court.confidence:6.2f}   | '
					f'{pR.x:6d}{pR.y:6d}{detR.court.confidence:6.2f}'
				)

			# Motion
			print('\nMotion:')
			print(f'{"Entity":<12}{"L.X":>12}{"L.Y":>12}   | {"R.X":>12}{"R.Y":>12}')
			for lbl, mL, mR in [
				('Pos', motL.position, motR.position),
				('Vel', motL.velocity, motR.velocity),
				('Acc', motL.acceleration, motR.acceleration),
			]:
				print(f'{lbl:<12}{int(mL.x):12d}{int(mL.y):12d}   | {int(mR.x):12d}{int(mR.y):12d}')
			print(f'{"Speed":<12}{motL.speed():24.2f}   | {motR.speed():12.2f}')
			print(f'{"AccMag":<12}{motL.acc():24.2f}   | {motR.acc():12.2f}')

			# Zoom
			print('\nZoom:')
			print(f'{"Entity":<12}{"L.Z":>6}   | {"R.Z":>6}')
			print(f'{"Zoom":<12}{zomL.zoom_level:6.2f}   | {zomR.zoom_level:6.2f}')

			# Side Decision
			print('\nSideDecision:')
			print(f'{"Side":<6}{"Conf":>8}')
			print(f'{dec.side.name:<6}{dec.confidence:8.2f}\n')

		print('\nAll frames processed successfully.\n')
		print(f'{"Frame":<6}{"Side":<6}{"Conf":>8}')
		for i, d in enumerate(decisions, start=1):
			print(f'{i:<6}{d.side.name:<6}{d.confidence:8.2f}')
		print()

	else:
		# final summary
		print('\nAll frames processed successfully.\n')
		print(f'{"Frame":<6}{"Side":<6}{"Conf":>8}')
		for i, d in enumerate(decisions, start=1):
			print(f'{i:<6}{d.side.name:<6}{d.confidence:8.2f}')
		print()


def graph_data(
	detection_datas: list[list[DetectionData]],
	motion_datas: list[list[MotionData]],
	zoom_datas: list[list[ZoomData]],
	output_dir: str = './test/output/test_tracking_no_feed',
) -> None:
	os.makedirs(output_dir, exist_ok=True)
	linewidth = 500
	fw, fh = Settings.camera.resolution
	overlap_px = Settings.fov.focal_pixel_overlap()
	left_start = fw - overlap_px
	right_end = overlap_px
	n_frames = len(detection_datas[0])

	for idx in range(n_frames):
		fig, axes = plt.subplots(
			1,
			2,
			figsize=(2 * fw, fh),  # inches == pixels when dpi=1
			dpi=1,
		)

		for cam_idx, ax in zip(range(len(detection_datas)), axes, strict=False):
			det = detection_datas[cam_idx][idx]
			mot = motion_datas[cam_idx][idx]
			zom = zoom_datas[cam_idx][idx]

			ax.set_xlim(0, fw)
			ax.set_ylim(0, fh)
			ax.axis('off')

			# black border
			ax.add_patch(
				Rectangle((0, 0), fw, fh, linewidth=linewidth, edgecolor='black', fill=False)
			)

			# red overlap region
			if cam_idx == 0:  # left camera
				ax.add_patch(
					Rectangle(
						(fw - overlap_px, 0),
						overlap_px,
						fh,
						linewidth=linewidth,
						edgecolor='blue',
						fill=False,
					)
				)
			else:  # right camera
				ax.add_patch(
					Rectangle(
						(0, 0), overlap_px, fh, linewidth=linewidth, edgecolor='red', fill=False
					)
				)

				# draw ball
				(x, y) = (float(det.ball.center.x), float(det.ball.center.y))
				ax.add_patch(Circle((x, y), radius=25, color='blue', fill=True))

			# draw players
			for player in det.players:
				(px, py) = (float(player.position.x), float(player.position.y))
				ax.add_patch(Circle((px, py), radius=25, color='green', fill=True))

			# draw court
			court = det.court.court
			ax.add_patch(
				Polygon(
					[
						(court.top_left.x, court.top_left.y),
						(court.top_right.x, court.top_right.y),
						(court.bottom_right.x, court.bottom_right.y),
						(court.bottom_left.x, court.bottom_left.y),
					],
					closed=True,
					fill=None,
					edgecolor='orange',
					linewidth=linewidth,
				)
			)

			# ...insert draw-ball, draw-players, draw-court, draw-motion, draw-zoom here...

		path = os.path.join(output_dir, f'frame_{idx + 1:03d}.png')
		print(f'Graphing frame {idx + 1} to {path}')
		fig.tight_layout(pad=0)
		fig.savefig(path, dpi=1)
		plt.close(fig)


def test_detection_data() -> None:
	# random.seed(42)
	detection_datas = generate_random_detection_data()
	motion_datas = generate_random_motion_data(detection_datas)
	zoom_datas = generate_random_zoom_data(detection_datas, motion_datas)
	decisions_data = generate_random_frame_side_decision(motion_datas)
	log_data(detection_datas, motion_datas, zoom_datas, decisions_data, detailed=True)
	graph_data(detection_datas, motion_datas, zoom_datas)
	assert True
