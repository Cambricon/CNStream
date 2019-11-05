.. _topics-module:

CNStream模块
=============================

简介
-----------------------------


针对常规的视频结构化领域，CNStream提供了3个核心功能模块：数据源处理(视频demux+视频解码)模块、神经网络推理模块和追踪模块：

* 数据源处理模块：支持多种协议的解封装及对多种格式的视频压缩格式进行解码，依赖于CNCodec SDK（MLU视频解码SDK）。视频压缩格式和图片硬解码支持详情，请参考“Cambricon-CNCodec-Developer-Guide”文档手册。

* 神经网络推理模块：依赖于CNRT，支持多种神经网络离线模型对图像数据进行神经网络推理。CNStream的插件式设计，给用户提供了视频流解码和推理之后，对数据进一步加工处理的办法。

* 追踪模块：使用经过针对寒武纪平台优化的Deepsort和KCF算法，降低CPU使用的同时，保证精度前提下，提高了模块性能。

除通用模块外，CNStream还提供了自定义示例模块：OSD模块、编码模块和多媒体显示模块等。

- OSD(On-Screen display) 模块：支持内容叠加和高亮物件处理。
- 编码模块：支持CPU上进行编码。
- 显示模块：支持屏幕上显示视频。


数据源模块介绍
--------------
数据源模块设计是处理解封装，解码的模块，主要用来帮助用户架起常规输入源与推理模块的桥梁，当前集成了业界知名的开源多媒体库ffmpeg来处理大部分本地或者网络数据源的处理，另外，为了调试方便，还设计了解码原始H264
文件的支持。使用很简单，根据需求，在配置文件中，可以很容易添加。

使用说明
^^^^^^^^^
在json配置文件中进行配置，如下所示：

::
 
  "source" : {
    "class_name" : "cnstream::DataSource",  //数据源类名
    "parallelism" : 0,                      //并行度，无效值，设置为0即可。
    "next_modules" : ["detector"],          //下一个连接插件名称
    "custom_params" : {                     //特有参数
      "source_type" : "ffmpeg",             //类型
      "output_type" : "mlu",                //mlu or cpu 可以选择
      "decoder_type" : "mlu",               //mlu or cpu 可以选择
      "device_id" : 0                       //设备id,用于标识多卡机器的设备唯一编号
    }
  }

神经网络推理模块介绍
---------------------------

神经网络推理模块是基于寒武纪实时运行库CNRT基础上加入多线程并行处理及适应网络特定前后处理模块的总称，用户根据业务，只需要载入客制化的模型后，就可以实现简单的调用底层的推理能力，当前根据业务特点，支持多batch
及单batch推理，具体可参阅代码实现。

使用说明
^^^^^^^^^^^^^^^^^
在json配置文件中进行配置，如下如下所示：

::

  "detector" : {
    "class_name" : "cnstream::Inferencer",    //推理类名               
    "parallelism" : 32,                       //并行度 
    "max_input_queue_size" : 20,              //最大队列深度   
    "next_modules" : ["tracker"],             //下一个连接模块  
    "custom_params" : {                       //特有参数 
      "model_path" : "../data/models/MLU100/Primary_Detector/resnet34ssd/resnet34_ssd.cambricon", //模型路径，本路径使用了示例中模型，可能会调整。
      "func_name" : "subnet0",                //模型函数名  
      "postproc_name" : "PostprocSsd",        //前处理模块名 
      "device_id" : 0                         //设备id,用于标识多卡机器的设备唯一编号
    }
  }

Track插件介绍
---------------
Track插件是一个追踪插件，用于对检测到的物体进行追踪并输出检查结果。主要应用于车辆等检测和追踪。目前支持DeepSort和KCF两种追踪方法。Track插件连接在inference插件之后，通过在配置文件中指定Track使用的离线模型以及使用的跟踪方法来配置插件。

使用说明
^^^^^^^^^
1. 配置 detection_config.json文件。该文件位于cnstream/samples/detection-demo路径下。配置客户所需要的离线模型和跟踪方法等。

   ::
 
     “tracker” : {
     “class_name” : “cnstream::Tracker”,        //Track的类名
     “parallelism” : 4,                         //并行度
     “max_input_queue_size” : 20,              // 数据输入队列长度
     “next_modules” : [“osd”],                 //下一个连接的插件名
     “custom_params” : {
         “model_path” : “xxx.cambricon”,        //track使用的离线模型的路径
         “func_name” : “subnet0”,              //模型函数名
         “track_name” : “KCF”                 // 使用的是DeepSort还是KCF
         }
     }


2. 创建Track插件。在Track插件头文件中，open接口用来打开该插件，close接口用来关闭该插件，Process用来处理每一帧送到该插件的数据。

   示例代码：

   ::
     
     class Tracker ：public Module, public ModuleCreator<Tracker> {
     public:
        explicit Tracker (const std::string& name);    //Create a Track plugin.
        ~ Tracker();

        // Open the Track plugin you just created. Called by pipeline when pipeline start.
        bool Open(cnstream::ModuleParamSet paramSet) override;

        // Called by pipeline when pipeline stop.
        void Close() override;

        // do process for each frame
        int Process(std::shared_ptr<CNFrameInfo> data) override;
     }

     static const char *name = “test-tracker”;
     Int main()
     {
     std::shared_ptr<Module> track = std::make_shared<Tracker>(name);
     ModuleParamSet param;

     param[“model_path”] = “test_track”;
     param[“func_name”] = “func_name”;
     param[“track_name”] = “track_name”;
     param[“device_id”] = 1;
     Track->Open(param);

     Int width = 1920, height = 1080;
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
