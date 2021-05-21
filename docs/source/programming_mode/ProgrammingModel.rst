.. cnstream programming model

编程模型
======================

本章重点介绍了CNStream在寒武纪软件栈是如何工作的，也介绍了文件目录以及如何快速开始使用内置模块进行编程。

寒武纪软件栈
-------------

CNStream作为寒武纪视频结构化分析特定领域的框架，在整个寒武纪应用软件栈中起着承上启下的作用。CNStream能快速构建自己的视频分析应用，并获得比较高的执行效率。用户无需花费精力在一些底层的细节上，从而有更多时间关注业务的发展。下图展示了CNStream在软件栈中的位置关系。


    .. figure::  ../images/structure.*
       :align: center
	   
       寒武纪软件栈框图

文件目录
----------

在CNStream源码目录下，主要由以下部分组成：

* 核心框架：在 ``framework/core`` 文件夹下，包含创建Pipeline，Module等基础核心源码。
* 内置模块：在 ``modules`` 文件夹下，内置一套标准的视频结构化模块组件。
* 动态库：编译成功后，CNStream核心框架存放在 ``libcnstream_core.so`` 文件中，内置的视频结构化模块存放在 ``libcnstream_va.so`` 中。位于 ``lib`` 目录下。
* 示例程序源码：一系列示例程序存放在 ``samples`` 文件夹下。
* 样例数据：示例程序和测试程序所用到的数据文件，包括图片、短视频等。另外程序运行过程中从Model Zoo下载的离线模型文件也会存放在该文件夹下。

CNStream仓库主要目录结构及功能如下图所示：

    .. figure::  ../images/dir.png
       :align: center

       CNStream文件目录结构图

.. _programmingguide:

编程指南
---------

CNStream是典型的基于pipeline和模块机制的编程模型。支持在pipeline注册多个 :ref:`内置模块` 或者 :ref:`自定义模块`。这些模块通过隐含的队列连接，使用一个JSON的文本描述模块间的连接关系。

应用开发的通用编程步骤如下：

  #. 创建一个pipeline对象。
  #. 读入预先编排的JSON文件构建数据流pipeline。
  #. 创建消息监测模块，并注册到pipeline。
  #. 启动pipeline。
  #. 动态增加数据源。
  
详情请查看 :ref:`application`。
