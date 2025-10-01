import cv2 as cv

from app.adapters.capturer import CapturerAdapter


def main() -> None:
	camera_0 = CapturerAdapter(0, 0)
	camera_1 = CapturerAdapter(1, 0)

	camera_0.start()
	camera_1.start()

	while True:
		frame_0 = camera_0.capture()
		frame_1 = camera_1.capture()

		cv.imshow('Camera 0', frame_0.data)
		cv.imshow('Camera 1', frame_1.data)
		if cv.waitKey(1) & 0xFF == ord('q'):
			break


if __name__ == '__main__':
	main()
