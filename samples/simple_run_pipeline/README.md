# Simple Pipeline #

Simple pipeline is a sample of how to create an application without configuration files. The configuration is defined in the code describes a graph that has three modules, DataSource, Inferencer and Osd. And it encapsulates CNStream pipeline into a class ``SimplePipelineRunner`` which multiple inherits from ``StreamMsgObserver`` and ``IModuleObserver`` . Therefore we could receive message from pipeline by override ``Update()`` . Besides this class is set to Osd module as an observer, so each  ``CNFrameInfoPtr`` that contains all information of a frame will be sent through ``notify()`` callback function after processed by Osd.



The following step is done in simple pipeline:

- Build pipeline in ``SimplePipelineRunner`` constructor.
- Start pipeline and add resources to pipeline in ``SimplePipelineRunner::Start()`` function.
- Receive results from Osd module by ``SimplePipelineRunner::notify()`` callback function. And save the results.
- Receive message from pipeline by ``SimplePipelineRunner::Update()`` callback function.
- ``SimplePipelineRunner::WaitPipelineDone()`` will block util all EOS (end of stream) messages are received. And then stop pipeline.



Some parameters of the executable file ``${CNSTREAM_DIR}/samples/bin/simple_run_pipeline`` :

- input_url: The video path or image urls, for example: ``/path/to/your/video.mp4, /path/to/your/images/%d.jpg`` .
- how_to_show: [image] means dump as images, [video] means dump as video (output.avi), [display] means show in a window. The default value is [display].
- output_dir: Where to store the output file. The default value is "./".
- output_frame_rate: The output frame rate, valid when [how_to_show] set to [video] or [display]. The default value is 25.
- label_path: The label path.
- model_path: The Cambricon offline model path.
- func_name:  The function name in your model. The default value is [subnet0].
- keep_aspect_ratio: Whether keep aspect ratio for image scaling in model's preprocessing. The default value is false.
- mean_value: The mean values in format "100.0, 100.0, 100.0" in BGR order. The default value is "0, 0, 0".
- std: The std values in format "100.0, 100.0, 100.0", in BGR order. The default value is "1.0, 1.0, 1.0".
- model_input_pixel_format: The input image format for your model. BGRA/RGBA/ARGB/ABGR/RGB/BGR is supported. The default value is BGRA.
- dev_id: The device ordinal index. The default value is 0.



For more details, please check [source code](./simple_run_pipeline.cpp).



**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/simple_run_pipeline
# Usages: run.sh [mlu220/mlu270]
./run.sh mlu220  # For MLU220 platform
./run.sh mlu270  # For MLU270 platform
```
