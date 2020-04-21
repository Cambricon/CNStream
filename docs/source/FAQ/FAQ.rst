.. FAQ

FAQ
==================

file_list文件是做什么用的？
--------------------------

文本文件 ``file_list`` 的用于存储视频或图片的路径。文件中，每一行代表一路视频或者图片的URL，可以是本地视频文件路径、RTSP或RTMP地址等。执行 ``run.sh`` 脚本时， ``file_list`` 文件会被调用并传入应用程序。当首次执行 ``run.sh`` 脚本时，该脚本会自动生成 ``files.list_image`` 和 ``files.list_video`` 文件。 ``files.list_image`` 文件用于存放一组JPEG图片路径。 ``files.list_video`` 文件用于存放两路视频路径。

用户也可自己创建一个文件来存放存储视频或图片的路径。但是需要将文件名设置为 ``run.sh`` 脚本中 ``data_path`` 参数的值。该脚本存放于 ``cnstream/samples/demo``目录下。

常见几种file_list内容格式如下：

* file list中存放本地视频文件，内容如下：

  ::

    /path/of/videos/1.mp4
    /path/of/videos/2.mp4
    ...
    ...
    ...

* file list中存放RTSP视频流地址，内容如下：

  ::

     rtsp://ip:port/1
     rtsp://ip:port/2
     ...
     ...
     ...

* file list中存放图片，每一行为一组JPG图片路径，内容如下:

  ::

    // 最外层file list内容如下：
    /path/of/%d.jpg
    /path/of/%d.jpg
    ...
    ...
    ...

