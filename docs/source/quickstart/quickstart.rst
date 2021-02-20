.. _quickstart:

快速入门
================

本章重点介绍了如何配置和编译CNStream，以及如何运行寒武纪提供的CNStream示例。

更多CNStream详细介绍：

- CNStream详细概念和功能介绍，参考 :ref:`overview`。
- 编程指南详细介绍，参考 :ref:`programmingguide`。
- 创建应用的操作指南，参考 :ref:`application`。

.. _install:

安装和配置环境依赖和依赖库
----------------------------

用户需要安装和配置环境依赖和寒武纪Neuware安装包后使用CNStream。本节描述了如何在Debian、Ubuntu、CentOS以及Docker环境下配置CNStream。

.. _环境依赖:

环境依赖
^^^^^^^^^^^^^

CNStream有以下环境依赖。

- OpenCV2.4.9+
- GFlags2.1.2
- GLog0.3.4
- CMake2.8.7+
- SDL2 2.0.4+

用户需要配置所有环境依赖后再使用CNStream。

.. _寒武纪依赖库:

寒武纪安装包
^^^^^^^^^^^^^

CNStream的使用依赖于寒武纪CNToolkit安装包中CNRT库和CNCodec库。CNToolkit安装包是寒武纪公司发布的基于寒武纪硬件产品的神经网络开发工具包。用户需要在使用CNStream之前安装寒武纪CNToolkit安装包。发送邮件到 service@cambricon.com，联系寒武纪工程师获得CNToolkit安装包和安装指南。

Ubuntu环境下安装和配置
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

执行下面命令，在Ubuntu环境下安装和配置环境依赖和依赖库：

1.  运行下面指令从github仓库检出CNStream源码。``${CNSTREAM_DIR}`` 代表CNStream源码目录。

    ::

        git clone https://github.com/Cambricon/CNStream.git
        git submodule update --init

#.  安装寒武纪CNToolkit安装包。详情查看 :ref:`寒武纪依赖库` 。

#.  运行下面指令安装环境依赖。CNStream依赖的环境详情，查看 :ref:`环境依赖`。

    用户可通过 ``${CNSTREAM_DIR}/tools`` 下的 ``pre_required_helper.sh`` 脚本进行安装：

    ::

        cd ${CNSTREAM_DIR}/tools
        ./pre_required_helper.sh

    或者通过以下命令进行安装：

    ::

        sudo apt-get install libopencv-dev libgflags-dev libgoogle-glog-dev cmake
        sudo apt-get install libfreetype6 ttf-wqy-zenhei libsdl2-dev curl

CentOS环境下安装和配置
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

执行下面步骤，在CentOS环境下安装和配置环境依赖和依赖库：

1.  运行下面指令从github仓库检出CNStream源码。``${CNSTREAM_DIR}`` 代表CNStream源码目录。

    ::

        git clone https://github.com/Cambricon/CNStream.git
        git submodule update --init

#.  安装寒武纪CNToolkit安装包。详情查看 :ref:`寒武纪依赖库`。

#.  运行下面指令安装环境依赖。CNStream依赖的环境详情，查看 :ref:`环境依赖`。

    用户可通过 ``${CNSTREAM_DIR}/tools`` 下的 ``pre_required_helper.sh`` 脚本进行安装：

    ::

      cd ${CNSTREAM_DIR}/tools
      ./pre_required_helper.sh


    或者通过以下命令进行安装：

    ::

      sudo yum install opencv-devel.x86_64 gflags.x86_64 glog.x86_64 cmake3.x86_64
      sudo yum install freetype-devel SDL2_gfx-devel.x86_64 wqy-zenhei-fonts
      sudo yum install ffmpeg ffmpeg-devel curl


Docker环境下安装和配置
^^^^^^^^^^^^^^^^^^^^^^^

CNStream提供以下Dockerfile，其中``${CNSTREAM_DIR}`` 代表CNStream源码目录。

- ${CNSTREAM_DIR}/docker/Dockerfiler.16.04
- ${CNSTREAM_DIR}/docker/Dockerfiler.18.04
- ${CNSTREAM_DIR}/docker/Dockerfiler.CentOS

执行下面步骤使用Docker镜像配置独立于宿主机的开发环境：

1. 安装Docker。宿主机需要预先安装Docker。详情请查看Docker官网主页：https://docs.docker.com/    
2. 运行下面指令从github仓库检出CNStream源码。
 
   ::
          
      git clone https://github.com/Cambricon/CNStream.git
      git submodule update --init
 
3. 编译Docker镜像。用户可以选择以下其中一种方式编译镜像。

   -  如果选择将寒武纪CNToolkit包安装进镜像中：

      1. 运行下面命令，拷贝寒武纪CNToolkit安装包到CNStream源码目录下。
  
         ::
 
	        cp ${toolkit_package} CNStream
	  
      2. 运行下面命令将寒武纪CNToolkit安装包安装到镜像中，其中 ``${cntoolkit_package_name}`` 为寒武纪CNToolkit安装包及其存放路径。

         ::
	     
             docker build -f docker/Dockerfile --build-arg toolkit_package=${cntoolkit_package_name} -t ubuntu_cnstream:v1 

   -  如果选择不将寒武纪CNToolkit包安装进镜像中，运行下面命令编译镜像：

      ::
	     
             docker build -f docker/Dockerfile.18.04 -t ubuntu_cnstream:v1
			
4. 运行下面命令，开启容器：

   ::
   
     docker run -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY --privileged -v /dev:/dev --net=host --ipc=host --pid=host -v $HOME/.Xauthority -it --name container_name  -v $PWD:/workspace ubuntu_cnstream:v1

5. 如果之前制作的镜像没有安装寒武纪CNToolkit安装包，安装CNToolkit安装包。详情查看 :ref:`寒武纪依赖库` 。
     
.. _编译指令:

编译CNStream指令
-----------------------

完成环境依赖的部署以及依赖库的安装后，执行下面步骤编译CNStream指令：

1. 运行下面指令从github检出子仓easydk源码

   ::

      git submodule update --init

2. 运行下面命令创建 ``build`` 目录用来保存输出结果。

   ::
   
      mkdir build

3. 运行下面命令生成编译指令的脚本。``CNSTREAM_DIR`` 为CNStream源码目录。

   ::
  
     cd build
     cmake ${CNSTREAM_DIR}

4. 如果想要运行寒武纪提供的CNStream示例：

   1. 运行下面命令： 
     
      ::
 
         cmake -Dbuild_sample=ON ${CNSTREAM_DIR}
    
    2. 如果需要交叉编译，运行下面命令：

       ::

          cmake -DCMAKE_TOOLCHAIN_FILE=${CNSTREAM_DIR}/cmake/cross-compile.cmake ${CNSTREAM_DIR}
       
       .. attention::
          |  用户需要手动在 ``cross-compile.cmake`` 文件中配置toolchain。

5. 运行下面命令编译CNStream指令：

   ::

     make

CNStream开发样例
--------------------

寒武纪CNStream开发样例为用户提供了物体分类、检测、追踪、二级结构化、多进程、RTSP等场景的编程样例。另外还提供了前处理、后处理、自定义模块以及如何使用非配置文件方式创建应用程序的样例源码。帮助用户快速体验如何使用CNStream开发应用。用户只需直接通过脚本运行样例程序，无需修改任何配置。

样例介绍
^^^^^^^^^^^^

CNStream开发样例主要包括.json文件和.sh文件，其中.json文件为样例的配置文件，用于声明pipeline中各个模块的上下游关系以及配置模块的参数。用户可以根据自己的需求修改配置文件参数，完成应用开发。.sh文件为样例的运行脚本，通过运行该脚本来运行样例。

开发样例中的模型在运行样例时被自动加载，并且会保存在 ``${CNSTREAM_DIR}/data/models`` 目录下。

下面重点介绍CNStream提供的样例。样例支持在MLU270和MLU220 M.2平台上使用。

SSD目标检测样例
********************

SSD目标检测。

**样例文件**

- 配置文件：``${CNSTREAM_DIR}/samples/demo/detection_config.json``
- 运行脚本：``${CNSTREAM_DIR}/samples/demo/run.sh``
- 后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_ssd.cpp``

**使用模块**

- DataSource
- Inferencer
- Osd
- Displayer

YOLOv3网络目标检测样例
**************************

使用YOLOv3网络对目标物体进行检。

**样例文件**

- MLU270配置文件：``${CNSTREAM_DIR}/samples/demo/detection/mlu270/yolov3_mlu270_config.json``
- MLU270运行脚本：``${CNSTREAM_DIR}/samples/demo/detection/mlu270/run_yolov3_mlu270.sh``
- MLU220配置文件：``${CNSTREAM_DIR}/samples/demo/detection/mlu220/yolov3_mlu220_config.json``
- MLU220运行脚本：``${CNSTREAM_DIR}/samples/demo/detection/mlu220/run_yolov3_mlu220.sh``
- 后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_yolov3.cpp``

**使用模块**

- DataSource
- Inferencer
 
物体分类样例
********************

在MLU220 M.2平台上使用resnet18网络对物体分类。在MLU270平台上，使用resnet50网络对物体分类。

**样例文件**

- MLU270配置文件：``${CNSTREAM_DIR}/samples/demo/classification/mlu270/classification_resnet50_mlu270_config.json``                                                                  
- MLU270运行脚本：``${CNSTREAM_DIR}/samples/demo/classification/mlu270/run_resnet50_mlu270.sh``                                                          
- MLU270后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_yolov3.cpp``   
                             
- MLU220配置文件：``${CNSTREAM_DIR}/samples/demo/classification/mlu220/classification_resnet18_mlu220_config.json``                                                                        
- MLU220运行脚本：``${CNSTREAM_DIR}/samples/demo/classification/mlu220/run_resnet18_mlu220.sh``                                                            
- 预处理源码：``${CNSTREAM_DIR}/samples/demo/preprocess/preprocess_standard.cpp``                                                                
- MLU220后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_classification.cpp``                                         

**使用模块**

- DataSource
- Inferencer
 
物体追踪样例 
********************

物体目标追踪。

**样例文件**

- MLU270配置文件：``${CNSTREAM_DIR}/samples/demo/track/mlu270/yolov3_track_mlu270.json``        
- MLU270运行脚本：``${CNSTREAM_DIR}/samples/demo/track/mlu270/run_yolov3_track_mlu270.sh``      
- MLU220配置文件：``${CNSTREAM_DIR}/samples/demo/track/mlu220/track_SORT_mlu220_config.json``   
- MLU220运行脚本：``${CNSTREAM_DIR}/samples/demo/track/mlu220/run_track_SORT_mlu220.sh``        
- MLU220后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_fake_yolov3.cpp``      
- MLU270后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_yolov3.cpp``     

**使用模块**

- DataSource
- Inferencer
- Tracker
 
二级结构化样例
********************

先使用SSD检测目标物体，再在resnet50网络上对车辆进行分类。
 
**样例文件**

- MLU270配置文件：``${CNSTREAM_DIR}/samples/demo/ssd_resnet34_and_resnet50_mlu270_config.json``
- MLU270运行脚本：``${CNSTREAM_DIR}/samples/demo/secondary/run_ssd_and_resnet50_mlu270.sh``
- 车辆筛选车的策略源码：``${CNSTREAM_DIR}/samples/demo/obj_filter/car_filter.cpp``
- 后处理源码：

   - ``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_ssd.cpp``
   - ``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_classification.cpp`` 

**使用模块**

- DataSource
- Inferencer
- Osd
- RtspSink
 
RTSP视频流样例
********************

在多窗口多通道模式（single模式）和单窗口多通道（mosaic模式）下处理数据流。   
   
**样例文件** 

- 多窗口多通道模式的配置文件：``${CNSTREAM_DIR}/samples/demo/rtsp/RTSP.json``                     
- 多窗口多通道模式的运行脚本：``${CNSTREAM_DIR}/samples/demo/rtsp/run_rtsp.sh``                  
- 单窗口多通道模式的配置文件：``${CNSTREAM_DIR}/samples/demo/rtsp/RTSP_mosaic.json``              
- 单窗口多通道模式的运行脚本：``${CNSTREAM_DIR}/samples/demo/rtsp/run_rtsp_mosaic.sh``            
- 后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_ssd.cpp``  

**使用模块**

- DataSource
- Inferencer
- Osd
- RtspSink
 
多进程样例
********************

单个pipleline的多进程操作。
  
**样例文件**
 
- 进程1的配置文件：``${CNSTREAM_DIR}/samples/demo/multi_process/config_process1.json``          
- 进程1的运行脚本：``${CNSTREAM_DIR}/samples/demo/multi_process/run_process1.sh``               
- 进程2的配置文件：``${CNSTREAM_DIR}/samples/demo/multi_process/config_process2.json``          
- 进程2的运行脚本：``${CNSTREAM_DIR}/samples/demo/multi_process/run_process2.sh``               
- 多进程的运行脚本：``${CNSTREAM_DIR}/samples/demo/multi_process/run_multi_process.sh``          
- 后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_classification.cpp``  
 
如果想要进程1和进程2分别执行，并返回结果到不同的窗口，可以分别运行run_process1.sh和run_process2.sh。如果想要进程1和进程2的执行结果在同一个窗口显示，运行run_multi_process.sh。 

**使用模块**

- DataSource
- Inferencer
- Osd
- RtspSink
 
视频预处理源码
********************  
  
提供CPU常规的标准预处理和YOLO v3视频预处理源码。被其他样例调用。

**样例文件**

- CPU常规的标准预处理源码：``${CNSTREAM_DIR}/samples/demo/preprocess/preprocess_standard.cpp``           
- YOLO v3预处理源码：``${CNSTREAM_DIR}/samples/demo/preprocess/preprocess_yolov3.cpp`` 
 
如果想要自定义预处理，用户可以在该文件夹下加入预处理的源码。
 
视频后处理源码
********************

提供分类后处理、SSD后处理和YOLO v3后处理的源码。被其他样例调用。     

**样例文件**

- 分类后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_classification.cpp``    
- SSD后处理源码：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_ssd.cpp``               
- 标准YOLO v3后处理源码，按等比例缩放：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_yolov3.cpp``   
- YOLO v3后处理源码，采用拉伸模式：``${CNSTREAM_DIR}/samples/demo/postprocess/postprocess_fake_yolov3.cpp``
                                          
如果想要自定义后处理，用户可以在该文件夹下加入后处理的源码。

目标物体筛选源码
********************

二级结构化，筛选车的策略源码。被其他样例调用。

**样例文件**

车辆筛选源码：``${CNSTREAM_DIR}/samples/demo/obj_filter/car_filter.cpp``   
                                         
如果想要自定义筛选算法，用户可以在该文件夹下加入筛选的源码。

运行开发样例
^^^^^^^^^^^^^^

编译指令_ 后，执行样例中的.sh文件运行开发样例。例如，执行下面命令运行SSD目标检测样例：

::

  cd ${CNSTREAM_DIR}/samples/demo

  ./run.sh

