.. FAQ

FAQ
==================

.. _filelist:

file_list文件是做什么用的？
-----------------------------

文本文件 ``file_list`` 的用于存储视频或图片的路径。文件中，每一行代表一路视频或者图片的URL，可以是本地视频文件路径、RTSP或RTMP地址等。执行 ``run.sh`` 脚本时， ``file_list`` 文件会被调用并传入应用程序。当首次执行 ``run.sh`` 脚本时，该脚本会自动生成 ``files.list_image`` 和 ``files.list_video`` 文件。 ``files.list_image`` 文件用于存放一组JPEG图片路径。 ``files.list_video`` 文件用于存放两路视频路径。

用户也可自己创建一个文件来存放存储视频或图片的路径。但是需要将文件名设置为 ``run.sh`` 脚本中 ``data_path`` 参数的值。该脚本存放于 ``cnstream/samples/demo`` 目录下。

常见几种file_list内容格式如下：

* file list中存放本地视频文件，内容如下：

  ::

    /path/of/videos/1.mp4
    /path/of/videos/2.mp4
    ...

* file list中存放RTSP视频流地址，内容如下：

  ::

     rtsp://ip:port/1
     rtsp://ip:port/2
     ...

* file list中存放图片，每一行为一组JPG图片路径，通配符遵循FFMpeg AVformat匹配规则，如%02d.jpg，%*.jpg等。内容如下:

  ::

    /path/of/%d.jpg
    /path/of/%d.jpg
    ...

怎么输入任意命名的图片？
-----------------------------

目前CNStream支持Jpeg图片解码，可以通过file_list中添加字段进行通配符匹配，这种使用方式对图片源输入来说并不灵活。CNStream同时还支持输入任意名字的图片，通过fopen或者cv::imread事先将图片读入内存，然后再把该内存数据喂入Pipeline中进行后续处理。具体细节可以参考
``cnstream/samples/demo/demo.cpp`` 文件中的函数  ``AddSourceForDecompressedImage`` 和 ``AddSourceForImageInMem`` 内容。


有没有交叉编译CNStream的指导？
-----------------------------

可以参考第三方开发人员提供步骤 https://github.com/CambriconKnight/mlu220-cross-compile-docker-image


parallelism 参数该怎么配置？
-----------------------------

parallelism是模块内并行度，表明有多少个线程在同时运行 ``Module::Process`` 函数。 总体上该值越大，并行度越高，流水线处理能力越强，占用资源也越多。

* parallelism值应不大于数据流路数，否则会有线程空挂，造成资源浪费。同时增大该值时需要时刻关注系统资源是否够用。

* 对于数据源插件，parallelism设置为0即可, 因为数据源插件的并行度是由输入数据路数决定的。

* 对于Inference插件，parallelism建议小于硬件核数与离线模型核数之比。

* 其余插件可以根据具体性能进行调整，比如某个插件性能较低，那么可以增大其parallelism值提高处理速率。如果当前性能已经远远超出pipeline上其他插件，那么可以减小该值以减少资源占用。
