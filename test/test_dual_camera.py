import os
import sys

import cv2
import numpy as np

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))
from app.adapters.hardware.camera.picamera import Picamera2Adapter


def test_dual_camera():
	window_size = (2560, 720)
	window_name = 'Dual Camera Feed'

	cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
	cv2.resizeWindow(window_name, window_size[0], window_size[1])
	
	print('Starting dual camera test...')

	print('Initializing cameras...')
	print('Camera 0: Picamera2Adapter(0)')
	cam0 = Picamera2Adapter(0)
	print('Camera 1: Picamera2Adapter(1)')
	cam1 = Picamera2Adapter(1)
	print('Cameras initialized')

	print('Starting cameras...')
	print('Starting Camera 0...')
	cam0.start()
	print('Starting Camera 1...')
	cam1.start()
	print('Cameras started')

	frame_counter = 0

	try:
		while True:
			frame_counter += 1

			cam0.get_frame()
			cam1.get_frame()

			print('Capturing frame...')
			frame0 = cam0.get_frame().data
			print(f"Captured frame {frame_counter} from camera 0")
			frame1 = cam1.get_frame().data
			print(f"Captured frame {frame_counter} from camera 1")
			print('Captured frames')

			print(f'Combining frame {frame_counter} from both cameras...')
			combined = cv2.hconcat((frame0, frame1))
			print('Frames combined')
			
			print(f' Resizing combined frame {frame_counter}')
			resized = cv2.resize(combined, window_size)

			print('Displaying frames')
			cv2.imshow(window_name, resized)
			print('Frames displayed')

			if cv2.waitKey(1) & 0xFF == ord('q'):
				break
	finally:
		cam0.stop()
		cam1.stop()
		cv2.destroyAllWindows()


if __name__ == '__main__':
	test_dual_camera()
