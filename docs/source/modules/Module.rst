.. _内置模块:

内置模块
===========

简介
-----

针对常规的视频结构化领域，CNStream提供了以下核心功能模块：

* 数据源处理模块：依赖于CNCodec SDK（MLU视频解码SDK），用于视频解码和JPEG解码。支持多种协议的解封装及对多种格式的视频压缩格式进行解码。视频压缩格式和图片硬解码支持详情，请参考“Cambricon-CNCodec-Developer-Guide”文档手册。

* 神经网络推理模块：依赖于寒武纪实时运行库（CNRT），支持多种神经网络离线模型对图像数据进行神经网络推理。CNStream的模块式设计，为用户提供了在视频流解码和推理之后的进一步数据加工和处理。

* 追踪模块：使用针对寒武纪平台优化的FeatureMatch和KCF算法，在保证精度的前提下减少CPU使用率，提高了模块性能。

除以上核心模块外，CNStream还提供了自定义示例模块：OSD模块、编码模块、多媒体显示模块和RTSP推流模块等。

- OSD（On-Screen Display）模块：支持内容叠加和高亮物件处理。
- 编码模块：支持在CPU上编码。
- 多媒体显示模块：支持屏幕上显示视频。
- RTSP推流模块：将图像数据编码后推流至互联网。

数据源模块
--------------
数据源（DataSource）模块是pipeline的起始模块，实现了视频图像获取功能。不仅支持获取内存数据裸流，还可以通过FFmpeg解封装、解复用本地文件或网络流来得到码流。之后喂给解码器解码得到图像，并把图像存到CNDataFrame的CNSyncedMemory中。目前支持H264、H265、MP4、JPEG、RTSP等协议。

.. attention::
   |  一个pipeline只支持定义一个数据源模块。

数据源模块主要有以下特点：

- 作为pipeline的起始模块，没有输入队列。因此pipeline不会为DataSource启动线程，也不会调度source。source module需要内部启动线程，通过pipeline的 ``ProvideData()`` 接口向下游发送数据。
- 每一路source由使用者指定唯一标识 ``stream_id`` 。
- 支持动态增加和减少数据流。
- 支持通过配置文件修改和选择source module的具体功能，而不是在编译时选择。

**cnstream::DataSource** 类在 ``data_source.hpp`` 文件中定义。 ``data_source.hpp`` 文件存放在 ``modules/source/include`` 文件夹下。 ``DataSource`` 主要功能继承自 ``SourceModule`` 类，存放在 ``framework/core/include`` 目录下。主要接口如下。源代码中有详细的注释，这里仅给出必要的说明。

::

  class SourceModule : public Module {
   public:
    // 动态增加一路stream接口。
    int AddSource(std::shared_ptr<SourceHandler> handler);
    // 动态减少一路stream接口。
    int RemoveSource(std::shared_ptr<SourceHandler> handler);
    int RemoveSource(const std::string &stream_id);

    // 对source module来说，Process（）不会被调用。
    // 由于Module::Process()是纯虚函数，这里提供一个缺省实现。
    int Process(std::shared_ptr<CNFrameInfo> data) override;
    ...

   private:
    ...
    // 每一路stream，使用一个SourceHandler实例实现。
    // source module维护stream_id和source handler的映射关系。
    // 用来实现动态的增加和删除某一路stream。
    std::map<std::string /*stream_id*/, std::shared_ptr<SourceHandler>> source_map_;
  };

使用说明
^^^^^^^^^

在 ``detection_config.json`` 配置文件中进行配置，如下所示。该文件位于 ``cnstream/samples/demo`` 目录下。

::
 
  "source" : {
    "class_name" : "cnstream::DataSource",  // （必设参数）数据源类名。
    "parallelism" : 0,                      // （必设参数）并行度。无效值，设置为0即可。
    "next_modules" : ["detector"],          // （必设参数）下一个连接模块的名称。
    "custom_params" : {                     // 特有参数。
      "source_type" : "ffmpeg",             // （可选参数）source类型。
      "output_type" : "mlu",                // （可选参数）输出类型。可设为MLU或CPU。
      "decoder_type" : "mlu",               // （可选参数）decoder类型。可设为MLU或CPU。
      "reuse_cndec_buf" : "false"           // （可选参数）是否复用Codec的buffer。
      "device_id" : 0                       // 当使用MLU时为必设参数。设备id,用于标识多卡机器的设备唯一编号。
    }
  }

其他配置字段可以参考 ``data_source.hpp`` 中详细注释或者通过CNStream Inspect工具查看。

神经网络推理模块
---------------------------

神经网络推理（Inferencer）模块是基于寒武纪实时运行库（CNRT）的基础上，加入多线程并行处理及适应网络特定前后处理模块的总称。用户根据业务需求，只需载入客制化的模型，即可调用底层的推理。根据业务特点，该模块支持多batch及单batch推理，具体可参阅代码实现。

使用说明
^^^^^^^^^^^^^^^^^

在 ``detection_config.json`` 配置文件中进行配置，如下所示。该文件位于 ``cnstream/samples/demo`` 目录下。

::

  "detector" : {
    "class_name" : "cnstream::Inferencer",    // （必设参数）推理类名。               
    "parallelism" : 2,                       // （必设参数）并行度。 
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

追踪模块
---------------

追踪模块（Tracker）用于对检测到的物体进行追踪。主要应用于车辆行人等检测物的追踪。目前支持FeatureMatch和KCF两种追踪方法。该模块连接在神经网络推理模块后，通过在配置文件中指定追踪使用的离线模型以及使用的追踪方法来配置模块。

使用说明
^^^^^^^^^

配置追踪模块所需要的离线模型和追踪方法等。

   ::
 
     “tracker” : {
     “class_name” : “cnstream::Tracker”,       // （必设参数）Track的类名。
     “parallelism” : 4,                        // （必设参数）并行度。
     “max_input_queue_size” : 20,              // （必设参数）数据输入队列长度。
     “next_modules” : [“osd”],                 // （必设参数）下一个连接的模块名。
     “custom_params” : {
         // （必设参数）追踪使用的离线模型的路径。该参数支持绝对路径和相对路径。相对路径是相对于JSON配置文件的路径。
         “model_path” : “xxx.cambricon”,  
         “func_name” : “subnet0”,    // 如果设置将使用MLU，如果不设置将使用CPU。模型函数名。
         “track_name” : “KCF”       // （可选参数）追踪方法。支持FeatureMatch和KCF两种追踪方法。
         }
     }
    
OSD模块
---------------

OSD（On Screen Display）模块用于在图像上绘制对象，并输出图像为BGR24格式。

OSD模块可以连接在下面模块后面，绘制需要的推理结果：

- 神经网络推理模块（Inferencer）
- 追踪模块（Tracker）

OSD模块后面可以连接下面模块，实现不同功能：

- RTSP模块（RtspSink）：进行编码和RTSP推流。
- 展示模块（Displayer）：对结果进行展示。
- 编码模块（Encode）：编码成视频或者图片。

使用说明
^^^^^^^^^

例如在 ``ssd_resnet34_and_resnet50_mlu270_config.json`` 配置文件中配置一个包含二级网络的推理过程，即包含两个推理模块。该文件位于 ``cnstream/samples/demo/secondary/`` 目录下。

::

  "class_name" : "cnstream::Osd",
    "parallelism" : 4,
    "max_input_queue_size" : 20,
    "next_modules" : ["rtsp_sink"],
    "show_perf_info" : true,
    "custom_params" : {
      "label_path" : "../../../data/models/MLU270/Primary_Detector/ssd/label_voc.txt",
      "font_path" : "../../data/wqy_zenhei.ttf", 
      "label_size" : "normal",
      "text_scale" : 1,
      "text_thickness" : 1,
      "box_thickness" : 1,
      "secondary_label_path" : "../../../data/models/MLU270/Classification/resnet50/synset_words.txt",
      "attr_keys" : "classification",
      "logo" : "cambricon"  
    }
  }

配置文件中参数说明如下：

- class_name：（必设参数）模块名称。

- parallelism：（必设参数）模块并行度。

- max_input_queue_size：（必设参数）数据输入队列长度。

- next_modules：（必设参数）下一个连接模块名称。

- show_perf_info：（可选参数）是否显示模块信息。

- label_path：（可选参数）标签路径。对应一级网络的标签路径。

- font_path：（可选参数）字体路径。使用的 ``wqy_zenhei.ttf`` 文件需要自行下载，作为正确输出中文标签。

- label_size：（可选参数）标签大小，默认值为 ``normal``。可设置的值包括：
  
  - normal：正常标签。
  - large：大标签。
  - larger：加大标签。
  - small：小标签。
  - smaller：较小标签。
  - 直接为数字，如1、2等。

- text_scale：（可选参数）字体大小，默认值为1。

- text_thickness：（可选参数）字体宽度，默认值为1。

- box_thickness：（可选参数）标识框宽度，默认值为1。设置label_size后可分别设置text_scale、text_thickness、box_thickness大小调节。也可以只设置label_size为其他缺省。

- secondary_label_path：（可选参数）二级标签路径。对应二级网络的标签路径。

- attr_keys：（可选参数）显示二级标签中某个关键特征。
  该属性必须结合二级网络的后处理过程。例如二级网络对车辆进行识别，识别出车的类别，车的颜色两个特征。同时在后处理时类别标记为 ``classification``，颜色标记为 ``color``。通过显示包含关键字 ``classification`` 可以输出车辆类别，也可以同时包含 ``classification`` 和 ``color`` 输出类别和颜色两个标签。

- logo：（可选参数）打印logo的名称。例如 ``cambricon`` 可以在每帧图像右下角添加名称为 ``cambricon`` 的水印。

Encode模块
---------------

Encode为编码模块，主要用于编码视频和图像。

编码模块可以连接在下面模块后面，对视频和图像进行编码：

- 数据源模块（cnstream::DataSource）
- 神经网络推理模块（cnstream::Inferencer）
- 追踪模块（cnstream::Tracker）
- OSD模块（cnstream::Osd）

编码模块一般作为最后一个模块，后面不再连接其他模块。

使用说明
^^^^^^^^^

例如 ``encode_config.json`` 配置文件，如下所示。该文件位于 ``cnstream/samples/demo/encode/`` 目录下。

::

  "encode" : {
    "class_name" : "cnstream::Encode",
    "parallelism" : 2,
    "max_input_queue_size" : 20,
    "show_perf_info" : true,
    "custom_params" : {
      "encoder_type" : "mlu",
      "codec_type" : "h264",
      "preproc_type" : "cpu",
      "use_ffmpeg" : "false",
      "dst_width": 1280,
      "dst_height": 720,
      "frame_rate" : 25,
      "kbit_rate" : 3000,
      "gop_size" : 30,
      "output_dir" : "./output",
      "device_id": 0
    }
  }

配置参数说明如下：

- class_name：（必设参数）模块名称。

- parallelism：（必设参数）模块并行度。

- max_input_queue_size：（必设参数）数据输入队列长度。

- show_perf_info：（可选参数）是否显示模块信息。

- encoder_type：（可选参数）编码类型。可设置的值包括：

  - cpu：使用CPU编码（默认值）。
  - mlu：使用MLU编码。

- codec_type：（可选参数）编码的格式。可设置的值包括：

  - h264（默认值）
  - h265
  - jpeg

- preproc_type：（可选参数）前处理使用的类型是cpu还是mlu。可设置的值包括：

  - cpu(默认值)
  - mlu(目前不支持)

- use_ffmpeg：（可选参数）是否使用ffmpeg进行大小调整和色彩空间转换。可设置的值包括：

  - true 
  - false(默认值)

- dst_width：（可选参数）输出图像宽度，单位像素。注意当使用mlu解码时，设置输出图像宽度不为奇数。

- dst_height：（可选参数）输出图像高度，单位像素。注意当使用mlu解码时，设置输出图像高度不为奇数。

- frame_rate：（可选参数）编码后视频的帧率。默认值为25。

- kbit_rate：（可选参数）单位时间内编码的数据量。默认值为1Mbps，仅当在mlu上编码时才有效。较高的比特率表示视频质量较高，但相应编码速度较低。

- gop_size：（可选参数）表示连续的画面组的大小。与两个关键帧I-frame相关。默认值30。

- output_dir：（可选参数）视频解码后保存的地址。如果不指定，则不显示保存解码后视频或图片。默认值为 ``{CURRENT_DIR}/output``.

- device_id：（可选参数）当 ``encoder_type`` 或者 ``preproc_type`` 设置为 ``mlu`` 时，必须指定设备的id。


Display模块  
---------------

Display模块是CNStream中基于SDL视频播放插件开发的多媒体展示模块。使用该模块可以略过视频流编码过程，对Pipeline中处理完成的视频流进行实时的播放展示。

Display模块支持客户在播放视频过程中，选择播放窗口大小，并且支持全屏播放功能。对于不同路的视频流可以做到同时进行播放，可以通过设置 ``max-channels`` 对播放的视频流个数进行设置。该值若是小于输入视频个数，则只会展示前几路输入视频流。

Display模块一般连接下面模块后面：

- 神经网络推理模块（Inferencer）
- 追踪模块（Tracker）
- OSD模块（Osd）

Display模块一般作为最后一个模块，后面不再连接其他模块。


使用说明
^^^^^^^^^

例如 ``detection_config.json`` 配置文件如下，该文件位于 ``cnstream/samples/demo/`` 目录下。配置Display模块所需要的窗口大小、视频流个数以及刷新帧率等参数。Display模块和Encode模块一样，处于Pipeline最后位置的模块，所以不需要 ``next_modules`` 参数的设置。

::
 
     “displayer” : {
     “class_name” : “cnstream::Displayer”,     
     “parallelism” : 4,                        
     “max_input_queue_size” : 20,              
     “custom_params” : {
         // 设置视频流播放时的窗口大小和刷新帧率等信息。
         “window-width” : “1920”, 
         “window-height” : “1080”, 
         “refresh-rate” : “25”,   
         "max-channels" : "32",     
         "show" : "false",      
         "full-screen" : "false" 
         }
     }

配置参数说明如下：

- class_name：（必设参数）模块名称。

- parallelism：（必设参数）模块并行度。

- max_input_queue_size：（必设参数）数据输入队列长度。

- window-width：（必设参数）播放窗口的宽度，单位像素。

- window-height：（必设参数）播放窗口的高度，单位像素。

- refresh-rate：（必设参数）播放时的刷新帧率。

- max-channels：（必设参数）播放的视频流路数。建议大于等于输入视频流路数。

- show：（必设参数）是否播放视频流，设置为 ``true`` 之前需要确保顶层 ``CMakeList.txt`` 文件中 ``build_display`` 选项设置为 ``ON``。

- full-screen：（可选参数）是否进行全屏播放。

.. _rstp_sink:

RTSP Sink模块
---------------

RTSP（Real Time Streaming Protocol）Sink模块主要用于对每帧数据进行预处理，将图调整到需要的大小，并进行编码及RTSP推流。

RTSP Sink模块提供single模式和mosaic模式来处理数据流。single模式下，每个窗口仅显示一路视频，如16路视频会有16个端口，每个端口打开都是一个窗口，显示对应路的视频流。而mosaic模式下，多路视频仅有一个端口，所有路的视频都在一个窗口上显示。如16路视频只有一个端口，打开这个端口，显示的是4×4的拼图。

RTSP Sink模块处理数据流程如下：

.. figure:: ../images/rtsp_sink.png

   RTSP Sink模块数据处理流程

使用说明
^^^^^^^^^

用户可以通过配置JSON文件方式设置和使用RTSP Sink模块。JSON文件的配置参数说明如下：

- color_mode：（可选参数）颜色空间。可设置的值包括：

  -  bgr：输入为BGR。
  -  nv：输入为YUV420NV12或YUV420NV21。（默认值）

- preproc_type：（可选参数）预处理 (resize)。可设置的值包括：

  -  cpu：在CPU上进行预处理。（默认值）
  -  mlu：在MLU上进行预处理。(暂不支持)

- encoder_type：（可选参数）编码。可设置的值包括：

  -  ffmpeg：在CPU上使用ffmpeg进行编码。
  -  mlu：在MLU上进行编码。（默认值）

- device_id：（可选参数）设备号。仅在使用MLU时生效。默认使用设备0。

- view_mode：（可选参数）显示界面。可设置的值包括：

  -  single：single模式，每个端口仅显示一路视频，不同路视频流会被推到不同的端口。（默认值）
  -  mosaic：mosaic模式，实现多路显示。根据参数 ``view_cols`` 和 ``view_rows`` 的值，将画面均等分割，默认为4*4。

     .. attention::
        |  使用mosaic模式时，注意下面配置：

           - ``view_cols`` * ``view_rows`` 必须大于等于视频路数。以2*3为特例，画面将会分割成1个主窗口（左上角）和5个子窗口。
           - mosaic模式仅支持BGR输入。

- view_cols：（可选参数）多路显示列数。仅在mosaic模式有效。取值应大于0。默认值为0。

- view_rows：（可选参数）多路显示行数。仅在mosaic模式有效。取值应大于0。默认值为0。

- udp_port：（可选参数）UDP端口。格式为：
 
  ``url=rtsp://本机ip:9554/rtsp_live``。
  
  运行 示例代码_，URL将保存在文件 ``RTSP_url_names.txt`` 中。默认值为9554。

- http_port：（可选参数）RTSP-over-HTTP隧道端口。默认值为8080。

- dst_width：（可选参数）输出帧的宽。取值为大于0，小于原宽。只能向下改变大小。默认值为0（原宽）。

- dst_height：（可选参数）输出帧的高。取值为大于0，小于原高。只能向下改变大小。默认值为0（原高）。

- frame_rate：（可选参数）编码视频帧率。取值为大于0。默认值为25。

- kbit_rate：（可选参数）编码比特率。单位为kb，需要比特率/1000。取值为大于0。默认值为1000。

- gop_size：（可选参数）GOP（Group of Pictures），两个I帧之间的帧数。取值为大于0。默认值为30。

配置文件示例
^^^^^^^^^^^^^^^^

**Single模式**

::

   "rtsp_sink" : {
       "class_name" : "cnstream::RtspSink",
       "parallelism" : 16,
       "max_input_queue_size" : 20,
       "custom_params" : {
         "http_port" : 8080,
         "udp_port" : 9554,
         "frame_rate" :25,
         "gop_size" : 30,
         "kbit_rate" : 3000,
         "view_mode" : "single",
         "dst_width" : 1920,
         "dst_height": 1080,
         "color_mode" : "bgr",
         "encoder_type" : "ffmpeg",
         "device_id": 0
       }
     }


**Mosaic模式**

::

   "rtsp_sink" : {
       "class_name" : "cnstream::RtspSink",
       "parallelism" : 1,
       "max_input_queue_size" : 20,
       "custom_params" : {
         "http_port" : 8080,
         "udp_port" : 9554,
         "frame_rate" :25,
         "gop_size" : 30,
         "kbit_rate" : 3000,
         "encoder_type" : "ffmpeg",
         "view_mode" : "mosaic",
         "view_rows": 2,
         "view_cols": 3,
         "dst_width" : 1920,
         "dst_height": 1080,
         "device_id": 0
       }
     }

.. _示例代码:

示例代码
^^^^^^^^^

CNStream提供两个示例，位于CNStream github仓库 ``samples/demo/rtsp`` 目录下：

- run_rtsp.sh：示例使用single模式。对应配置文件 ``RTSP.json``。
- run_rtsp_mosaic.sh：示例使用mosaic模式。对应配置文件 ``RTSP_mosaic.json``。

**运行示例代码**

执行下面步骤运行示例代码：

1. 运行run_rtsp.sh或run_rtsp_mosaic.sh脚本。
2. 使用VLC Media Player打开生成的URL。例如：``rtsp://本机ip:9554/rtsp_live``。URL保存在 ``samples/demo/rtsp`` 目录下的 ``RTSP_url_names.txt`` 文件中。

.. _单进程单Pipeline:

单进程单Pipeline中使用多个设备
---------------------------------

在单进程、单个pipeline场景下，CNStream支持不同模块在不同的MLU卡上运行。用户可以通过设置模块的 ``device_id`` 参数指定使用的MLU卡。

下面以Decode和Inference模块使用场景为例，配置Decode模块使用MLU卡0，Inference模块使用MLU卡1。单进程中建议在source module中复用codec的buffer，即应设置 ``reuse_codec_buf`` 为 ``true``。

::

  {
    "source" : {
      "class_name" : "cnstream::DataSource",	// 数据源类名。
      "parallelism" : 0,			// 并行度。无效值，设置为0即可。
      "next_modules" : ["ipc"],		        // 下一个连接模块名称。
      "custom_params" : {			// 特有参数。
        "source_type" : "ffmpeg",	        // source类型。
        "reuse_cndec_buf" : "true",	        // 是否复用codec的buffer。
        "output_type" : "mlu",		        // 输出类型，可以设置为MLU或CPU。
        "decoder_type" : "mlu",		        // decoder类型，可以设置为MLU或CPU。
        "device_id" : 0			        // 设备id，用于标识多卡机器的设备唯一标号。
      }
    },

     "infer" : {				
      "class_name" : "cnstream::Inferencer",	// 推理类名。
      "parallelism" : 1,			// 并行度。
      "max_input_queue_size" : 20,		// 最大队列深度。
      "custom_params" : {			// 特有参数。
        //模型路径。
        "model_path" : "../../data/models/MLU270/Classification/resnet50/resnet50_offline.cambricon",
        "func_name" : "subnet0",		     // 模型函数名。
        "postproc_name" : "PostprocClassification",  // 后处理类名。
        "batching_timeout" : 60,		     // 攒batch的超时时间。
        "device_id" : 1				     // 设备id，用于标识多卡机器的设备唯一标号。
  }
    }
  }  

了解如何在多进程、单个pipeline下使用多个设备，查看 多进程_。

.. _多进程:

多进程操作
---------------

由于pipeline只能进行单进程操作，用户可以通过ModuleIPC模块将pipeline拆分成多个进程，并完成进程间数据传输和通信，例如最常见的解码和推理进程分离等。ModuleIPC模块继承自CNStream中的Module类。两个ModuleIPC模块组成一个完整的进程间通信。此外，通过定义模块的 ``memmap_type`` 参数，可以选择进程间的内存共享方式。

CNStream支持在单个pipeline中，不同的进程使用不同的MLU卡执行任务。用户可以通过设置模块的 ``device_id`` 参数指定使用的MLU设备。

使用示例
^^^^^^^^^

下面以进程1做解码，进程2做推理为例，展示了如何使用ModuleIPC模块完成多进程设置和通信，以及设置各进程使用不同的MLU卡。

1. 创建配置文件，如 ``config_process1.json``。在配置文件中设置进程1。第一个模块配置为解码模块，然后设置一个ModuleIPC模块。主要参数设置如下：
   
   - 设置 ``ipc_type`` 参数值为 **client**，做为多进程通信的客户端。
   - 设置 ``memmap_type`` 参数值为 **cpu**。当前仅支持CPU内存共享方式。后续会支持MLU内存共享方式。
   - 设置 ``socket_address`` 参数值为进程间通信地址。用户需定义一个字符串来表示通信地址。
   - 设置不同进程使用不同的MLU卡：设置Decode进程使用MLU卡0。但配置ModuleIPC模块时，无需设置 ``device_id``。另外，多进程使用中，不建议在source module中复用codec的buffer，即应设置 ``reuse_codec_buf`` 设为false。
   
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
     

2. 创建配置文件，如 ``config_process2.json``。在配置文件中设置进程2。第一个模块配置为ModuleIPC模块，然后设置一个推理模块。主要参数设置如下：
   
   - 在ModuleIPC模块中，设置 ``ipc_type`` 参数值为 **server**，做为多进程通信的服务器端。
   - 在ModuleIPC模块中，设置 ``memmap_type`` 参数值为 **cpu**。当前仅支持CPU内存共享方式。后续会支持MLU内存共享方式。
   - 在ModuleIPC模块中，设置 ``socket_address`` 参数值为进程间通信地址。用户需定义一个字符串来表示通信地址。
   - 设置不同进程使用不同的MLU卡：设置Inference进程使用MLU卡1。但配置ModuleIPC模块时，需要指定 ``device_id``。该 ``device_id`` 的值应与推理模块设置的 ``device_id`` 的值保持一致。
   
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
           "socket_address" : "test_ipc",       // 进程间通信地址，一对通信的进程，需要设置为相同的通信地址。
           "device_id":1                        // 设备id，用于标识多卡机器的设备唯一标号。 
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
           "device_id" : 1				  // 设备id，用于标识多卡机器的设备唯一标号。
         }
       }
     }










