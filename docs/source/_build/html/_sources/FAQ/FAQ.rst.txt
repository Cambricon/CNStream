.. FAQ

FAQ
==================

什么是file list？
--------------------

文本文件file_list的每一行代表一路视频或者图片的URL，可以是本地视频文件路径、RTSP或RTMP地址等。
当首次执行run.sh脚本时，该脚本会自动生成files.list_image和files.list_video。前者存放一组JPEG图片路径，后者存放两路视频路径。用户也可自己新建一个文件并在文件中添加相应的URL，然后通过‘data_path’参数将该文件名传入应用程序，具体用法参见run.sh脚本内容。

常见几种file_list内容如下：

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

