from math import radians, tan

# Camera and FOV configuration
resolution_px = 1920
fov_deg = 120  # horizontal field of view in degrees
focal_distance_c = 3000  # in mm
focal_angle_acb = 60  # in degrees

# Compute fixed distance between cameras
focal_distance_ab = 2 * tan(radians(focal_angle_acb / 2)) * focal_distance_c

# Header
print(f'{"de (mm)":>10} | {"c′ (mm)":>10} | {"Pixel Overlap":>15} | {"% of Frame Width":>18}')
print('-' * 48)

# Iterate over range of c′ values from 3000 to 3000 mm
for c_prime in range(focal_distance_c, focal_distance_c * 5 + 1, 100):
	# Calculate overlap in mm
	focal_distance_de = (focal_distance_ab * c_prime) / focal_distance_c

	# Calculate total frame width at c′ in mm
	frame_width_mm = 2 * tan(radians(fov_deg / 2)) * c_prime

	# mm per pixel
	mm_per_pixel = frame_width_mm / resolution_px

	# Convert to pixels
	pixel_overlap = focal_distance_de / mm_per_pixel

	# Percentage of frame width
	percent_overlap = (pixel_overlap / resolution_px) * 100

	# Print results
	print(
		f'{focal_distance_de:10.2f} | {c_prime:10} | {int(pixel_overlap):15} | {percent_overlap:17.2f}%'
	)


def calculate_overlap_constant() -> int:
	return int(resolution_px * tan(radians(focal_angle_acb / 2)) / tan(radians(fov_deg / 2)))


constant_pixel_overlap = calculate_overlap_constant()
print(f'\nConstant Pixel Overlap: {constant_pixel_overlap} pixels')
