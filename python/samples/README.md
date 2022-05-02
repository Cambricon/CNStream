# Cambricon CNStream Python API Samples

Two application samples are provided. ``${CNSTREAM_DIR}`` represents CNStream source directory.

- pycnstream_demo.py, represents how to build and start a pipeline, add and remove resources and so on.
- yolov3_detector.py, provides a demonstration of how to customize preprocessing and postprocessing in python and set it to the Inferencer module.

Before run the samples, please make sure CNStream Python API is compiled and installed. And the requirements are meet. Execute the following commands to install the dependencies.

```sh
cd {CNSTREAM_DIR}/python
pip install -r requirement.txt
```

## Pycnstream Demo

The source code is ``{CNSTREAM_DIR}/python/samples/pycnstream_demo.py`` .

The configuration file is ``{CNSTREAM_DIR}/python/samples/python_demo_config.json`` .

**How to run:**

```sh
cd {CNSTREAM_DIR}/python/samples/
python pycnstream_demo.py
```

## Yolov3 Detector

The source code is ``{CNSTREAM_DIR}/python/samples/yolov3_detector.py`` .

The configuration file is ``{CNSTREAM_DIR}/python/samples/yolov3_detection_config.json`` .

**How to run:**

```sh
cd {CNSTREAM_DIR}/python/samples/
python yolov3_detector.py
```

