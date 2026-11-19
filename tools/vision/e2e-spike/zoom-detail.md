# Virtual-camera crop detail (geometric bound, not image-quality measurement)

Assume both sensor modes have the same field of view and center, and each view fills a 1920×1080 output. At virtual zoom `z`, the crop has `source_width/z × source_height/z` source samples, then scales to 1920×1080. 4K therefore provides twice the linear source detail (four times the samples) of 1080p at the same zoom. This does **not** measure real sharpness, optics, ISP, codec or seam quality.

| Virtual zoom | 4K capture crop | 1080p capture crop | 1080p native samples per output pixel | 4K native samples per output pixel |
| --- | ---: | ---: | ---: | ---: |
| 1.0× | 3840×2160 | 1920×1080 | 1.00 | 4.00 |
| 1.5× | 2560×1440 | 1280×720 | 0.44 | 1.78 |
| 2.0× | 1920×1080 | 960×540 | 0.25 | 1.00 |
| 3.0× | 1280×720 | 640×360 | 0.11 | 0.44 |

At a **representative assumed 2× crop**, 1080p capture enlarges 960×540 to 1920×1080: each source pixel supplies four output pixels (75% of the output pixels are interpolation, not independent detail). 4K capture provides native 1920×1080 for that crop. In a seam spanning cameras, this calculation applies per camera only where the output samples that camera. A real typical zoom distribution and matching sensor FOV have not been measured; revisit once calibrated framing and representative footage exist.
