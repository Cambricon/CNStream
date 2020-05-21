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
      "model_path" : "../data/models/MLU100/Primary_Detector/resnet34ssd/resnet34_ssd.cambricon",
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
      "model_path" : "../data/models/MLU100/Track/track.cambricon",
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
      "label_path" : "../data/models/MLU100/Primary_Detector/resnet34ssd/label_voc.txt"
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
