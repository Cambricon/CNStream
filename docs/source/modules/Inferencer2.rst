基于推理服务的神经网络推理模块
--------------------------------

神经网络推理模块2（Inferencer2）是基于Cambricon EasyDK推理服务实现的一个神经网络推理模块，主要包括前处理、推理和后处理三个部分。用户根据业务需求，只需载入相应的模型，即可调用底层的推理，简化了开发推理相关插件的代码。

Inferencer2与神经网络推理模块在功能上基本一致，区别如下：

- 推理服务提供了一套类似服务器的推理接口，Inferencer2在推理服务的基础上实现了推理模块功能，简化了模块内部的代码逻辑。
- 推理服务支持用户根据需求选择组batch的策略，包括dynamic和static两种策略。详情查看 batch策略_。
- 推理服务实例化模型的多个实例时，相对于神经网络推理模块，减少了MLU内存开销。
- 推理服务可以创建多份推理实例，并且在推理服务内部实现了负载均衡。
- 当多个模块的推理流程完全一致时，即前处理、推理模型和后处理部分都相同时，支持多个模块共用处理节点。

.. _inferencer2前处理:

前处理
^^^^^^^^^^^^^^^^^

前处理是将原始数据处理成适合网络输入的数据。

用户可通过在json配置文件中，设置参数 ``preproc_name`` 选择相应的前处理，取值如下:

- 通过ResizeAndConvert算子实现在MLU平台上对图像进行缩放和颜色空间转换的功能。可将 ``preproc_name`` 参数设置为 ``RCOP`` 或 ``rcop`` 。
- 实现在scaler硬件上对图像进行缩放和颜色空间转换功能。可将 ``preproc_name`` 参数设置为 ``SCALER`` 或 ``scaler`` 。仅在MLU220平台上支持。
- 自定义前处理，传入自定义前处理的类名。可以通过继承 ``cnstream::VideoPreproc`` 类，并重写 ``Execute`` 函数，实现自定义前处理。例如，使用CNStream中提供的 ``VideoPreprocCpu`` 前处理，可将 ``preproc_name`` 参数设置为 ``VideoPreprocCpu`` 。

  CNStream中提供开发样例，通过定义 ``VideoPreprocCpu`` 以及 ``VideoPreprocYolov3`` 前处理的类来自定义前处理，示例位于 ``samples/demo/preprocess`` 文件夹下的 ``video_preprocess_standard.cpp`` 和 ``video_preprocess_yolov3.cpp`` 文件中。

- 如果未指定 ``preproc_name`` 的值，默认选择使用ResizeAndConvert算子。
- 不支持传入空字符串作为 ``preproc_name``。

json配置文件详情查看 inferencer2说明_。

推理
^^^^^^^^^^^^^^^^^

推理是该模块的核心功能，用户只需在json配置文件中通过 ``model_path`` 参数设置离线模型的存放路径以及通过 ``func_name`` 参数指定子网络名字（一般为subnet0），传入离线模型即可。详情查看 inferencer2说明_。


.. _inferencer2后处理:

后处理
^^^^^^^^^^^^^^^^^

后处理是将模型的输出结果做进一步的加工处理，如筛选阈值等。后处理必须由用户指定，该模块不提供默认后处理。自定义后处理需继承 ``cnstream::VideoPostproc`` 类。

后处理的执行流程如下：

1. 推理结束后，在推理服务内部对推理结果进行后处理。在推理模块内部调用 ``cnstream::VideoPostproc`` 类的 ``Execute`` 函数。
   
   .. attention::
      | CNStream将整个batch的数据拆分为一份一份的数据传入后处理函数中进行处理。
	 
2. 将推理服务的结果写入到 ``CNFrameInfo`` 数据结构中。模块使用异步执行推理任务（前处理、推理和后处理三部分）的方式，在推理任务执行结束后，推理服务通过 ``Response`` 函数通知模块。 在 ``Response`` 函数中，模块将调用 ``cnstream::VideoPostproc`` 类的 ``UserProcess`` 函数。

   .. attention::
      | ``UserProcess`` 函数每次输入的数据为每一个后处理结果。例如，对于一级推理，每次输入一帧数据的结果，对于二级推理，每次输入一个object的结果。

CNStream提供以下两种自定义后处理的方法：

.. _自定义后处理方法一:

自定义后处理方法一
+++++++++++++++++++++

该方法适用于一般后处理，或后处理较复杂的场景。将后处理逻辑放进推理服务的后处理节点，与前处理和推理节点并行执行，提高执行效率。

1. 继承 ``cnstream::VideoPostproc`` 类。
2. 重写 ``Execute`` 函数，自定义处理每一份推理输出结果。函数参数描述如下：   
   
   - output_data（输出参数）：后处理的结果。参数类型为 ``infer_server::InferData*``。参数值可以在步骤3中 ``UserProcess`` 函数中获得。可设置任何类型的数据到本参数中。例如，设置一个 int 类型的数据， ``int example_var = 1; output_data->Set(example_var);``
   
   - model_output（输入参数）：离线模型推理的结果，即后处理的输入。参数类型为 ``const infer_server::ModelIO&`` 。可以通过 ``model_output.buffers`` 获得模型的输出。离线模型可以包含多个输出，每个输出都保存在buffers对应的元素中。例如，获得模型输出0的所有数据，数据类型为 ``float``， ``const float* data = reinterpret_cast<const float*>(model_output.buffers[0].Data());``
   
   - model_info （输入参数）：提供离线模型的详细信息，包括输入输出数量、形状、数据布局以及数据类型等。参数类型为 ``const infer_server::ModelInfo&`` 。
   
   .. attention::
      | CNStream提供 ``Execute`` 函数默认方法。将 ``model_output`` 的值直接填入 ``output_data`` 中。

3. 重新写 ``UserProcess`` 函数，将后处理过的数据按照自定义的方式，填入数据结构 ``CNFrameInfo`` 中。

   - 对于一级推理，模块的 ``object_infer`` 参数设为 ``false``。参数描述如下：

     - output_data（输入参数）：后处理结果。参数类型为 ``infer_server::InferDataPtr`` 。通过本参数可以获得在步骤2中 ``Execute`` 函数中填入的数据。例如， ``Execute`` 函数中设置了一个 int 类型的数据，通过 ``int example_var = output->GetLref<int>();`` 可以获得该数据。
     - model_info（输入参数）：提供离线模型的详细信息，包括输入输出数量，形状、数据摆放格式以及数据类型等。参数类型为 ``const infer_server::ModelInfo&`` 。
     - frame（输入输出参数）：CNStream中流动在各个模块间的数据结构。将结果数据信息填入到该数据结构中对应的字段上。参数类型为 ``cnstream::CNFrameInfoPtr`` 。

   - 次级推理，模块的 ``object_infer`` 参数设为 ``true``。参数描述如下：
   
     - output_data（输入参数）：后处理结果。参数类型为 ``infer_server::InferDataPtr`` 。通过本参数可以获得在步骤2中 ``Execute`` 函数中填入的数据。例如， ``Execute`` 函数中设置了一个 int 类型的数据，通过 ``int example_var = output->GetLref<int>();`` 可以获得该数据。

     - model_info （输入参数）：提供离线模型的详细信息，包括输入输出数量，形状，数据摆放格式以及数据类型等等。参数类型为 ``const infer_server::ModelInfo&`` 。

     - frame （输入输出参数）：CNStream中流动在各个模块间的数据结构。将后推理结果数据信息填入到该数据结构中对应的字段上。参数类型为 ``cnstream::CNFrameInfoPtr`` 。

     - obj （输入输出参数）：后推理结果对应的object。将结果数据信息填入到该数据结构中对应的字段上。参数类型为 ``std::shared_ptr<cnstream::CNInferObject>`` 。

.. attention::
   | 当使用 ``Execute`` 函数默认方法时，通过 ``infer_server::ModelIO model_output = output_data->GetLref<infer_server::ModelIO>();`` 获得离线模型输出数据。``model_output`` 的详细介绍，可参见步骤2对 ``model_output`` 参数的描述。

自定义后处理方法二
++++++++++++++++++++

适用于后处理较为简单的情况或需要在后处理中获得除输入、输出和模型以外的额外信息，比如，图像的原宽高等。

用户只需重写 ``UserProcess`` 函数，该方法的优点是较为简单。执行步骤如下：

1. 继承 ``cnstream::VideoPostproc`` 类。
2. 重写自定义后处理类的 ``UserProcess`` 函数。该函数用于执行后处理，并将后处理结果写入数据结构 ``CNFrameInfo``。详细参数信息参考 自定义后处理方法一_。
  

.. _batch策略:

batch策略
^^^^^^^^^^^^^^^^^

通常我们会选择多batch的离线模型进行推理，一次执行一组batch数据，减少任务下发次数提升资源利用率，达到提高推理性能的目的。当使用的离线模型为多batch时，该模块支持用户根据需求选择组batch的策略，包括dynamic和static策略。

- dynamic策略：总吞吐量较高。在推理服务内部进行组batch，每次请求后不会立即执行，而是等到组满整个batch或是超时后才开始执行任务，所以单个推理响应时延较长。
- static策略：总吞吐量较低。每次请求后立刻执行任务，因此单个推理响应时延较短。

用户可以通过json配置文件中的 ``batch_strategy`` 参数来选择batch策略。详情查看 inferencer2说明_。

.. _推理引擎:

推理引擎
^^^^^^^^^^^^^^^^^

推理引擎是推理服务中的核心部分，负责整个推理任务的调度执行等。 用户可以通过增加推理引擎个数，增加推理并行度，从而提高推理性能。每增加一个推理引擎，便会fork一份推理模型，增加一定数量的线程数量以及申请一定大小的MLU内存用于存放模型的输入和输出数据。

一般来说推理引擎数目设置为MLU的IPU核心数目除以模型的核心数目最为合适。如果设置大于这个数目，性能可能不会提升，并且会占用更多的MLU内存。

当两个及以上模块使用相同的推理任务时，如前处理、推理和后处理任务都使用相同的推理任务，将会共用相同的推理引擎。如果使用dynamic策略组batch，这些模块的数据可能会在推理服务内部被组成一个batch进行推理任务。

.. attention::
    | 在两个及以上模块共用推理引擎时，推理引擎数目等于第一个接入推理服务的模块设置的推理引擎数目，其他模块的设置将无效。

.. _inferencer2说明:

使用说明及参数说明
^^^^^^^^^^^^^^^^^^^^^^^

以下为 ``ssd_resnet34_and_resnet50_mlu270_config.json`` 配置文件示例。该示例文件位于 ``cnstream/samples/demo/secondary_by_inferserver`` 目录下。

::

  "detector" : {
    "class_name" : "cnstream::Inferencer2",   // （必设参数）推理类名。
    "parallelism" : 2,                        // （必设参数）并行度。
    "max_input_queue_size" : 20,              // （必设参数）最大队列深度。
    "next_modules" : ["classifier"],          // （必设参数）下一个连接模块的名称。
    "custom_params" : {                       // 特有参数。
      "model_path" : "../../../data/models/MLU270/Primary_Detector/ssd/resnet34_ssd.cambricon",
      "func_name" : "subnet0",
      "postproc_name" : "VideoPostprocSsd",
      "batching_timeout" : 300,
      "threshold" : 0.6,
      "batch_strategy" : "dynamic",
      "engine_num" : 2,
      "model_input_pixel_format" : "BGRA32",
      "show_stats" : false,
      "device_id" : 0
    }
  },

模块特有参数说明如下：

- model_path：（必设参数）模型存放的路径。如设置为相对路径，则应该设置为相对于JSON配置文件的路径。

- func_name：（必设参数）模型函数名。模型加载时必须用到的参数。

- postproc_name：（必设参数）后处理类名。详情参看 inferencer2后处理_。

- preproc_name：（可选参数）前处理类名。详情参看 inferencer2前处理_。

- device_id：（可选参数）设备id，用于标识多卡机器的设备唯一编号。默认值为0。

- engine_num：（可选参数）推理引擎个数。默认值1。详情参看 推理引擎_。

- batching_timeout：（可选参数）组batch的超时时间，单位为毫秒。只在 ``batch_strategy`` 为 ``dynamic`` 策略时生效。当超过指定的时间时，该模块将直接进行推理不再继续等待，未组满的部分数据则为随机值。一般应调整至大多数情况都能凑齐一组batch的时长，以避免资源的浪费。默认值为3000。

- batch_strategy：（可选参数）组batch的策略，目前支持 ``static`` ( ``STATIC`` ) 和 ``dynamic`` ( ``DYNAMIC`` ) 两种。默认为 ``dynamic`` 策略。详情参看 batch策略_。

- priority:（可选参数）该模块在推理服务中的优先级。优先级只在同一设备上有效。优先级限制为 0~9 的整数，低于 0 的按 0 处理，高于 9 的按 9 处理，数值越大，优先级越高。

- data_order：（可选参数）模型输出数据摆放顺序。可设置为 ``NCHW`` 或者 ``NHWC`` 。默认值为 ``NHWC`` 。

- threshold：（可选参数）后处理输出阈值。默认值为0。

- show_stats：（可选参数）是否显示推理服务内部的性能统计数据，包括前后处理、推理的吞吐量、时延等。可设置为 ``true`` 或者 ``false`` 。默认值为fasle。

- object_infer：（可选参数）是否为二级推理。可以设置为 ``true``、 ``1`` 、``TRUE`` 以及 ``True`` 具有相同效果，代表二级推理，以数据帧中的目标作为输入。可以设置为 ``false`` , ``0`` , ``FALSE`` 以及 ``False`` 具有相同效果，代表一级推理，以数据帧作为输入。默认值为false。

- keep_aspect_ratio：（可选参数）缩放时是否保持宽高比，请根据模型进行选择。只在使用 ``RCOP`` 作为前处理时生效。可以设置为 ``true`` 、 ``1`` 、 ``TRUE`` 以及 ``True`` 具有相同效果，代表保持宽高比。可以设置为 ``false`` 、 ``0`` 、 ``FALSE`` 以及 ``False`` 具有相同效果，代表不保持宽高比。默认值为false。

- model_input_pixel_format：（可选参数）模型输入的图像像素格式，请根据模型进行选择。对于使用 ``RCOP`` 前处理，该参数可以设置为 ``ARGB32``、``ABGR32`` 、 ``RGBA32`` 、 ``BGRA32`` 。对于用户自定义前处理，该参数可以设置为 ``ARGB32``、``ABGR32`` 、 ``RGBA32``、 ``BGRA32``、 ``RGB24`` 以及 ``BGR24``。用户可在自定义前处理类中通过 ``model_input_pixel_format_`` 成员变量获得该值。默认值为 ``RGBA32``。


开发样例
^^^^^^^^^^^^

自定义前处理开发样例
+++++++++++++++++++++

CNStream中提供自定义前处理示例，保存在 ``samples/demo/preprocess`` 文件夹，提供给用户参考:

一级推理前处理示例：

-  **VideoPreprocCpu** 类：标准前处理，通过颜色空间转换及缩放，将图片转换为适用离线网络的输入。定义在 ``video_preprocess_standard.cpp`` 文件中。
-  **VideoPreprocYolov3** 类：提供yolov3网络的前处理（输入保持宽高比）。通过颜色空间转换，缩放以及补边，将图片转换为适用离线网络的输入。定义在 ``video_preprocess_yolov3.cpp`` 文件中。

次级推理前处理示例：

-  **VideoObjPreprocCpu** 类：标准次级网络前处理。将object所在的roi区域截取出来。并通过颜色空间转换及缩放，将图片转换为适用离线网络的输入。定义在 ``video_preprocess_standard.cpp`` 文件中。


自定义后处理开发样例
++++++++++++++++++++++

CNStream中提供自定义后处理示例，保存在 ``samples/demo/postprocess`` 文件夹，提供给用户参考:

一级推理后处理示例：

-  **VideoPostprocClassification** 类：分类网络作为一级网络的后处理。定义在 ``video_postprocess_classification.cpp`` 文件中。
-  **VideoPostprocYolov3** 类：提供yolov3网络的后处理（输入保持宽高比）。定义在 ``video_postprocess_yolov3.cpp`` 文件中。
-  **VideoPostprocSsd** 类：提供ssd网络的后处理。定义在 ``video_postprocess_ssd.cpp`` 文件中。

次级推理后处理示例：

-  **VideoObjPostprocClassification** 类：分类网络作为次级网络的后处理。定义在 ``video_postprocess_classification.cpp`` 文件中。

