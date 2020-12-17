.. _application:

创建应用程序
=============================

概述
-----

基于CNStream创建应用程序，实际上是基于CNStream自有模块和用户自定义模块搭建业务流水线。用户可以选择使用配置文件方式或非配置文件方式创建应用程序。

配置文件方式与非配置文件方式的主要区别在于，配置文件使用JSON文件格式声明pipeline结构、模块上下游关系和模块参数等，而非配置文件则需要开发者创建模块对象，设置模块参数和模块上下游关系等。  相对而言，配置文件方式更加灵活，推荐使用。开发者编写pipeline基本骨架后，仍可以灵活地调整配置文件中的模块参数甚至结构，而无需重新编译。

应用程序的创建
---------------
 
配置文件方式
^^^^^^^^^^^^^^^^^^^^

在配置文件方式下，用户开发应用时需要关注两部分：JSON配置文件的编写和pipeline基本骨架的构建。 

JSON配置文件的编写
*******************

JSON配置文件主要用于声明pipeline中各个模块的上下游关系及其每个模块内部的参数配置。   

下面示例展示了如何使用CNStream提供的自有模块DataSource、Inferencer、Tracker、Osd、Encoder，以及ssd和track离线模型，实现一个典型的pipeline操作。

典型的pipeline操作为：

1. 视频源解析和解码。
2. 物体检测。
3. 追踪。
4. 在视频帧上，叠加绘制的物体检测信息框。
5. 编码输出视频。   

配置文件示例如下：
 
::

  {
  "source" : {     
    // 数据源模块。设置使用ffmpeg进行demux，使用MUL解码，不单独启动线程。 
    "class_name" : "cnstream::DataSource",
    "parallelism" : 0,
    "next_modules" : ["detector"],
    "show_perf_info" : true,
    "custom_params" : {
      "source_type" : "ffmpeg",
      "output_type" : "mlu",
      "decoder_type" : "mlu",
      "device_id" : 0
    }
  },

  "detector" : {  
    // Inferencer模块。设置使用resnet34ssd离线模型，使用PostprocSsd进行网络输出数据后处理，并行度为4，模块输入队列的max_size为20。
    "class_name" : "cnstream::Inferencer",
    "parallelism" : 4,
    "max_input_queue_size" : 20,
    "next_modules" : ["tracker"],
    "show_perf_info" : true,
    "custom_params" : {
      "model_path" : "../data/models/resnet34ssd/resnet34_ssd.cambricon",
      "func_name" : "subnet0",
      "postproc_name" : "PostprocSsd",
      "device_id" : 0
    }
  },

  "tracker" : {   
    // Tracker模块。设置使用track离线模型，并行度为4，模块输入队列的max_size为20。
     "class_name" : "cnstream::Tracker",
    "parallelism" : 4,
    "max_input_queue_size" : 20,
    "next_modules" : ["osd"],
    "show_perf_info" : true,
    "custom_params" : {
      "model_path" : "../data/models/Track/track.cambricon",
      "func_name" : "subnet0"
    }
  },

  "osd" : {
    // Osd模块。配置解析label路径，设置并行度为4，模块输入队列的max_size为20。
    "class_name" : "cnstream::Osd",
    "parallelism" : 4,
    "max_input_queue_size" : 20,
    "next_modules" : ["encoder"],
    "show_perf_info" : true,
    "custom_params" : {
      "chinese_label_flag" : "false", 
      "label_path" : "../data/models/resnet34ssd/label_voc.txt"
    }
  },

  "encoder" : {
    // Encoder模块。配置输出视频的dump路径，设置并行度为4，模块输入队列的max_size为20。
    "class_name" : "cnstream::Encoder",
    "parallelism" : 4,
    "max_input_queue_size" : 20,
    "show_perf_info" : true,
    "custom_params" : {
      "dump_dir" : "output"
    }
  }
 }

用户可以参考以上JSON的配置构建自己的配置文件。另外，CNStream提供了inspect工具来查询每个模块支持的自定义参数以及检查JSON配置文件的正确性。详情查看 :ref:`inspect` 。

Pipeline基本骨架的构建
***********************

构建pipeline核心骨架包括：搭建整体业务流水线和设置事件监听处理机制。    

在配置文件方式下，搭建整体的业务流水线实际是从预准备的JSON文件中获取pipeline结构、module上下游关系和各个module的参数，并初始化各个任务执行环节，即模块。另外，用户可以通过设置事件监听获取pipeline的处理状态，添加对应的状态处理机制，如eos处理、错误处理等。
   
整个过程主要包括下面步骤：

1. 创建pipeline对象。
2. 调用 ``Pipeline.BuildPipelineByJSONFile`` ，使用预准备的JSON配置文件构建。
3. 调用 ``pipeline.SetStreamMsgObserver`` ，设置事件监听处理机制。
4. 调用 ``pipeline.CreatePerfManager``，创建性能统计管理器。
5. 调用 ``pipeline.Start()`` ，启动pipeline。
6. 调用 ``pipeline.AddVideoSource()`` 或 ``RemoveSource()`` ，动态添加或删除视频和图片源。

源代码示例，可参考CNStream源码中 ``samples/demo/demo.cpp`` 。      

非配置文件方式  
^^^^^^^^^^^^^^^^^^^^

CNStream针对非配置文件方式提供了一些完整的、独立的应用程序开发示例。参见CNStream源代码中 ``samples/example/example.cpp``。

用户侧MessageHandle
---------------------

用户程序可以通过注册的事件监听监测Pipeline的Message信息，目前定义的用户侧Message信息包括EOS_MSG、FRAME_ERR_MSG、STREAM_ERR_MSG、ERROR_MSG(参见StreamMsgType定义)。

各消息处理示例可以参考CNStream源代码 ``samples/demo/demo.cpp``。

1. EOS_MSG
^^^^^^^^^^^^^^^^^^^^

EOS_MSG表示Pipeline数据处理结束，接收到该消息时，可以正常结束Pipeline释放资源等。

2. FRAME_ERR_MSG
^^^^^^^^^^^^^^^^^^^^

FRAME_ERR_MSG表示帧解码失败消息，当前仅支持使用mlu解码JPEG图片场景：

（1）JPEG图片文件形式时，用户侧接收到FRAME_ERR_MSG消息时，可以同时获取解码错误的图片帧信息，包含用户侧定义的stream_id和内部赋值定义的pts、frame_id信息；

（2）从内存中输入JPEG数据流时，用户侧接收到FRAME_ERR_MSG消息时，可以同时获取解码错误的图片帧信息，包含用户侧定义的stream_id、pts和内部赋值定义的frame_id信息；

接收到这些信息后，用户侧可以根据自己的业务逻辑处理解码失败的图片帧，比如丢弃、记录等。

3. STREAM_ERR_MSG
^^^^^^^^^^^^^^^^^^^^

STREAM_ERR_MSG表示某一路数据发生不可恢复错误，通常包括超过内存限制导致的解码器申请失败等。

用户侧接收到该信息时，若希望Pipeline继续进行，将出现错误的数据流移除掉即可（使用Source模块的RemoveSource方法进行特定数据流的卸载），该操作不影响其他正常处理的数据流。

4. ERROR_MSG
^^^^^^^^^^^^^^^^^^^^

ERROR_MSG表示普通的错误信息，目前表示不可恢复错误，建议直接停止Pipeline，并根据log信息进行错误定位。