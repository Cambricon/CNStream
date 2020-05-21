内置模块
===========

简介
-----

针对常规的视频结构化领域，CNStream提供了以下核心功能模块：

* 数据源处理模块：依赖于CNCodec SDK（MLU视频解码SDK），用于视频demux和视频解码。支持多种协议的解封装及对多种格式的视频压缩格式进行解码。视频压缩格式和图片硬解码支持详情，请参考“Cambricon-CNCodec-Developer-Guide”文档手册。

* 神经网络推理模块：依赖于寒武纪实时运行库（CNRT），支持多种神经网络离线模型对图像数据进行神经网络推理。CNStream的模块式设计，为用户提供了在视频流解码和推理之后的进一步数据加工和处理。

* 追踪模块：使用针对寒武纪平台优化的FeatureMatch和KCF算法，在保证精度的前提下减少CPU使用率，提高了模块性能。

除以上核心模块外，CNStream还提供了自定义示例模块：OSD模块、编码模块、多媒体显示模块和帧率统计模块等。

- OSD（On-Screen Display）模块：支持内容叠加和高亮物件处理。
- 编码模块：支持在CPU上编码。
- 多媒体显示模块：支持屏幕上显示视频。

数据源模块
--------------
数据源（DataSource）模块是pipeline的起始模块，实现了视频图像获取功能。通过FFmpeg来解封装，或解复用本地文件或者网络流，来得到码流。之后喂给CNDecoder或者CPU Decoder进行解码，得到图像，并把图像存到CNDataFrame的syncedMem中。另外，为了调试方便，该模块还支持读取H264和H265格式的裸码流文件。

.. attention::
   |  一个pipeline只支持定义一个数据源模块。

数据源模块主要有以下特点：

- 作为pipeline的起始模块，没有输入队列。因此pipeline不会为DataSource启动线程，也不会调度source。source module需要内部启动线程，通过pipeline的 ``ProvideData()`` 接口向下游发送数据。
- 每一路source由使用者指定唯一标识 ``stream_id`` 。
- 支持动态增加和减少数据流。
- 支持通过配置文件修改和选择source module的具体功能，而不是在编译时选择。

**cnstream::DataSource** 类在 ``cnstream_source.hpp`` 文件中定义。 ``cnstream_source.hpp`` 文件存放在 ``modules/core/include`` 文件夹下。 主要接口如下。源代码中有详细的注释，这里仅给出必要的说明。

::

  class DataHandler;

  class DataSource : public Module, public ModuleCreator<DataSource> {
   public:
    // 动态增加一路stream接口。
    int AddVideoSource(const std::string &stream_id, const std::string &filename, int framerate, bool loop = false);
    // 动态减少一路stream接口。
    int RemoveSource(const std::string &stream_id);

    // 对source module来说，Process（）不会被调用。
    // 由于Module::Process()是纯虚函数，这里提供一个缺省实现。
    int Process(std::shared_ptr<CNFrameInfo> data) override;
    ...

   private:
    ...
    // 每一路stream，使用一个DataHandler实例实现。
    // source module维护stream_id和source handler的映射关系。
    // 用来实现动态的增加和删除某一路stream。
    std::map<std::string /*stream_id*/, std::shared_ptr<DataHandler>> source_map_;
  };

使用说明
^^^^^^^^^

在 ``detection_config.json`` 配置文件中进行配置，如下所示。该文件位于 ``cnstream/samples/detection-demo`` 目录下。

::
 
  "source" : {
    "class_name" : "cnstream::DataSource",  // 数据源类名。
    "parallelism" : 0,                      // 并行度。无效值，设置为0即可。
    "next_modules" : ["detector"],          // 下一个连接模块的名称。
    "custom_params" : {                     // 特有参数。
      "source_type" : "ffmpeg",             // source类型。
      "output_type" : "mlu",                // 输出类型。可设为MLU或CPU。
      "decoder_type" : "mlu",               // decoder类型。可设为MLU或CPU。
      "reuse_cndec_buf" : "false"           // 是否复用Codec的buffer。
      "device_id" : 0                       // 设备id,用于标识多卡机器的设备唯一编号。
    }
  }

其他配置字段可以参考 ``data_source.hpp`` 中详细注释。

神经网络推理模块
---------------------------

神经网络推理（Inferencer）模块是基于寒武纪实时运行库（CNRT）的基础上，加入多线程并行处理及适应网络特定前后处理模块的总称。用户根据业务需求，只需载入客制化的模型，即可调用底层的推理。根据业务特点，该模块支持多batch及单batch推理，具体可参阅代码实现。

使用说明
^^^^^^^^^^^^^^^^^

在 ``detection_config.json`` 配置文件中进行配置，如下所示。该文件位于 ``cnstream/samples/detection-demo`` 目录下。

::

  "detector" : {
    "class_name" : "cnstream::Inferencer",    // 推理类名。               
    "parallelism" : 32,                       // 并行度。 
    "max_input_queue_size" : 20,              // 最大队列深度。   
    "next_modules" : ["tracker"],             // 下一个连接模块的名称。  
    "custom_params" : {                       // 特有参数 。
      // 模型路径。本例中的路径使用了代码示例的模型，用户需根据实际情况修改路径。该参数支持绝对路径和相对路径。相对路径是相对于JSON配置文件的路径。
      "model_path" : "../data/models/MLU100/Primary_Detector/resnet34ssd/resnet34_ssd.cambricon",
      // 模型函数名。通过寒武纪神经网络框架生成离线模型时，通过生成的twins文件获取。
      "func_name" : "subnet0",  
      // 前处理类名。可继承cnstream::Preproc实现自定义前处理。在代码示例中，提供标准前处类PreprocCpu和YOLOv3的前处理类PreprocYolov3。
      preproc_name" : "PreprocCpu",          
      //后处理类名。可继承cnstream::Postproc实现自定义后处理操作。在代码示例中提供分类、SSD以及YOLOv3后处理类。
      "postproc_name" : "PostprocSsd",        
      // 多batch推理支持。用于提高单位时间内吞吐量。该参数仅支持MLU100。MLU100生成离线时设置batchsize为1。通过指定batchsize参数，来进行多batch推理。使用MLU270进行多batch推理时，需要在生成离线模型时指定batchsize。
      "batchsize" : 1,                        
      // 攒batch的超时时间，单位为毫秒。即使用多batch进行推理时的超时机制。当超过指定的时间时，该模块将直接进行推理，不再继续等待上游数据。
      "batching_timeout" : 30,                
      "device_id" : 0    // 设备id，用于标识多卡机器的设备唯一编号。
    }
  }

追踪模块
---------------

追踪（Tracker）模块用于对检测到的物体进行追踪并输出检查结果。主要应用于车辆等检测和追踪。目前支持FeatureMatch和KCF两种追踪方法。该模块连接在神经网络推理模块后，通过在配置文件中指定追踪使用的离线模型以及使用的追踪方法来配置模块。

使用说明
^^^^^^^^^

1. 配置 ``detection_config.json`` 文件。该文件位于 ``cnstream/samples/demo`` 目录下。配置所需要的离线模型和追踪方法等。

   ::
 
     “tracker” : {
     “class_name” : “cnstream::Tracker”,       // Track的类名。
     “parallelism” : 4,                        // 并行度。
     “max_input_queue_size” : 20,              // 数据输入队列长度。
     “next_modules” : [“osd”],                 // 下一个连接的模块名。
     “custom_params” : {
         // 追踪使用的离线模型的路径。该参数支持绝对路径和相对路径。相对路径是相对于JSON配置文件的路径。
         “model_path” : “xxx.cambricon”,        
         “func_name” : “subnet0”,    // 模型函数名。
         “track_name” : “KCF”       // 追踪方法。支持FeatureMatch和KCF两种追踪方法。
         }
     }
    
2. 创建追踪模块。在追踪模块的头文件中，做如下设置:

   - 调用 ``Open`` 接口打开该模块。
   - 调用 ``close`` 接口关闭该模块。
   - 调用 ``Process`` 接口处理每一帧送到该模块的数据。

   示例代码：

   ::
     
     class Tracker ：public Module, public ModuleCreator<Tracker> {
     public:
        explicit Tracker (const std::string& name);    //Create a Track plugin.
        ~ Tracker();

        // Open the Track plugin you have just created. Called by pipeline when pipeline started.
        bool Open(cnstream::ModuleParamSet paramSet) override;

        // Called by pipeline when pipeline stopped.
        void Close() override;

        // Process each frame.
        int Process(std::shared_ptr<CNFrameInfo> data) override;
     }

     static const char *name = “test-tracker”;
     int main()
     {
     std::shared_ptr<Module> track = std::make_shared<Tracker>(name);
     ModuleParamSet param;

     param[“model_path”] = “test_track”;
     param[“func_name”] = “func_name”;
     param[“track_name”] = “track_name”;
     param[“device_id”] = 1;
     track->Open(param);

     int width = 1920, height = 1080;
     size_t nbytes = width x height x sizeof(uint8_t) * 3; 
     auto data = cnstream::CNFrameInfo::Create(std::to_string(channel_id));
     data->channel_idx = channel_id;
     CNDataFrame &frame = data->frame;
     frame.frame_id = 1;
     frame.width = width;
     frame.height = height;
     frame.fmt = CN_PIXEL_FORMAT_YUV420_NV21;
     frame.strides[0] = width;
     frame.ctx.dev_type = DevContext::DevType::MLU;
     frame.data[0].reset(new CNSyncedMemory(nbytes));

     int ret = track->Process(data);
     if (ret != 0)
       printf(“track process error\n”);

     track->Close();
     }

.. _多线程:

多线程操作
---------------

由于pipeline只能进行单进程操作，用户可以通过ModuleIPC模块将pipeline拆分成多个进程，并完成进程间数据传输和通信，例如最常见的解码和推理进程分离等。ModuleIPC模块继承自CNStream中的Module类。两个ModuleIPC模块组成一个完整的进程间通信。此外，通过定义模块的 ``memmap_type`` 参数，可以选择进程间的内存共享方式。

使用示例
^^^^^^^^^

下面以进程1做解码，进程2做推理为例，展示了如何使用ModuleIPC模块完成多进程设置和通信。

1. 创建配置文件，如 ``config_process1.json``。在配置文件中设置进程1。第一个模块配置为解码模块，再配置一个ModuleIPC模块。主要参数设置如下：
   
   - 设置 ``ipc_type`` 参数值为 **client**，做为多进程通信的客户端。
   - 设置 ``memmap_type`` 参数值为 **cpu**。当前仅支持CPU内存共享方式。后续会支持MLU内存共享方式。
   - 设置 ``socket_address`` 参数值为进程间通信地址。用户需定义一个字符串来表示通信地址。
   
   示例如下：

   ::
   
     {
       "source" : {
         "class_name" : "cnstream::DataSource",	 // 数据源类名。
         "parallelism" : 0,			 // 并行度。无效值，设置为0即可。
         "next_modules" : ["ipc"],		 // 下一个连接模块的名称。
         "custom_params" : {			 // 特有参数设置。
           "source_type" : "ffmpeg",		 // source类型。
           "reuse_cndec_buf" : "false",		 // 是否复用codec的buffer。
           "output_type" : "mlu",	         // 输出类型，可以设置为MLU或CPU。
           "decoder_type" : "mlu",		 // decoder类型，可以设置为MLU或CPU。
           "device_id" :0			 // 设备id，用于标识多卡机器的设备唯一标号。
         }
       },
     
       "ipc" : {
         "class_name" : "cnstream::ModuleIPC",	// 进程间通信类名。
         "parallelism" :1 ,		        // 并行度，针对client端，设置为1。
         "max_input_queue_size" : 20,		// 最大队列深度。
         "custom_params" : {			// 特有参数设置。
           "ipc_type" : "client",		// 进程间通信类型，可设为client和server。上游进程设置为client，下游进程设置为server。
           "memmap_type" : "cpu",		// 进程间内存共享类型，可以设置为CPU。
           "max_cachedframe_size" : "40",       // 最大缓存已处理帧队列深度，仅client端有该参数。
           "socket_address" : "test_ipc"        // 进程间通信地址，一对通信的进程，需要设置为相同的通信地址。
         }
       }
     }
     

2. 创建配置文件，如 ``config_process2.json``。在配置文件中设置进程2。第一个模块配置为ModuleIPC模块，然后设置推理模块。在ModuleIPC模块中，主要参数设置如下：
   
   - 设置 ``ipc_type`` 参数值为 **server**，做为多进程通信的服务器端。
   - 设置 ``memmap_type`` 参数值为 **cpu**。当前仅支持CPU内存共享方式。后续会支持MLU内存共享方式。
   - 设置 ``socket_address`` 参数值为进程间通信地址。用户需定义一个字符串来表示通信地址。
   
   .. attention::
       | ``memmap_type`` 与 ``socket_address`` 的参数值设置需要与进程1中ModuleIPC模块的相关参数设置保持一致。

   ::

     {
       "ipc" : {
         "class_name" : "cnstream::ModuleIPC",	// 进程间通信类名。
         "parallelism" : 0,			// 并行度，无效值，针对server端，设置为0即可。
         "next_modules" : ["infer"],		// 下一个连接模块名称。
         "custom_params" : {			// 特有参数设置。
           "ipc_type" : "server",		// 进程间通信类型，可设为client和server。上游进程设置为client，下游进程设置为server。
           "memmap_type" : "cpu",		// 进程间内存共享类型，可以设置为CPU。
           "socket_address" : "test_ipc"        // 进程间通信地址，一对通信的进程，需要设置为相同的通信地址。
         }
       },
     
       "infer" : {				
         "class_name" : "cnstream::Inferencer",	  // 推理类名。
         "parallelism" : 1,			  // 并行度。
         "max_input_queue_size" : 20,		  // 最大队列深度。
         "custom_params" : {			  // 特有参数设置。
           "model_path" : "../../data/models/MLU270/Classification/resnet50/resnet50_offline.cambricon",   // 模型路径。
           "func_name" : "subnet0",		          // 模型函数名。
           "postproc_name" : "PostprocClassification",	  // 后处理类名。
           "batching_timeout" : 60,			  // 攒batch的超时时间。
           "device_id" : 0				  // 设备id，用于标识多卡机器的设备唯一标号。
         }
       }
     }










