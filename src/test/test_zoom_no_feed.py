import matplotlib

matplotlib.use('Agg')

import random
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def compute_zoom_bbox(frame_width, frame_height, center_x, center_y, zoom):
	crop_w = max(16, int(frame_width * (1 - zoom)))
	crop_h = max(9, int(frame_height * (1 - zoom)))
	x1 = max(0, min(int(center_x - crop_w / 2), frame_width - crop_w))
	y1 = max(0, min(int(center_y - crop_h / 2), frame_height - crop_h))
	return x1, y1, crop_w, crop_h


# Parámetros
frame_width, frame_height = 1920, 1080
zoom_levels = [round(x, 2) for x in np.arange(0.0, 1.01, 0.1)]

# Directorio de salida
script_dir = Path(__file__).parent
output_dir = script_dir / 'outputs'
output_dir.mkdir(exist_ok=True)

# Generar 10 imágenes con coordenadas aleatorias
for i in range(1, 11):
	# Coordenadas aleatorias dentro del frame
	center_x = random.randint(0, frame_width)
	center_y = random.randint(0, frame_height)

	# Dibujo
	plt.figure(figsize=(10, 6))
	ax = plt.gca()
	ax.add_patch(plt.Rectangle((0, 0), frame_width, frame_height, fill=False, linewidth=2))

	for zoom in zoom_levels:
		x1, y1, w, h = compute_zoom_bbox(frame_width, frame_height, center_x, center_y, zoom)
		ax.add_patch(plt.Rectangle((x1, y1), w, h, fill=False, linewidth=1))
		ax.text(x1, y1, f'z={zoom}', fontsize=8)

	plt.xlim(-10, frame_width + 10)
	plt.ylim(frame_height + 10, -10)
	plt.title(f'Crop Boxes Random #{i} at center ({center_x}, {center_y})')
	plt.xlabel('X pixels')
	plt.ylabel('Y pixels')

	# Guardar imagen
	output_path = output_dir / f'crop_boxes_{i}_x_{center_x}_y_{center_y}.png'
	plt.savefig(output_path, dpi=150, bbox_inches='tight')
	plt.close()

print(f'10 imágenes generadas en: {output_dir.resolve()}')
