
数据源模块
--------------
数据源（DataSource）模块是pipeline的起始模块，实现了视频图像获取功能。不仅支持获取内存数据裸流，还可以通过FFmpeg解封装、解复用本地文件或网络流来得到码流。之后喂给解码器解码得到图像，并把图像存到CNDataFrame的CNSyncedMemory中。目前支持H264、H265、MP4、JPEG、RTSP等协议。

.. attention::
   |  - 一个pipeline支持定义多个数据源模块,不同的模块分别处理不同设备上的数据，可参考示例程序multi_sources。
   |  - MLU220硬件平台上使用需要尤其注意apply_stride_align_for_scaler参数的使用。详情查看 apply_stride_align_for_scaler_。

数据源模块主要有以下特点：

- 作为pipeline的起始模块，没有输入队列。因此pipeline不会为DataSource启动和调度线程。数据源模块需要内部启动线程，通过pipeline的 ``ProvideData()`` 接口向下游发送数据。
- 每一路数据流由使用者指定唯一标识 ``stream_id`` 。
- 支持动态增加和减少数据流。
- 支持通过配置文件修改和选择数据源模块的具体功能，而不是在编译时选择。

**cnstream::DataSource** 类在 ``data_source.hpp`` 文件中定义。 ``data_source.hpp`` 文件存放在 ``modules/source/include`` 文件夹下。 ``DataSource`` 主要功能继承自 ``SourceModule`` 类，存放在 ``framework/core/include`` 目录下。主要接口如下，源代码中有详细的注释，这里仅给出必要的说明。

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

使用说明及参数详解
^^^^^^^^^^^^^^^^^^^

在 ``detection_config.json`` 配置文件中进行配置，如下所示。该文件位于 ``cnstream/samples/demo`` 目录下。

::
 
  "source" : {
    "class_name" : "cnstream::DataSource",  // （必设参数）数据源类名。
    "parallelism" : 0,                      // （必设参数）并行度。无效值，设置为0即可。
    "next_modules" : ["detector"],          // （必设参数）下一个连接模块的名称。
    "custom_params" : {                     // 特有参数。
      "output_type" : "mlu",                // （可选参数）输出类型。可设为MLU或CPU。
      "decoder_type" : "mlu",               // （可选参数）decoder类型。可设为MLU或CPU。
      "reuse_cndec_buf" : "false"           // （可选参数）是否复用Codec的buffer。
      "device_id" : 0                       // 当使用MLU时为必设参数。设备id,用于标识多卡机器的设备唯一编号。
    }
  }


decoder_type
''''''''''''''

字符串类型。指定解码器使用的硬件设备，可选值有cpu、mlu。默认为cpu。

- 当decoder_type值设置为cpu时表示使用CPU进行解码，CPU解码器输出数据在主机内存上。
- 当decoder_type值设置为mlu时表示使用MLU进行解码，MLU解码器输出数据在MLU设备内存上。

output_type
''''''''''''''

字符串类型。指定插件输出数据内存类型，可选值有cpu、mlu。默认为cpu。

- 当output_type值设置为cpu时表示插件将解码后的数据搬运到主机内存上。此时分为两种情况：

  - decoder_type为cpu，此时解码后原始数据在主机内存上，不进行数据搬运。
  
  - decoder_type为mlu，此时解码后原始数据在MLU设备内存上。在MLU270和MLU220 M.2平台将使用PCIE进行主机侧到设备侧的内存拷贝。在MLU220 EDGE平台将使用CPU进行内存拷贝。
  
- 当output_type值设置为mlu时表示插件将解码后的数据搬运到MLU设备内存上。此时分三种情况：

  - decoder_type为cpu，此时解码后原始数据在主机内存上。在MLU270和MLU220 M.2平台将使用PCIE进行主机测到设备侧的内存拷贝。在MLU220   EDGE平台将使用CPU进行内存拷贝。

  - decoder_type为mlu，且reuse_cndec_buf为false。此时解码后原始数据在MLU设备内存上。将使用MLU计算核心（IPU）进行内存拷贝。

  - decoder_type为mlu，且reuse_cndec_buf为true。此时解码后原始数据在MLU设备内存上，直接将解码后的数据指针向后传递使用，不进行内存拷贝。

reuse_cndec_buf
'''''''''''''''''''

设置是否开启解码器内存复用功能。可选值有true、false。默认为false。

解码器内存复用即解码后内存直接向后传递使用，不进行内存拷贝。需要配合decoder_type和output_type参数使用。只有当两个参数都设置为mlu且该参数设置为true时，解码器内存复用功能生效。

.. attention::
   |  使用解码器内存复用功能时需要注意output_buf_number、input_buf_number两个参数的设置。它们将影响pipeline性能、和MLU设备侧内存占用。

input_buf_number
'''''''''''''''''''

设置MLU解码器输入队列中最大缓存帧数。当decoder_type为mlu时生效。该值越大占用MLU设备侧内存越大。推荐值为4。

output_buf_number
'''''''''''''''''''

设置MLU解码器输出队列中最大缓存帧数。当对视频流解码时该值最大为32。当对JPEG进行解码时最大为16。

该值越大占用MLU设备内存越大。pipeline中数据流数越多，码流分辨率越大，图片分辨率越大，占用MLU设备内存越多。
受限于内存大小，则output_buf_number上限值越小。

- 当复用解码器内存功能打开时，基于性能考虑，推荐把该值尽可能的设置大。
- 当复用解码器内存关闭时，该值设置为大于码流参考帧数量一般就不会影响性能。

若设置的值过大，会导致创建解码器失败。

interval
'''''''''''''''''

插件丢帧策略。指定每interval帧数据帧输出一帧，剩余的帧将被丢弃。默认为1（即不丢帧）。最小值为1，最大值为size_t类型最大值。

例如，interval为3。解码后输出7帧。则第1帧和第4帧和第7帧将被传递到后续模块，其余帧将被丢弃。

.. _apply_stride_align_for_scaler:

apply_stride_align_for_scaler
''''''''''''''''''''''''''''''''

指定解码后输出按scaler硬件的要求进行对齐。在使用MLU解码时，输出的NV12/NV21数据将按照128像素对齐，即解码后的yuv数据stride为128的倍数。

可选值为true、false，默认值为false。MLU220硬件平台考虑使用该参数，其它硬件平台不推荐使用该参数为true。

device_id
''''''''''''''

设置使用的设备id，决定MLU解码使用的设备及解码后数据存放在哪张MLU卡上。

