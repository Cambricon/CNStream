# Customization

Common preprocessing and postprocessing methods, custom module, object filter and Kafka handler are provided. Users could use them directly or use them as a reference.

## Preprocessing

Preprocessing methods are stored at directory ``${CNSTREAM_DIR/samples/common/preprocess/`` . The following preprocessing methods are provided:

- Standard (resize and convert color)
- Yolov3
- Yolov5
- Lprnet

## Postprocessing

Postprocessing methods are stored at directory ``${CNSTREAM_DIR/samples/common/postprocess/`` . The following postprocessing methods are provided:

- Classification
- SSD
- Yolov3
- Yolov5
- Mobilenet
- Vehicle CTS
- Lprnet

## Custom Module

A custom module ``PoseOsd`` is provided as an example. It shows how to create a custom module. The source code is at ``samples/common/cns_openpose/pose_osd_module.cpp``

Basically the steps are:

- Define a class inherits from ``Module`` and ``ModuleCreater`` class. Specialize the template class ``ModuleCreater`` with your custom class.
- Override ``Open()`` , ``Close()`` and ``Process()`` functions. Load resources in ``Open()``  . Clear resources in ``Close()`` . And most important, write your processing code in ``Process()`` function.

## Object Filter

Several object filters are provided at ``${CNSTREAM_DIR/samples/common/obj_filter`` . Object filters are useful when we want to filter out objects that we are not interested in. We could do the secondary inference on the objects we are interested in only.

To define a object filter, do the following steps:

- Define a class inherits from ``ObjFilter`` class.
- Override ``Filter()`` function. Return false to filter out the object, otherwise return true.

## Frame Filter

Although there is no example for custom a frame filter, but it is still useful and it is as easy as object filter. The difference between them is a frame filter could filter out frame and all the objects on it. Filtering out frame does not mean the frame is dropped, it means we do not do inference on it.

To define a frame filter, do the following steps:

- Define a class inherits from ``FrameFilter`` class.
- Override ``Filter()`` function. Return false to filter out the frame, otherwise return true.

##  Kafka Handler

We provide a Kafka handler at ``${CNSTREAM_DIR/samples/common/kafka_handler`` as an example for users who use Kafka module and want to produce CNFrameInfo data using RdKafka.

To define a Kafka handler, do the following steps:

- Define a class inherits from ``KafkaHandler`` class.
- Override ``UpdateFrame()`` callback function. Make all information you needed to a json string and produce the Kafka message.
