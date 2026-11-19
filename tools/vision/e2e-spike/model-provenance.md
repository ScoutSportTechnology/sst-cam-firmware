# RF-DETR Small model provenance (spike only)

- Source: https://repo.roboflow.com/rfdetr/small_coco/checkpoint_best_regular.pth; RF-DETR code/weights Apache-2.0, https://github.com/roboflow/rf-detr .
- Exporter: `rfdetr[onnx]==1.12.1`, `torch==2.9.1+cpu`, `torchvision==0.24.1+cpu` in `/root/setup/vision/.venv`; no GPU on export host.
- Input weight `/root/setup/vision/rfdetr-models/rf-detr-small.pth`: MD5 `fb37061c1af7bace359c91b723a8d5c1` (matches upstream registry).
- Export command: `RF_HOME=/root/setup/vision/rfdetr-models /root/setup/vision/.venv/bin/python -c 'from rfdetr import RFDETRSmall; print(RFDETRSmall().export(format="onnx", batch_size=2, output_dir="/root/setup/vision/rfdetr-export", verbose=False))'`.
- ONNX `/root/setup/vision/rfdetr-export/rfdetr-small.onnx`: SHA-256 `2151b499cfddc5803ffd1fd29370afb3f8e40311bad390d514b4c8ba66016dbd`; `onnx.checker.check_model` passed. Input `input` float32 `[2,3,512,512]`; outputs `dets` float32 `[2,300,4]` normalized cxcywh boxes and `labels` float32 `[2,300,91]` logits.
- Exporter emitted shape-dependent tracer warnings for a **fixed 512×512, batch 2** graph. FP16 TensorRT plan must be built on the actual Jetson and validated there, not cross-compiled. Neither ONNX validation nor published T4 latency proves Jetson performance or model accuracy on sports footage.
- The checkpoint and ONNX are large binary inputs stored outside git; the spike code and raw run logs belong in this directory, but do not commit pretrained weights without a separate artifact/distribution decision.
