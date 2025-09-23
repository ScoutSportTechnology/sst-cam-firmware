import subprocess


def get_device() -> str:
	try:
		result = subprocess.check_output(['cat', '/sys/firmware/devicetree/base/model'], text=True)
		model = result.strip('\x00\n').lower()
	except (subprocess.CalledProcessError, FileNotFoundError):
		model = 'unknown'

	if 'jetson' in model:
		return 'jetson'
	elif 'raspberry' in model:
		return 'raspberrypi'
	else:
		raise ValueError(f'Unsupported device: {model}')
