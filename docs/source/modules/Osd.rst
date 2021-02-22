
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
