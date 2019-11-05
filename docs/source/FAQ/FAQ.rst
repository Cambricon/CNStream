.. FAQ

FAQ
==================

file list
----------------

file_list为一个文本文件，其中每一行为一个视频流url（本地视频文件路径、rtsp、rtmp地址）或者图片路径,file list中一行代表一个视频流。

**下面列举几种常见的file_list内容。**

* file list 中存放本地视频文件,内容如下：

  ::

   /path/of/videos/1.mp4
   /path/of/videos/2.mp4
   ...
   ...
   ...

* file list 中存放rtsp视频流地址，内容如下：

  ::

   rtsp://ip:port/1
   rtsp://ip:port/2
   ...
   ...
   ...

* file list 中存放file list，该情况在samples中当使用图片作为输入时用到（参考samples中的input_image参数）。其中file list 中每一行为一个file list路径，嵌套在内的每个file list中每一行为一张jpg图片路径。可参考samples/inference/file_list文件的编写方式。

  `最外层file list内容如下：`

  ::

   /path/of/file_list1
   /path/of/file_list2
   ...
   ...
   ...
   /path/of/file_list1 与 /path/of/file_list2中的内容如下：
   /path/of/1.jpg
   /path/of/2.jpg
   ...
   ...
   ...

