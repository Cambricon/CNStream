.. _性能统计:

性能统计
=============

CNStream提供性能统计机制，帮助用户统计各模块及整条pipeline的性能，其中包括时延及吞吐量等。用户也可以自定义想要统计的信息，详情查看 注册信息类型_ 。

性能统计机制的实现主要在 **PerfManager** 类中定义。**PerfManager** 类实现了自身初始化、记录信息、注册信息类型、计算模块性能和计算pipeline性能等功能。类的声明在 ``modules/core/include/perf_manager.hpp`` 文件的源码中。此外，初始化时会创建数据库和实例化 **PerfCalculator** 类。 **PerfCalculator** 类主要用于性能计算。

.. attention::
    |  统计性能依赖于pts，需要保证视频流中每帧的pts的唯一性，否则CNStream不能保证提供信息的准确性。


实现机制
----------

性能统计的实现机制如下图所示：

    .. figure::  ../images/performance_mech.png

       性能统计实现机制

每个数据流都需要创建一个PerfManager进行性能统计。初始化PerfManager后，数据库文件将被创建。每一帧数据将会被记录下来，并保存到创建的数据库中。基于数据库中的数据，计算相应各模块和pipeline的性能数据。

数据库文件
<<<<<<<<<<<<

CNStream使用SQLite3数据库保存用户想要计算的性能数据。

用户在初始化PerfManager时，可以通过 ``Init`` 函数传入数据库文件名以及数据库中的字段名称。再通过调用 ``Record`` 函数，基于数据库中的字段，将相关数据记录到数据库中。最后CNStream调用该数据库的数据对模块和pipeline进行性能计算。

初始化PerfManager
<<<<<<<<<<<<<<<<<<<

根据perf类型的不同，初始化PerfManager的方法也会有所不同。主要分为：

- 使用CNStream预定义的perf类型 ``PROCESS`` 做性能统计。
- 使用自定义的perf类型做性能统计。

**使用CNStream预定义的perf类型**

寒武纪CNStream提供 ``PROCESS`` perf类型对模块及pipeline的性能做统计。用户需要调用 ``Init`` 函数传入数据库文件名（包括数据库文件的所在路径）、pipeline中所有节点（模块）、开始节点和所有结束节点的名字，初始化PerfManager。

初始化PerfManager后，CNStream将以 ``PROCESS`` 作为表格名生成数据库表格。使用CNStream预定义的 ``PROCESS`` 类型生成的数据库表格，主索引为pts，其他索引为节点名字加 ``_stime`` 和 ``_etime`` 后缀，分别代表开始和结束时间，开始节点索引始终紧随主索引pts。

用户也可以自定义想要统计的其他方面的信息，详情查看 注册信息类型_ 。

以下面pipeline为例：

::

    ModuleA------ModuleB------ModuleC

PerfManager初始化调用 ``Init`` 函数，示例如下：

::

  PerfManager perf_manager;

  // 初始化数据库名字、所有节点名字、开始节点、所有结束节点。
  perf_manager.Init("db_name.db", {"ModuleA", "ModuleB", "ModuleC"}, ModuleA, {ModuleC});

生成数据库表格如下：

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleB_stime   ModuleB_etime    ModuleC_stime    ModuleC_etime
   -------  -------------   -------------   -------------   -------------    -------------    -------------

**使用自定义的perf类型**

如果想要使用自定义的perf类型，需要创建数据库文件，并连接数据库。使用此方法，用户需要调用 ``Init`` 函数传入数据库文件名（包括数据库文件的所在路径），并调用 ``RegisterPerfType`` 函数自定义per类型。

调用 ``Init`` 函数初始化PerfManager，示例如下：

::

  PerfManager perf_manager;

  // 初始化数据库名字
  perf_manager.Init("db_name.db");

随后可以通过 ``RegisterPerfType`` 函数创建perf类型，并生成表格。以记录模块ModuleA的开始和结束时间为例，依次传入perf类型、主索引和其他索引（即ModuleA的开始时间和结束时间）。

::

  perf_manager.RegisterPerfType("TEST", "pts", {"ModuleA_stime", "ModuleA_etime"})

生成表格如下：

::

  TABLE TEST

    pts     ModuleA_stime   ModuleA_etime
   -------  -------------   -------------

记录相关数据
<<<<<<<<<<<<<<

每帧数据在流过每个模块时相关数据都会分别被记录下来，并储存到数据库中。用户可以通过调用 ``Record`` 函数实现。根据用户需要，有以下几种方式：

- 仅记录模块的开始和结束时间。
- 记录模块的其他信息的时间戳。
- 记录模块除时间戳外的其他信息。

**仅记录模块的开始和结束时间**

使用这种方法，用户需调用 ``Record`` 函数来依次传入参数：是否为结束帧、perf类型、模块名字以及pts。

例如：记录pts为100的一帧数据进入ModuleA模块的开始时间戳，perf类型是 ``PROCESS``。

::

  // 初始化数据库名字、所有节点名字、开始节点、所有结束节点。
  PerfManager perf_manager;
  perf_manager.Init("db_name.db", {"ModuleA", "ModuleB", "ModuleC"}, ModuleA, {ModuleC});

  //记录新信息。
  perf_manager.Record(false, "PROCESS", "ModuleA", 100);

在数据库中记录情况如下，其中xxxx代表当前时间的时间戳。

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleB_stime   ModuleB_etime    ModuleC_stime    ModuleC_etime
   -------  -------------   -------------   -------------   -------------    -------------    -------------
    100      xxxx

随后，记录pts为100的一帧数据ModuleA模块的结束时间戳，perf类型是 ``PROCESS``。

::

  perf_manager.Record(true, "PROCESS", "ModuleA", 100);

在数据库中记录情况如下：

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleB_stime   ModuleB_etime    ModuleC_stime    ModuleC_etime
   -------  -------------   -------------   -------------   -------------    -------------    -------------
    100      xxxx            xxxx

**记录模块的其他信息的时间戳**

使用这种方法，用来记录其他信息的时间戳。用户需调用 ``Record`` 函数来依次传入参数：perf类型、主索引、主索引值、索引。

例如：某一帧的一个log信息的时间戳，记录perf类型是LOG，主索引为pts，其值100，索引为ModuleA_log。

::

  // 初始化，注册perf type LOG，主索引pts，其他索引ModuleA_log
  PerfManager perf_manager;
  perf_manager.Init("db_name.db");
  perf_manager.RegisterPerfType("LOG", "pts", {"ModuleA_log"});

  // 记录信息
  perf_manager.Record("LOG", "pts", "100", "ModuleA_log");

在数据库中记录情况如下：

::

  TABLE LOG

    pts     ModuleA_log
   -------  -------------
    100      xxxx

**记录模块除时间戳外的其他信息**

使用这种方法，用来记录其他信息，不仅仅是当前时间的时间戳。用户需调用 ``Record`` 函数来依次传入参数：perf类型、主索引、主索引值、索引、索引值。

例如：某一帧的frame id信息。记录perf类型是INFO，主索引为pts，其值1000，索引为frame_id，其值为300。

::

  // 初始化，注册perf type INFO，主索引pts，其他索引frame_id
  PerfManager perf_manager;
  perf_manager.Init("db_name.db");
  perf_manager.RegisterPerfType("INFO", "pts", {"frame_id"});

  // 记录信息
  perf_manager.Record("INFO", "pts", "1000", "frame_id"， "300");

在数据库中记录情况如下：

::

  TABLE INFO

    pts      frame_id
   -------  ------------
    1000      300

计算模块和Pipeline的性能
<<<<<<<<<<<<<<<<<<<<<<<<<<<<

每隔一段时间各模块及整条pipeline的性能就会被统计一次。性能指标主要包括时延和吞吐量。

模块的性能计算
****************

每帧的时延是模块处理该帧的时间。性能统计时，我们将计算所有帧的平均时延，最大时延以及吞吐量。吞吐量是平均时延的倒数。通过调用 ``CalculatePerfStats`` 函数实现。例如：

::

    PerfStats statsA = perf_manager.CalculatePerfStats("PROCESS", ModuleA);
    PerfStats statsB = perf_manager.CalculatePerfStats("PROCESS", ModuleB);
    PerfStats statsC = perf_manager.CalculatePerfStats("PROCESS", ModuleC);

如需打印模块性能信息，可以调用 **PerfCalculator** 类的 ``PrintPerfStats`` 函数实现。详情参见 ``modules/core/include/perf_calculator.hpp`` 文件。

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

如需打印模块性能信息，可以调用 **PerfCalculator** 类的 ``PrintPerfStats`` 函数实现，详情见 ``modules/core/include/perf_calculator.hpp`` 文件。

::

    for (auto it : stats) {
      PrintPerfStats(it);
    }

除此之外，如果只打印时延或吞吐量信息，用户可以调用 **PerfCalculator** 类的 ``PrintLatency`` 或 ``PrintThroughput`` 函数来实现。

开发样例介绍
---------------

用户可以直接使用CNStream提供的开发样例，无需修改任何设置，即可快速体验模块和pipleline的性能统计功能。

示例脚本说明
<<<<<<<<<<<<<<<<<<

用户通过运行 ``run.sh`` 示例脚本来运行示例。示例位于 ``${CNSTREAM_PATH}/samples/demo`` 目录下，其中 ``${CNSTREAM_DIR}`` 是指CNStream源码目录。

数据库文件默认保存到 ``perf_database`` 文件夹下。如果希望更改生成的数据库文件的储存路径，只需设置示例脚本中的参数 ``perf_db_dir`` 即可。此外，CNStream提供的示例默认开启性能统计功能。如需关闭，可在脚本中设置 ``perf`` 参数为 **false**。

::

  ./../bin/demo  \

      ...

      --config_fname "detection_config.json" \

      ...

      --perf=false   \           #关闭性能统计功能，默认开启。
      --perf_db_dir="db_dir"     #设置数据库文件保存路径到执行目录下的db_dir文件夹下，默认保存到perf_database文件夹下。

配置文件说明
<<<<<<<<<<<<<<<<<<<<

示例脚本 ``run.sh`` 对应的JSON配置文件 ``detection_config.json`` 位于 ``${CNSTREAM_PATH}/samples/demo`` 目录下，其中 ``${CNSTREAM_DIR}`` 是指CNStream源码目录。模块参数 ``show_perf_info`` 表示是否显示模块性能。设为 **true** 时将显示该模块的性能，设为 **false** 时则不显示该模块的性能。

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

用户需要按照 :ref:`programmingguide` 的步骤构建pipeline。但在动态增加数据源之前，需要调用 ``CreatePerfManager`` 函数创建PerfManager，并在函数中传入所有数据流的唯一标识 ``stream_id`` 和希望保存数据库文件的路径。

创建PerfManager源代码示例如下，详情可参考 ``samples/demo/demo.cpp`` 文件的CNStream源码。

::

   /*
      创建perf manager。
   */
   if (FLAGS_perf) {
     std::vector<std::string> stream_ids;
     for (int i = 0; i < static_cast<int>(video_urls.size()); i++) {
       stream_ids.push_back(std::to_string(i));
     }
     // 创建PerfManager。
     pipeline.CreatePerfManager(stream_ids, FLAGS_perf_db_dir);  // 传入stream_id和数据库文件储存路径。
   }

.. attention::
    |  用户需要在pipeline开始之前，调用 ``CreatePerfManager`` 函数。

自定义性能统计
----------------

除了统计模块及整条pipeline的性能，用户也可以对其他方面的信息进行统计，如所有模块open的时间等。本节介绍了如何自定义性能统计的信息以及自定义模块如何统计性能。

.. _注册信息类型:

自定义性能统计信息
<<<<<<<<<<<<<<<<<<<<

如果想要对其他方面信息进行统计，用户需要调用 ``RegisterPerfType`` 函数注册一个perf类型。随后可通过调用 ``Record`` 函数记录信息。

例如，注册TEST1类型和TEST2类型。

::

  PerfManager perf_manager;

  // 初始化PerfManager。
  perf_manager.Init("db_nam.db", {"ModuleA", "ModuleB", "ModuleC"}, ModuleA, {ModuleC});

  // 注册TEST1类型。
  perf_manager.RegisterPerfType("TEST1");

  // 注册TEST2类型。
  perf_manager.RegisterPerfType("TEST2");

  int64_t pts = 1;
  perf_manager.Record(false, "TEST1", "ModuleA", pts);
  perf_manager.Record(false, "TEST2", "ModuleB", pts);


自定义计时
<<<<<<<<<<<<<<<<<<<

用户需要在模块基类中声明如下变量来实现自定义计时。调用 ``CreatePerfManager`` 函数后，其他模块即可访问到各视频流的PerfManager。

::

  // 每个视频流的PerfManager, key为stream_id。
  std::unordered_map<std::string, std::shared_ptr<PerfManager>> perf_managers_;


自定义模块设置
<<<<<<<<<<<<<<<<

如果不在pipeline中调用自定义模块的 ``Process`` 和 ``TransmitData`` 函数，则用户需要在模块的Process开始处记录开始时间戳，处理完毕后记录结束时间戳。
