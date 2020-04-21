.. _install:

环境配置
================

本章介绍了如何在Debian、Ubuntu、CentOS以及Docker环境下配置CNStream。

依赖库
-------
	   
.. _环境依赖:

环境依赖
^^^^^^^^^^^^^

CNStream有以下环境依赖。用户可以在MLU文件夹下找到环境依赖的头文件和依赖库。



- OpenCV2.4.9+
- GFlags2.1.2
- GLog0.3.4
- Cmake2.8.7+

**Ubuntu** 或 **Debian**

如果用户使用Ubuntu或者Debian，执行下面命令安装环境依赖：

::

  OpenCV2.4.9+  >>>>>>>>>   sudo apt-get install libopencv-dev
  GFlags2.1.2   >>>>>>>>>   sudo apt-get install libgflags-dev
  GLog0.3.4     >>>>>>>>>   sudo apt-get install libgoogle-glog-dev
  Cmake2.8.7+   >>>>>>>>>   sudo apt-get install cmake

**Centos**

如果用户使用Centos，执行下面命令安装环境依赖：

::

  OpenCV2.4.9+  >>>>>>>>>   sudo yum install opencv-devel.i686
  GFlags2.1.2   >>>>>>>>>   sudo yum install gflags.x86_64
  GLog0.3.4     >>>>>>>>>   sudo yum install glog.x86_64
  Cmake2.8.7+   >>>>>>>>>   sudo yum install cmake3.x86_64
	   
.. _寒武纪依赖库:

寒武纪安装包
^^^^^^^^^^^^^

CNStream的使用依赖于寒武纪Neuware安装包中CNRT库和CNCodec库。Neuware安装包是寒武纪公司发布的基于寒武纪硬件产品的神经网络开发工具包。用户需要在使用CNStream之前安装寒武纪Neuware安装包。发送邮件到 service@cambricon.com 联系寒武纪工程师获得Neuware安装包和安装指南。

Debian或Ubuntu环境配置
------------------------

1.  运行下面指令从github仓库检出CNStream源码。

    ::

      git clone https://github.com/Cambricon/CNStream.git

#.  安装寒武纪Neuware安装包。详情查看 :ref:`寒武纪依赖库` 。

#.  运行下面指令安装环境依赖。CNStream依赖OpenCV2.4.9+、GFlags2.1.2、GLog0.3.4和Cmake2.8.7+。详情查看 :ref:`环境依赖` 。
    
    ::

       sudo apt-get install libopencv-dev  libgflags-dev libgoogle-glog-dev cmake

#.  编译CNStream。CNStream使用CMake编译系统进行编译。 ``${CNSTREAM_DIR}`` 指的是CNStream源码目录。

    - 针对MLU100平台：

      ::

        mkdir -p build; cd build
        cmake ${CNSTREAM_DIR} -DMLU=MLU100
        make

    - 针对MLU270平台：

      ::

        mkdir -p build; cd build
        cmake ${CNSTREAM_DIR} -DMLU=MLU270
        make

#.  运行示例程序。

    ::
    
      cd ${CNSTREAM_DIR}/samples/demo
      ./run.sh

CentOS环境配置
------------------


1.  运行下面指令从github仓库检出CNStream源码。

    ::

      git clone https://github.com/Cambricon/CNStream.git


#.  安装寒武纪Neuware安装包。详情查看 :ref:`寒武纪依赖库` 。

#.  运行下面指令安装环境依赖。CNStream依赖OpenCV2.4.9+、GFlags2.1.2、GLog0.3.4和Cmake2.8.7+。详情查看 :ref:`环境依赖` 。
    
    ::

        sudo yum install opencv-devel.x86_64 gflags.x86_64 glog.x86_64 cmake3.x86_64。

#.  编译CNStream。CNStream使用CMake编译系统进行编译。 ``${CNSTREAM_DIR}`` 指的是CNStream源码目录。

    - 针对MLU100平台：

      ::

        mkdir -p build; cd build
        cmake ${CNSTREAM_DIR} -DMLU=MLU100
        make

    - 针对MLU270平台：

      ::

        mkdir -p build; cd build
        cmake ${CNSTREAM_DIR} -DMLU=MLU270
        make

#.  运行示例程序。

    ::
    
      cd ${CNSTREAM_DIR}/samples/demo
      ./run.sh

Docker环境配置
---------------

使用Docker镜像配置独立于宿主机的开发环境。

1.  安装Docker。宿主机需要预先安装Docker。详情请查看Docker官网主页：https://docs.docker.com/    

2.  制作Docker镜像。

    ::

      git clone https://github.com/Cambricon/CNStream.git

      cp ${neuware_package} CNStream   #copy your neuware package into CNStream 
      
      docker build -f Dockerfile --build-arg mlu_platform=${board_series} --build-arg neuware_package=${neuware_package_name} -t ubuntu_cnstream:v1 .

3.  运行示例程序。

    ::
      
      docker run -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY --device /dev/cambricon_c10Dev0 --net=host --pid=host -v $HOME/.Xauthority -it --privileged --name container_name  -v $PWD:/workspace ubuntu_cnstream:v1
      ./run.sh