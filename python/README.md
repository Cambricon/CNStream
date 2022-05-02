# Cambricon CNStream Python API #
CNStream provides Python API gives python programmers access to some of the CNStream C++ API. It helps python programmers develop application code based on CNStream. Python API is based on pybind11 and depends on Python (3+).



CNStream Python API mainly supports the following behaviors:

- Build pipelines with built-in modules.

- Start and stop pipelines.

- Add resources to DataSource module and remove resources while the pipeline is running.

- Get Stream Message from pipeline.

- Get CNFrameInfoPtr after it is processed by a certain module or all modules in the pipeline.

- Write custom preprocessing and post processing and set it to Inferencer or Inferencer2 modules.

- Write custom module and insert it to pipelines.

- Get performance information.

  

## Getting started ##

CNStream Python API is not compiled and installed by default. To compile and install Python API package, execute the following commands. ``${CNSTREAM_DIR}`` represents CNStream source directory.

```sh
cd {CNSTREAM_DIR}/python
python setup.py install
```

 After that, run the following line by python interpreter to check if the package is installed successfully:

```python
import cnstream
```


## Samples ##

Two application samples are provided, located at ``{CNSTREAM_DIR}/python/samples`` .

- pycnstream_demo.py, represents how to build and start a pipeline, add and remove resources and so on.
- yolov3_detector.py, provides a demonstration of how to customize preprocessing and postprocessing and set it to the Inferencer module.

Before run the samples, please install the dependencies by execute the following commands:

```sh
cd {CNSTREAM_DIR}/python
pip install -r requirement.txt
```

## Documentation ##

[Cambricon Forum Docs Python API](https://www.cambricon.com/docs/cnstream/user_guide_html/python/python.html)

Check out this page for more details on how to use CNStream Python API. 


