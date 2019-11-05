.. cnstream programming model

CNStream编程模型
======================

寒武纪软件栈
-------------

CNStream作为寒武纪视频结构化分析特定领域的框架，在整个寒武纪应用软件栈中起着承上启下的作用。CNStream能快速构建自己的视频分析应用，并获得比较高的执行效率。用户无需花费精力在一些底层的细节上，从而有更多时间关注在业务的发展。下图展示了CNStream在软件栈中的位置关系：


    .. figure::  ../images/structure.*
       :align: center
	   
       寒武纪软件栈框图

依赖库
-------

用户可以在MLU文件夹下找到依赖的头文件和依赖库。

环境依赖
^^^^^^^^^^^^^
CNStream有以下环境依赖：

- OpenCV2.4.9+
- GFlags2.1.2
- GLog0.3.4
- Cmake2.8.7+

**Ubuntu or Debian**

如果用户使用Ubuntu或者Debian，执行下面命令：

::

  OpenCV2.4.9+  >>>>>>>>>   sudo apt-get install libopencv-dev
  GFlags2.1.2   >>>>>>>>>   sudo apt-get install libgflags-dev
  GLog0.3.4     >>>>>>>>>   sudo apt-get install libgoogle-glog-dev
  Cmake2.8.7+   >>>>>>>>>   sudo apt-get install cmake

**Centos**

如果用户使用Centos，执行下面命令：

::

  OpenCV2.4.9+  >>>>>>>>>   sudo yum install opencv-devel.i686
  GFlags2.1.2   >>>>>>>>>   sudo yum install gflags.x86_64
  GLog0.3.4     >>>>>>>>>   sudo yum install glog.x86_64
  Cmake2.8.7+   >>>>>>>>>   sudo yum install cmake3.x86_64

文件目录
----------

在CNStream SDK目录下，主要有三个部分：头文件，动态库和示例程序源码。

* 头文件：include的文件夹下存放的是CNStream的头文件，包含所有的数据类型和接口。
* 动态库：lib64的文件夹的libcnstream.so是CNStream动态库。
* 示例程序源码：samples文件夹存放的是一系列示例程序。

CNStream文件目录结构如下图所示：

    .. figure::  ../images/dir.*
       :align: center

       CNStream文件目录结构图

编程模型
---------

CNStream是典型的基于Pipeline和插件机制的编程模型。支持在Pipeline注册多个预置插件或者自定义的插件。这些插件之间通过隐含的深度可控的队列连接，可以使用一个JSON的文本描述组件间的连接关系。

通用应用编程步骤如下：

  #. 创建一个Pipeline对象。
  #. 读入预先编排的json文件构建数据流Pipeline。
  #. 创建消息监测模块，并设置到Pipeline。
  #. 启动pipeline。
  #. 动态增加数据源。