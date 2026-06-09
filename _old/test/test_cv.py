import os
import sys

import cv2
import numpy as np

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))
from app.adapters.hardware.camera.picamera import Picamera2Adapter


def test_cv():
	window_size = (1280, 720)
	window_name = 'Test Preview'

	cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
	cv2.resizeWindow(window_name, window_size[0], window_size[1])
	cv2.moveWindow(window_name, 100, 100)

	cam = Picamera2Adapter(0)
	cam.start()

	while True:
		img = np.zeros((window_size[1], window_size[0], 3), dtype=np.uint8)
		frame = cam.get_frame()
		img = frame.data
		cv2.putText(
			img, window_name, (600, 270), cv2.FONT_HERSHEY_SIMPLEX, 2, (255, 255, 255), 3
		)
		cv2.imshow(window_name, img)
		if cv2.waitKey(30) & 0xFF == ord('q'):
			break
	cv2.destroyAllWindows()


if __name__ == '__main__':
	test_cv()
