# web可视化工具 #

## 编译使用方式 ##

``${CNSTREAM_DIR}`` 代表CNStream源码路径.

1. 编译CNStream

    Web可视化依赖于PythonAPI，因此需要打开 ``build_python_api`` 编译选项开关。（Python API依赖于python3+）

    ```sh
    cd ${CNSTREAM_DIR}/
    mkdir build;  cd build
    cmake .. -Dbuild_python_api=ON
    # For MLU370,
    # cmake .. -Dbuild_python_api=ON -DCNIS_USE_MAGICMIND=ON
    make -j8
    ```

2. 安装依赖的python包

    Web可视化的依赖有：Flask，gevent，gunicorn，numpy以及opencv。

    ```sh
    cd ${CNSTREAM_DIR}/tools/web_visualize
    pip install -r requirements.txt
    ```

3. 运行webserver

    ```sh
    cd ${CNSTREAM_DIR}/tools/web_visualize
    ./run_web_visualize.sh
    ```

4. 访问网页

    直接浏览器中输入网页地址即可。网页地址为 ``服务器ip:端口号`` ，例如 ``192.168.81.41:9099`` 。在webserver运行起来后， ``Listening at:`` 字段会给出网页地址。启动webserver的服务器和访问网页的主机需要在相同网段。

    端口号可在 [webserver_config.py](./webserver_config.py) 文件中进行修改。


## 功能简介 ##

### web可视化主页功能一览 ###

<img src="./src/webui/static/data/viz_home.png" width="60%" height="60%">

**web可视化工具主要提供的功能：**

- 支持选用或设计pipeline配置：
  - 提供一些内置的pipeline demo配置
  - 支持在线design pipeline配置（web主页面点击右上角的“Pipeline Design”进入）
- 支持部分数据源选用和上传
  - home页面上依次给出cars/people/images/objects四种类型的数据源
  - “Choose a file”按钮支持上传视频文件
- 选择好数据和pipeline配置后，支持运行pipeline,通过RUN和STOP按钮控制开始运行和结束：
  - “PREVIEW”tab页下执行RUN/STOP操作，运行pipeline并在网页端渲染pipeline输出的视频（针对自行设计的pipeline配置，需要pipeline的end节点为汇聚节点）

### pipeline design页面功能一览 ###

<img src="./src/webui/static/data/viz_design.png" width="60%" height="60%">

**web端design pipeline主要提供以下功能：**

- 提供内置插件的流程块，支持像绘制流程图一样在web端绘制pipeline，点击选择表示插件的流程块（在页面上可拖动），并通过连线连接数据流向；
- 插件参数可以修改配置：选中插件，在右侧边栏的中修改并点击submit保存；
- 提供pipeline配置正确性检测：包括基本的插件参数配置和流程图的环检测（自动执行）；
- 流程图绘制完成后，可以通过download下载为json文件或者点击Done跳转至主页面运行或预览。
