.. _install:

环境配置
================

本章介绍了如何在Debian、Ubuntu、CentOS以及Docker环境下配置CNStream。

依赖库
-------

.. _环境依赖:

环境依赖
^^^^^^^^^^^^^

CNStream有以下环境依赖。

- OpenCV2.4.9+
- GFlags2.1.2
- GLog0.3.4
- Cmake2.8.7+
- Live555
- SDL22.0.4+
- SQLite3

.. _寒武纪依赖库:

寒武纪安装包
^^^^^^^^^^^^^

CNStream的使用依赖于寒武纪Neuware安装包中CNRT库和CNCodec库。Neuware安装包是寒武纪公司发布的基于寒武纪硬件产品的神经网络开发工具包。用户需要在使用CNStream之前安装寒武纪Neuware安装包。发送邮件到 service@cambricon.com 联系寒武纪工程师获得Neuware安装包和安装指南。

Debian或Ubuntu环境配置
------------------------

1.  运行下面指令从github仓库检出CNStream源码。``${CNSTREAM_DIR}`` 代表CNStream源码目录。

    ::

        git clone https://github.com/Cambricon/CNStream.git

#.  安装寒武纪Neuware安装包。详情查看 :ref:`寒武纪依赖库` 。

#.  运行下面指令安装环境依赖。CNStream依赖的环境详情，查看 :ref:`环境依赖`。

    用户可通过 ``${CNSTREAM_DIR}/tools`` 下的 ``pre_required_helper.sh`` 脚本进行安装：

    ::

        cd ${CNSTREAM_DIR}/tools
        ./pre_required_helper.sh

    或者通过以下命令进行安装：

    ::

        sudo apt-get install libopencv-dev  libgflags-dev libgoogle-glog-dev cmake
        sudo apt-get install libfreetype6 ttf-wqy-zenhei libsdl2-dev lcov libsqlite3-dev sqlite3
        cd ${CNSTREAM_DIR}/tools
        ./download_live.sh

#.  编译CNStream。CNStream使用CMake编译系统进行编译。

    - 针对MLU270平台：

      ::

            mkdir -p build; cd build
            cmake ${CNSTREAM_DIR} -DMLU=MLU270
            make

    - 针对MLU220 SOC平台：

      ::

            mkdir -p build; cd build
            cmake ${CNSTREAM_DIR} -DMLU=MLU220_SOC
            make

#.  运行示例程序。

    ::
    
        cd ${CNSTREAM_DIR}/samples/demo
        ./run.sh

CentOS环境配置
------------------


1.  运行下面指令从github仓库检出CNStream源码。``${CNSTREAM_DIR}`` 代表CNStream源码目录。

    ::

        git clone https://github.com/Cambricon/CNStream.git


#.  安装寒武纪Neuware安装包。详情查看 :ref:`寒武纪依赖库`。

#.  运行下面指令安装环境依赖。CNStream依赖的环境详情，查看 :ref:`环境依赖`。

    用户可通过 ``${CNSTREAM_DIR}/tools`` 下的 ``pre_required_helper.sh`` 脚本进行安装：

    ::

      cd ${CNSTREAM_DIR}/tools
      ./pre_required_helper.sh


    或者通过以下命令进行安装：

    ::

      sudo yum install opencv-devel.x86_64 gflags.x86_64 glog.x86_64 cmake3.x86_64
      sudo yum install freetype-devel SDL2_gfx-devel.x86_64 wqy-zenhei-fonts lcov sqlite-devel
      sudo yum install ffmpeg ffmpeg-devel
      cd ${CNSTREAM_DIR}/tools
      ./download_live.sh

#.  编译CNStream。CNStream使用CMake编译系统进行编译。 ``${CNSTREAM_DIR}`` 代表CNStream源码目录。

    - 针对MLU270平台：

      ::

            mkdir -p build; cd build
            cmake ${CNSTREAM_DIR} -DMLU=MLU270
            make


    - 针对MLU220 SOC平台：

      ::

            mkdir -p build; cd build
            cmake ${CNSTREAM_DIR} -DMLU=MLU220_SOC
            make

#.  运行示例程序。

    ::
    
        cd ${CNSTREAM_DIR}/samples/demo
        ./run.sh

Docker环境配置
---------------

使用Docker镜像配置独立于宿主机的开发环境。

1.  安装Docker。宿主机需要预先安装Docker。详情请查看Docker官网主页：https://docs.docker.com/    

2.  运行下面命令制作Docker镜像。

    其中``${neuware_package}`` 为寒武纪Neuware安装包及其存放路径。``${board_series}`` 为用户使用板卡的型号，即MLU270或MLU220SOC。

    ::

        git clone https://github.com/Cambricon/CNStream.git
        cp ${neuware_package} CNStream   #copy your neuware package into CNStream
        docker build -f Dockerfile --build-arg mlu_platform=${board_series} --build-arg neuware_package=${neuware_package_name} -t ubuntu_cnstream:v1 .


    CNStream提供以下Dockerfile：

    ::

         docker/Dockerfiler.16.04
         docker/Dockerfiler.18.04
         docker/Dockerfiler.CentOS

3.  运行示例程序。

    ::

        docker run -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY --privileged -v /dev:/dev --net=host --ipc=host --pid=host -v $HOME/.Xauthority -it --name container_name  -v $PWD:/workspace ubuntu_cnstream:v1
        ./run.sh
