.. _性能统计:

性能统计
=============

介绍
-------

CNStream提供性能统计机制，帮助用户统计各模块及整条pipeline的性能，包括时延及吞吐量等。用户也可以自定义想要统计的信息，例如所有模块open的时间等，详情查看 注册信息类型_ 。

每一帧数据将会被打上一系列的时间戳，即数据进入每个模块的开始时间和结束时间。再根据该时间戳序列计算相应各模块和pipeline的性能数据。CNStream使用SQLite3数据库保存时间戳序列。

性能统计机制的实现主要在 ``PerfManager`` 类中定义。``PerfManager`` 类实现了自身初始化、记录信息、注册信息类型、计算模块性能和计算pipeline性能等功能。类的声明在 ``modules/core/include/perf_manager.hpp`` 文件的源码中。初始化时会创建数据库和实例化 ``PerfCalculator`` 类。 ``PerfCalculator`` 类主要用于性能计算。

.. attention::
    |  统计性能依赖于pts，需要保证视频流中每帧的pts的唯一性,否则CNStream不能保证提供信息的准确性。


实现机制
----------

每个数据流都需要创建一个 ``PerfManager`` 进行性能统计。``PerfManager`` 的实现机制如下图所示：

    .. figure::  ../images/performance_mech.png

       性能统计实现机制


初始化PerfManager
<<<<<<<<<<<<<<<<<<<

初始化 ``PerfManager`` 时，需要传入pipeline中所有节点（模块）、开始节点和所有结束节点的名字。

``PerfInfo`` 结构体记录了当前时间戳的信息，如类型、模块名称、数据帧的pts等。``PerfManager`` 默认使用 ``PROCESS`` 类型（PerfType）对模块及pipeline的性能做统计。在初始化时将以 ``PROCESS`` 作为表格名生成数据库表格。用户也可以自定义想要统计的其他方面的信息，详情查看 注册信息类型_ 。

``PerfInfo`` 结构体如下：

::

  struct PerfInfo {
    bool is_finished;           // 模块开始或结束。
    std::string perf_type;      // 类型。
    std::string module_name;    // 模块名字。
    int64_t pts;                // 数据库表格的主索引pts。
    size_t timestamp;           // 时间戳，记录此信息时刻的时间。
  };  // struct PerfInfo

数据库表格的主索引为pts, 其他索引为节点名字加 ``_stime`` 和 ``_etime`` 后缀，分别代表开始和结束时间。开始节点索引始终紧随主索引pts。

以下面pipeline为例：

::

    ModuleA------ModuleB------ModuleC

``PerfManager`` 初始化调用函数如下：

::

  PerfManager perf_manager;

  // 初始化数据库名字、所有节点名字、开始节点、所有结束节点。
  perf_manager.Init("db_name.db", {"ModuleA", "ModuleB", "ModuleC"}, ModuleA, {ModuleC});

  PerfInfo info {false, "PROCESS", node_name, pts};  // 模块开始，类型，节点名字，pts。

生成表格如下：

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleB_stime   ModuleB_etime    ModuleC_stime    ModuleC_etime
   -------  -------------   -------------   -------------   -------------    -------------    -------------

记录时间戳
<<<<<<<<<<<

每帧数据在流过每个模块的开始和结束时都会分别生成时间戳，并储存到数据库中。用户可以通过调用 ``RecordPerfInfo`` 函数实现。例如：

::

  perf_manager.RecordPerfInfo(info); 

记录一段时间后，数据库将被填入时间戳信息，例如：

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleB_stime   ModuleB_etime    ModuleC_stime    ModuleC_etime
   -------  -------------   -------------   -------------   -------------    -------------    -------------
    0        xxxx            xxxx            xxxx            xxxx             xxxx             xxxx
    1        xxxx            xxxx            xxxx            xxxx             xxxx             xxxx
    ...

性能计算
<<<<<<<<<

每隔一段时间各模块及整条pipeline的性能就会被统计一次。性能指标主要包括时延和吞吐量。

模块的性能计算
****************

每帧的时延是模块处理该帧的时间。性能统计时，我们将计算所有帧的平均时延，最大时延以及吞吐量。吞吐量是平均时延的倒数。通过调用 ``CalculatePerfStats`` 函数实现。例如：

::

    PerfStats statsA = perf_manager.CalculatePerfStats("PROCESS", ModuleA);
    PerfStats statsB = perf_manager.CalculatePerfStats("PROCESS", ModuleB);
    PerfStats statsC = perf_manager.CalculatePerfStats("PROCESS", ModuleC);

如需打印模块性能信息，调用 ``PrintPerfStats`` 函数。详情参见 ``modules/core/include/perf_calculator.hpp`` 文件。

::

    PrintPerfStats(statsA);
    PrintPerfStats(statsB);
    PrintPerfStats(statsC);

Pipeline的性能计算
********************

每帧的时延是该帧走完整个pipeline的时间。如果pipeline有多个结束节点，则对于每个结束节点都有一组统计信息包括平均时延、最大时延和吞吐量。

吞吐量计算公式如下：

::

  throughput = frame count / (结束节点时间戳最大值 - 开始节点时间戳最小值)

用户可以通过调用以下函数实现：

::

    std::vector<PerfStats> stats = perf_manager.CalculatePipelinePerfStats("PROCESS");

如需打印模块性能信息，调用 ``PrintPerfStats`` 函数，详情见 ``modules/core/include/perf_calculator.hpp`` 文件。

::

    for (auto it : stats) {
      PrintPerfStats(it);
    }

除此之外，如果只打印时延或吞吐量信息，用户可以 ``PrintLatency`` 或 ``PrintThroughput`` 函数。

开发样例介绍
---------------

用户可以直接使用CNStream提供的开发样例，无需修改任何设置，即可快速体验模块和pipleline的性能统计功能。

示例脚本说明
<<<<<<<<<<<<<<<<<<

CNStream提供的run.sh示例脚本位于 ``${CNSTREAM_PATH}/samples/demo`` 目录下，其中 ``${CNSTREAM_DIR}`` 是指CNStream源码目录。数据库文件默认保存到 ``perf_database`` 文件夹下。如果希望更改生成的数据库文件的储存路径，设置参数 ``perf_db_dir`` 即可。此外，CNStream提供的示例默认开启性能统计功能。如需关闭，可在脚本中设置 ``perf`` 参数为 **false**。

::

  ./../bin/demo  \

      ...

      --config_fname "detection_config.json" \

      ...

      --perf=false   \           #关闭性能统计功能，默认开启。
      --perf_db_dir="db_dir"     #设置数据库文件保存路径到执行目录下的db_dir文件夹下，默认保存到perf_database文件夹下。

配置文件说明
<<<<<<<<<<<<<<<<<<<<

示例脚本run.sh对应的JSON配置文件 ``detection_config.json`` 位于 ``${CNSTREAM_PATH}/samples/demo`` 目录下，其中 ``${CNSTREAM_DIR}`` 是指CNStream源码目录。模块参数 ``show_perf_info`` 表示是否显示模块性能。设为 **true** 时将显示该模块的性能，设为 **false** 时则不显示该模块的性能。

例如显示source模块的性能数据，JSON配置文件配置如下：

::

  {
    "source" : {
      // 数据源模块。设置使用ffmpeg进行demux，使用MUL解码，不单独启动线程。
      "class_name" : "cnstream::DataSource",

      ...

      "show_perf_info" : true,   //显示数据源模块的性能。
      "custom_params" : {
        ...
      }
    },

    ...
  }

.. _自定义构建pipeline:

对自定义构建pipeline的性能统计
------------------------------

用户需要按照 :ref:`programmingguide` 的步骤构建pipeline。但在动态增加数据源之前，需要调用 ``CreatePerfManager`` 函数创建 ``PerfManager``，并在函数中传入所有数据流的唯一标识 ``stream_id`` 和希望保存数据库文件的路径。

创建 ``PerfManager`` 源代码示例如下，详情可参考 ``samples/demo/demo.cpp`` 文件的CNStream源码。

::

   /*
      创建perf recorder。
   */
   if (FLAGS_perf) {
     std::vector<std::string> stream_ids;
     for (int i = 0; i < static_cast<int>(video_urls.size()); i++) {
       stream_ids.push_back(std::to_string(i));
     }
     // 创建PerfManager。
     pipeline.CreatePerfManager(stream_ids, FLAGS_perf_db_dir);  // 传入stream_id和数据库文件储存路径。
   }

自定义性能统计
----------------

除了统计模块及整条pipeline的性能，用户也可以对其他方面的信息进行统计，如所有模块open的时间等。本节介绍了如何自定义性能统计的信息以及自定义模块如何统计性能。

.. _注册信息类型:

自定义性能统计信息
<<<<<<<<<<<<<<<<<<<<

如果想要对其他方面信息进行统计，用户需要调用 ``RegisterPerf()`` 函数注册一个信息类型，即PerfType。在调用 ``RecordPerfInfo`` 函数记录 ``PerfInfo`` 时，需将定义的信息类型传入到函数中。用户可以在 ``PerfManager`` 初始化之前注册信息类型，所有已经注册的PerfType的表格在初始化时统一生成。如果 ``PerfManager`` 已经初始化，则在注册时生成对应的表格。

.. attention::
    |  如果需要在模块的 ``Open`` 函数中注册信息类型，注意搭建pipeline时一定要在 ``pipeline.Start()`` 函数前创建 ``PerfManager``。详情参考 自定义构建pipeline_ 。
	
例如，注册TEST1类型和TEST2类型。

::

  PerfManager perf_manager;

  // 注册TEST1类型。
  perf_manager.RegisterPerf("TEST1");

  // 初始化PerfManager。
  perf_manager.Init("db_nam.db", {"ModuleA", "ModuleB", "ModuleC"}, ModuleA, {ModuleC});

  // 注册TEST2类型。
  perf_manager.RegisterPerf("TEST2");

  PerfInfo info1 {false, "TEST1", node_name, pts};
  perf_manager.RecordPerfInfo(info1);

  PerfInfo info2 {false, "TEST2", node_name, pts};
  perf_manager.RecordPerfInfo(info2);


自定义计时
<<<<<<<<<<<<<<<<<<<

用户需要在模块基类中声明如下变量来实现自定义计时。调用 ``CreatePerfManager`` 函数后，其他模块即可访问到各视频流的 ``PerfManager``。

::

  // 每个视频流的PerfManager。key为stream_id。
  std::unordered_map<std::string, std::shared_ptr<PerfManager>> perf_managers_;


自定义模块设置
<<<<<<<<<<<<<<<<

如果不在pipeline中调用自定义模块的 ``Process`` 和 ``TransmitData`` 函数，则用户需要在模块的Process开始处记录开始时间戳，处理完毕后记录结束时间戳。
