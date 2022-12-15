# Simple Pipeline #

Simple pipeline is a sample of how to create an application without configuration files. The configuration is defined in the code describes a graph that has three modules, DataSource, Inferencer and Osd. And it encapsulates CNStream pipeline into a class ``SimplePipelineRunner`` which multiple inherits from ``Pipeline`` , ``StreamMsgObserver`` and ``IModuleObserver`` . Therefore we could receive message from pipeline by override ``Update()`` . Besides this class is set to Osd module as an observer, so each  ``CNFrameInfoPtr`` that contains all information of a frame will be sent through ``Notify()`` callback function after processed by Osd.



The following step is done in simple pipeline:

- Build pipeline in ``SimplePipelineRunner`` constructor.
- Start pipeline in ``SimplePipelineRunner::Start()`` function.
- Add resources to pipeline by ``SimplePipelineRunner::AddStream()`` function.
- Receive results from Osd module by ``SimplePipelineRunner::Notify()`` callback function. And save the results.
- Receive message from pipeline by ``SimplePipelineRunner::Update()`` callback function.
- ``SimplePipelineRunner::WaitPipelineDone()`` will block util all EOS (end of stream) messages are received. And then stop pipeline.



Some parameters of the executable file ``${CNSTREAM_DIR}/samples/bin/simple_run_pipeline`` :

- input_url: The video path or image urls, for example: ``/path/to/your/video.mp4, /path/to/your/images/%d.jpg`` .
- input_num: The input number indicates repeatedly add the input_url to the pipeline.
- how_to_show: [image] means dump as images, [video] means dump as video (output.avi). The default value is [video]. Otherwise, the result will not be shown.
- output_dir: Where to store the output file. The default value is "./".
- output_frame_rate: The output frame rate, valid when [how_to_show] set to [video]. The default value is 25.
- label_path: The label path.
- model_path: The MagicMind offline model path.
- keep_aspect_ratio: Whether keep aspect ratio for image scaling in model's preprocessing. The default value is false.
- pad_value: The pad values in format "128, 128, 128" in model input pixel format order. The default value is "114, 114, 114".
- mean_value: The mean values in format "100.0, 100.0, 100.0" in model input pixel format order. The default value is "0, 0, 0".
- std: The std values in format "100.0, 100.0, 100.0", in model input pixel format order. The default value is "1.0, 1.0, 1.0".
- model_input_pixel_format: The input image format for your model. RGB/BGR is supported. The default value is RGB.
- dev_id: The device ordinal index. The default value is 0.



For more details, please check [source code](./simple_run_pipeline.cpp).



**How to run:**

```sh
cd ${CNSTREAM_DIR}/samples/simple_run_pipeline
# Usages: run.sh [ml370/ce3226]
./run.sh mlu370  # For mlu370 platform
./run.sh ce3226  # For ce3226 platform
```
