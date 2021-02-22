
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
