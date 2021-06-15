
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
