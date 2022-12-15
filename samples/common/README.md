# Customization

Common preprocessing and postprocessing methods, custom module, object filter osd handler and kafka handler are provided. Users could use them directly or use them as a reference.

### Preprocessing

Preprocessing methods are stored at directory ``${CNSTREAM_DIR/samples/common/preprocess/`` . The following preprocessing methods are provided:

- Classification
- Yolov3
- Yolov5
- Lprnet
- SSDLpd

### Postprocessing

Postprocessing methods are stored at directory ``${CNSTREAM_DIR/samples/common/postprocess/`` . The following postprocessing methods are provided:

- Classification
- Yolov3
- Yolov5
- Lprnet
- SSDLpd

### Custom Module

A custom module ``PoseOsd`` is provided as an example. It shows how to create a custom module. The source code is at ``samples/common/cns_openpose/pose_osd_module.cpp``

Basically the steps are:

- Define a class inherits from ``Module`` and ``ModuleCreater`` class. Specialize the template class ``ModuleCreater`` with your custom class.
- Override ``Open()`` , ``Close()`` and ``Process()`` functions. Load resources in ``Open()``  . Clear resources in ``Close()`` . And most important, write your processing code in ``Process()`` function.

### Object Filter

Several object filters are provided at ``${CNSTREAM_DIR/samples/common/filter`` . Object filters are useful when we want to filter out objects that we are not interested in. We could do the secondary inference on the objects we are interested in only.

To define a object filter, do the following steps:

- Define a class inherits from ``ObjectFilterVideo`` class.
- Override ``Filter()`` function. Return false to filter out the object, otherwise return true.

###  Osd Handler

A Osd handler for vehicle structure is provided at ``${CNSTREAM_DIR/samples/common/osd_handler`` .

To define a Osd handler, do the following steps:

- Define a class inherits from ``OsdHandler`` class.
- Override ``GetDrawInfo(const CNObjsVec &objects, const std::vector<std::string> &labels, std::vector<DrawInfo> *info)`` callback function. Set draw information to ``info`` .

###  Kafka Handler

We provide a Kafka handler at ``${CNSTREAM_DIR/samples/common/kafka_handler`` as an example for users who use Kafka module and want to produce CNFrameInfo data using RdKafka.

To define a Kafka handler, do the following steps:

- Define a class inherits from ``KafkaHandler`` class.
- Override ``UpdateFrame()`` callback function. Make all information you needed to a json string and produce the Kafka message.
