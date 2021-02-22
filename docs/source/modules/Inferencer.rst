
神经网络推理模块
---------------------------

神经网络推理（Inferencer）模块是基于寒武纪运行时库（CNRT）的基础上，加入多线程并行处理及适应网络特定前后处理模块的总称。用户根据业务需求，只需载入客制化的模型，即可调用底层的推理。根据业务特点，该模块支持多batch及单batch推理，具体可参阅代码实现。

使用说明及参数详解
^^^^^^^^^^^^^^^^^^^

在 ``detection_config.json`` 配置文件中进行配置，如下所示。该文件位于 ``cnstream/samples/demo`` 目录下。

::

  "detector" : {
    "class_name" : "cnstream::Inferencer",    // （必设参数）推理类名。               
    "parallelism" : 2,                        // （必设参数）并行度。 
    "max_input_queue_size" : 20,              // （必设参数）最大队列深度。   
    "next_modules" : ["tracker"],             // （必设参数）下一个连接模块的名称。  
    "custom_params" : {                       // 特有参数 。
      // （必设参数）模型路径。本例中的路径使用了代码示例的模型，用户需根据实际情况修改路径。该参数支持绝对路径和相对路径。相对路径是相对于JSON配置文件的路径。
      "model_path" : "../data/models/MLU100/Primary_Detector/resnet34ssd/resnet34_ssd.cambricon",
      // （必设参数）模型函数名。通过寒武纪神经网络框架生成离线模型时，通过生成的twins文件获取。
      "func_name" : "subnet0",  
      // （可选参数）前处理类名。可继承cnstream::Preproc实现自定义前处理。在代码示例中，提供标准前处类PreprocCpu和YOLOv3的前处理类PreprocYolov3。
      preproc_name" : "PreprocCpu",          
      // （必设参数）后处理类名。可继承cnstream::Postproc实现自定义后处理操作。在代码示例中提供分类、SSD以及YOLOv3后处理类。
      "postproc_name" : "PostprocSsd",                        
      // （可选参数）攒batch的超时时间，单位为毫秒。即使用多batch进行推理时的超时机制。当超过指定的时间时，该模块将直接进行推理，不再继续等待上游数据。
      "batching_timeout" : 30,                
      "device_id" : 0    // （可选参数）设备id，用于标识多卡机器的设备唯一编号。
    }
  }

.. attention::
   | preproc_name、model_input_pixel_format、use_scaler、keep_aspect_ratio几个参数与图像预处理息息相关。请仔细查阅使用说明，避免预处理不符合模型要求，导致错误的推理结果。
   | postproc_name指定的网络后处理实现尤其需要注意数据摆放顺序的问题，请仔细查阅postproc_name、data_order两个参数。


model_path
'''''''''''''

设置离线模型路径。

func_name
'''''''''''''

离线模型中函数名，一般为subnet0，可以从生成离线模型时生成的twins文件中获得。

postproc_name
'''''''''''''''

指定后处理类名，后处理类用来处理离线模型输出数据。后处理类应继承自cnstream::Postproc。后处理类实现方法可参考 ``samples/demo/postprocess/postprocess_ssd.cpp`` 中实现。



preproc_name
'''''''''''''''

指定预处理类名，预处理类用来实现自定义图像预处理，经过指定预处理类处理过后的数据将作为离线模型的输入进行推理。自定义预处理实现方法可参考 ``samples/demo/preprocess/preprocess_standard.cpp``。默认值为空字符串。

当未设置preproc_name或者设置为空字符串，且use_scaler参数为false时。将使用默认的MLU预处理，默认在MLU上实现的预处理将使用IPU资源进行颜色空间转换以及缩放和抠图操作。

预处理类应继承自cnstream::Preproc或cnstream::ObjPreproc，详情查看 object_infer_。

.. attention::
   |  该参数在Inferencer插件预处理系列参数中拥有最高优先级。
   |  自定义预处理需要注意最后需将预处理结果转为float类型，并按照NHWC的顺序摆放。详情查看cnstream::Preproc声明。

use_scaler
'''''''''''''

MLU220平台（包括M.2 和 EDGE)上可使用该参数。默认值为false。

scaler为MLU220平台上专门用于图像预处理的硬件单元。

当设置为true时，将使用scaler做网络预处理，包括颜色空间转换、图像缩放和抠图操作。该参数优先级比preproc_name参数低。

使用该参数应保证输入的YUV420 SP NV12/NV21为128像素对齐，可参考DataSource插件的 :ref:`apply_stride_align_for_scaler`。且需要注意scaler的输出为4通道ARGB数据且宽度为32的倍数。所以如果要使用scaler，离线模型的输入规模中宽必须徐为32的倍数。

当网络输入规模中宽度不是32的倍数，一般做法是在生成离线模型之前在网络首层加入crop层，并设置网络首层接收ARGB输入。

以Cambricon Caffe框架为例。如果有一个300x300的输入规模的网络，在网络的量化时设置input_format为ARGB，并在量化后在第一层卷积上加入ResizeCrop层，ResiceCrop层输入规模为300x320，输出为300x300。

batching_timeout
''''''''''''''''''''

设置攒batch超时时间，单位ms。默认为3000ms。

每次进行离线推理会根据离线模型的batchsize，一次送入一个batch的数据进行推理。在每次攒batch的过程中，如果超过batching_timeout指定的时间还未攒够一个batch的图，则不继续等待数据，直接进行推理。

data_order
''''''''''''''

指定网络输出数据按何种顺序排布，默认为NHWC。可选值有NHWC、NCHW。

.. attention::
   | 该参数决定了postproc_name中，指定的后处理类接收到的网络的输出数据的排布方式。
   | 当指定的数据排布顺序与模型输出的数据排布不同时将使用CPU进行数据重排。模型的输出数据顺序可通过查看生成模型时伴生的twins文件得到。

threshold
'''''''''''''''

浮点数，透传给后处理类。

infer_interval
'''''''''''''''

正整数，默认为1。抽帧推理，每隔infer_interval个数据推理一次。剩余的数据不会进行计算，但是不被丢弃。

例如，解码后输出7帧，infer_interval设置为3，则第1、4、7帧将进行推理，其它数据不进行推理。

.. _object_infer:

object_infer
'''''''''''''''

表示是否以检测目标为单位进行推理。可选值为1、true、TRUE、True、0、false、FALSE、False。默认值为false。

当为false时，以DataSource插件解码后的数据帧作为输入进行推理。

当为true时，以CNFrameInfo::datas[CNObjsVecKey]中存储的检测目标作为输入进行推理。

当object_infer为true时，preproc_name指定的预处理类应继承自cnstream::ObjPreproc。

当object_infer为false时，preproc_name指定的预处理类应继承自cnstream::Preproc。

object_filer_name
''''''''''''''''''''

指定过滤器类名，过滤器用来过滤检存放在CNFrameInfo::datas[CNObjsVecKey]中存储的检测目标，可用来过滤不需要进行推理的检测目标。当obj_infer为true时生效。

过滤器应继承自cnstream::ObjFiler类。可参考 ``samples/demo/obj_filer/car_filer.cpp`` 中实现。

keep_aspect_ratio
'''''''''''''''''''

指定当图像预处理在MLU上进行时，图像是否保持长宽比进行缩放。当使用保持长宽比的方式进行缩放，图像将保持长宽比不变缩放至网络的输入大小，并在左右或上下补0。

可选值1、true、TRUE、True、0、false、FALSE、False。默认值为false。


当preproc_name为空字符串且use_scaler为false时生效。

dump_resized_image_dir
''''''''''''''''''''''''''

调试功能，用于保存离线模型执行前的图像数据，用于检查预处理是否符合网络要求。

指定保存图片的目录路径。

model_input_pixel_format
''''''''''''''''''''''''''

用于指定离线模型输入要求的4通道颜色顺序。可选值为ARGB32、ABGR32、RGBA32、BGRA32。默认值为RGBA32。


mem_on_mlu_for_postproc
'''''''''''''''''''''''''

指定网络输出数据的内存是否存放在MLU设备内存上。可选值为1、true、TRUE、True、0、false、FALSE、False。默认值为false。


- 当为false，则网络输出数据将搬运至主机测，并根据data_order指定的数据顺序摆放数据。

- 当为true时，则网络推理后不进行内存拷贝，指针直接传递给postproc_name指定的后处理类中进行处理。

详情请查看cnstream::PostProc类中声明。

saving_infer_input
''''''''''''''''''''''

指定是否保存网络预处理结果。可选值为1、true、TRUE、True、0、false、FALSE、False。默认值为false。

若为true，则将网络预处理结果保存至CNFrameInfo::datas[CNInferDataPtrKey]中向后传递。

device_id
''''''''''''''

设置使用的设备id，决定MLU解码使用的设备及解码后数据存放在哪张MLU卡上。
