
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
