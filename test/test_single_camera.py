import os
import sys

import cv2
import numpy as np

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))
from app.adapters.hardware.camera.picamera import Picamera2Adapter


def test_single_camera():
	window_size = (1280, 720)
	window_name = 'Single Camera Feed'

	cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
	cv2.resizeWindow(window_name, window_size[0], window_size[1])

	cam = Picamera2Adapter(0)
	cam.start()

	try:
		while True:
			#img = np.zeros((window_size[1], window_size[0], 3), dtype=np.uint8)
			frame = cam.get_frame()
			img = frame.data
			cv2.imshow(window_name, img)
			if cv2.waitKey(1) & 0xFF == ord('q'):
				break

	finally:
		cam.stop()
		cv2.destroyAllWindows()


if __name__ == '__main__':
	test_single_camera()
