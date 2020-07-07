.. _性能统计:

性能统计
=============

CNStream提供性能统计机制，帮助用户统计各模块及整条pipeline的性能，其中包括时延及吞吐量等。用户也可以自定义统计信息，详情查看 自定义性能统计_ 。

性能统计机制的实现主要在 **PerfManager** 类和 **PerfCalculator** 类中定义。 **PerfManager** 类主要实现了创建数据库文件、注册信息类型、记录信息等功能。类的声明在 ``framework/core/include/perf_manager.hpp`` 文件的源码中。 **PerfCalculator** 类主要用于性能计算。类的声明在 ``framework/core/include/perf_calculator.hpp`` 文件的源码中。

.. attention::
    |  统计性能依赖于pts，需要保证视频流中每帧的pts的唯一性，否则CNStream不能保证提供信息的准确性。


实现机制
----------

性能统计的实现机制如下图所示：

    .. figure::  ../images/performance_mech.png
       :align: center
       
       性能统计实现机制

每个数据流都需要创建一个PerfManager进行性能统计。初始化PerfManager后，数据库文件将被创建。每一帧数据将会被记录下来，并保存到创建的数据库中。基于数据库中的数据，计算相应各模块和pipeline的性能数据。

CNStream使用SQLite3数据库保存计算性能所需的数据。

用户在初始化PerfManager时，可以通过 ``Init`` 函数传入数据库文件名。随后，通过调用 ``RegisterPerfType`` 函数注册perf类型并在数据库中生成表。再通过调用 ``Record`` 函数，基于数据库中的表和字段，将相关数据记录到数据库中。最后CNStream读取该数据库的数据对模块和pipeline进行性能计算。

性能统计机制中，将为每一路视频创建一个PerfManager。

以下面pipeline为例，介绍PerfManager实现机制。

::

    ModuleA------ModuleB------ModuleC

通过该pipeline的数据流的stream id为 ``stream_0``。

记录信息
<<<<<<<<<<<

初始化PerfManager
*******************

调用 ``Init`` 函数传入数据库文件名（包括数据库文件的所在路径），示例如下：

::

  PerfManager perf_manager;

  // 初始化，将以stream id作为数据库文件名创建数据库文件
  perf_manager.Init("stream_0.db");

注册perf类型
****************

随后通过 ``RegisterPerfType`` 函数注册perf类型``PROCESS``，并生成表格。表格名为``PROCESS``，主索引为pts，其他索引为节点名字加 ``_stime`` ， ``_etime`` 和 ``_th`` 后缀，分别代表开始时间，结束时间和线程名称。

::

  // 生成每个模块对应的开始时间，结束时间以及线程名称的索引
  std::vector<std::string> keys = PerfManager::GetKeys({"ModuleA", "ModuleB", "ModuleC"}, {"_stime", "_etime", "_th"});

  // 注册perf类型
  perf_manager.RegisterPerfType("PROCESS", "pts", keys);

生成数据库表格如下：

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleA_th   ModuleB_stime   ModuleB_etime    ModuleB_th   ModuleC_stime   ModuleC_etime   ModuleC_th
   -------  -------------   -------------   ----------   -------------   -------------   ----------   -------------   -------------   ----------

记录相关数据
*****************

每帧数据在流过每个模块时相关数据都会分别被记录下来，并储存到数据库中。可以通过调用 ``Record`` 函数实现。有以下几种方式：

- 记录模块的开始和结束时间。
- 记录模块的其他信息。
- 记录模块的其他信息的时间戳。

**记录模块的开始和结束时间**

使用这种方法，用户需调用 ``Record`` 函数并依次传入参数：是否为结束时间、perf类型、模块名字以及pts。

例如：记录pts为100的一帧数据进入ModuleA模块的开始时间戳。

::

  //记录开始时间戳
  perf_manager.Record(false, "PROCESS", "ModuleA", 100);

在数据库中记录情况如下，其中xxxx代表当前时间的时间戳。

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleA_th   ModuleB_stime   ModuleB_etime    ModuleB_th   ModuleC_stime   ModuleC_etime   ModuleC_th
   -------  -------------   -------------   ----------   -------------   -------------   ----------   -------------   -------------   ----------
    100      xxxx

随后，记录pts为100的一帧数据ModuleA模块的结束时间戳。

::

  //记录结束时间戳
  perf_manager.Record(true, "PROCESS", "ModuleA", 100);

在数据库中记录情况如下：

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleA_th   ModuleB_stime   ModuleB_etime    ModuleB_th   ModuleC_stime   ModuleC_etime   ModuleC_th
   -------  -------------   -------------   ----------   -------------   -------------   ----------   -------------   -------------   ----------
    100      xxxx            xxxx

**记录模块的其他信息**

使用这种方法，用来记录其他信息，不仅仅是当前时间的时间戳。用户需调用 ``Record`` 函数并依次传入参数：perf类型、主索引、主索引值、索引、索引值。

例如：pts为100的一帧数据被ModuleA模块处理时的线程名称信息。

::

  // 记录线程名称信息
  perf_manager.Record("INFO", "pts", "100", "ModuleA_th"， "cnModuleA0");

在数据库中记录情况如下：

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleA_th   ModuleB_stime   ModuleB_etime    ModuleB_th   ModuleC_stime   ModuleC_etime   ModuleC_th
   -------  -------------   -------------   ----------   -------------   -------------   ----------   -------------   -------------   ----------
    100      xxxx            xxxx           cnModuleA0

**记录模块的其他信息的时间戳**

使用这种方法，用来记录其他信息的时间戳。用户需调用 ``Record`` 函数并依次传入参数：perf类型、主索引、主索引值、索引。

例如：某一帧的一个log信息的时间戳，记录perf类型是LOG，主索引为pts，其值100，索引为ModuleA_log。

::

  // 注册perf type LOG，主索引pts，其他索引ModuleA_log
  perf_manager.RegisterPerfType("LOG", "pts", {"ModuleA_log"});

  // 记录LOG信息
  perf_manager.Record("LOG", "pts", "100", "ModuleA_log");

在数据库中记录情况如下：

::

  TABLE PROCESS

    pts     ModuleA_stime   ModuleA_etime   ModuleA_th   ModuleB_stime   ModuleB_etime    ModuleB_th   ModuleC_stime   ModuleC_etime   ModuleC_th
   -------  -------------   -------------   ----------   -------------   -------------   ----------   -------------   -------------   ----------
    100      xxxx            xxxx           cnModuleA0

  TABLE LOG

    pts     ModuleA_log
   -------  -------------
    100      xxxx

计算模块和Pipeline的性能
<<<<<<<<<<<<<<<<<<<<<<<<<<<<

每个模块和pipeline对应的需要创建一个PerfCalculator来统计性能。使用PerfCalculator，每隔一段时间统计一次各模块及整条pipeline的性能。性能指标主要包括时延和吞吐量。

创建PerfCalculator
***********************

为模块创建PerfCalculator时，实例化 ``PerfCalculatorForModule`` 类，该类是 ``PerfCalculator`` 类的子类，提供统计模块性能的方法。

例如：为ModuleA创建PerfCalculator。

::

  PerfCalculatorForModule module_a_perf_calculator;

为pipeline创建PerfCalculator时，实例化 ``PerfCalculatorForPipeline`` 类，该类是 ``PerfCalculator`` 类的子类，提供统计pipeline性能的方法。

例如：为pipeline创建PerfCalculator。

::

  PerfCalculatorForPipeline pipeline_perf_calculator;

添加PerfCalculator需要的数据库

::

  // 创建PerfUtils类，添加stream_0对应的数据库
  std::shared_ptr<PerfUtils> perf_utils = std::make_shared<PerfUtils>();
  perf_utils->AddSql("stream_0", perf_manager.GetSql());

  // 设置PerfUtils给PerfCalculator
  module_a_perf_calculator.SetPerfUtils(perf_utils);
  pipeline_perf_calculator.SetPerfUtils(perf_utils);

模块的性能计算
****************

每帧的时延是模块处理该帧的时间，吞吐是单位时间内通过该插件的帧数。性能统计时，我们将计算所有帧的平均时延，最大时延，最小时延以及两次计算之间的吞吐和平均吞吐。

时延通过调用 ``CalcLatency`` 函数实现。例如：

::

  // 计算ModuleA模块处理数据流stream_0的时延
  PerfStats stats = module_a_perf_calculator.CalcLatency("stream_0", "PROCESS", {"ModuleA_stime", "ModuleA_etime"});

两次计算之间的吞吐，通过调用 ``CalcThroughput`` 函数实现。例如：

::

  // 计算ModuleA模块的吞吐
  PerfStats stats = module_a_perf_calculator.CalcThroughput("", "PROCESS", {"ModuleA_stime", "ModuleA_etime", "ModuleA_th"});

计算平均吞吐，通过调用 ``GetAvgThroughput`` 函数实现。例如：

::

  // 计算ModuleA模块的平均吞吐
  PerfStats stats = module_a_perf_calculator.GetAvgThroughput("", "PROCESS");

Pipeline的性能计算
********************

每帧的时延是该帧走完整个pipeline的时间，吞吐是单位时间内通过pipeline的帧数。如果pipeline有多个结束节点，则对于每个结束节点都有一组统计信息包括平均时延、最大时延，最小时延以及两次计算之间的吞吐和平均吞吐。

例如，pipeline的开始节点为ModuleA，结束节点ModuleC。

时延通过调用 ``CalcLatency`` 函数实现：

::

  // 计算数据流stream_0流过pipeline的时延
  PerfStats stats = pipeline_perf_calculator.CalcLatency("stream_0", "PROCESS", {"ModuleA_stime", "ModuleC_etime"});

两次计算之间的吞吐，通过调用 ``CalcThroughput`` 函数实现：

::

  // 计算pipeline处理数据流stream_0的吞吐
  PerfStats stats = pipeline_perf_calculator.CalcThroughput("stream_0", "PROCESS", {"ModuleC_etime"});

  // 计算pipeline的吞吐
  PerfStats stats = pipeline_perf_calculator.CalcThroughput("", "PROCESS", {"ModuleC_etime"});

计算平均吞吐，通过调用 ``GetAvgThroughput`` 函数实现。例如：

::

  // 计算pipeline处理数据流stream_0的平均吞吐
  PerfStats stats = pipeline_perf_calculator.GetAvgThroughput("stream_0", "PROCESS");

  // 计算pipeline的平均吞吐
  PerfStats stats = pipeline_perf_calculator.GetAvgThroughput("", "PROCESS");

统计信息打印
***************
::

  PerfStats stats;

  // 打印时延信息
  PrintLatency(stats);

  // 打印吞吐信息
  PrintThroughput(stats);

获得历史统计信息
******************

获得时延：

::

  // 获得ModuleA，stream_0的时延
  PerfStats stats = module_a_perf_calculator.GetLatency("stream_0", "PROCESS");

::

  // 获得pipeline，stream_0的时延
  PerfStats stats = pipeline_perf_calculator.GetLatency("stream_0", "PROCESS");

获得每次计算的吞吐：

::

  // 获得ModuleA的吞吐
  std::vector<PerfStats> stats_vec = module_a_perf_calculator.GetThroughput("", "PROCESS");

::

  // 获得pipeline，stream_0的吞吐
  std::vector<PerfStats> stats_vec = pipeline_perf_calculator.GetThroughput("stream_0", "PROCESS");

::

  // 获得pipeline的吞吐
  std::vector<PerfStats> stats_vec = pipeline_perf_calculator.GetThroughput("", "PROCESS");

获得平均吞吐：

::

  // 计算ModuleA模块的平均吞吐
  PerfStats stats = module_a_perf_calculator.GetAvgThroughput("", "PROCESS");

::

  // 计算pipeline处理数据流stream_0的平均吞吐
  PerfStats stats = pipeline_perf_calculator.GetAvgThroughput("stream_0", "PROCESS");

::

  // 计算pipeline的平均吞吐
  PerfStats stats = pipeline_perf_calculator.GetAvgThroughput("", "PROCESS");

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

用户需要按照 :ref:`programmingguide` 的步骤构建pipeline。在pipeline开始之前，需要调用 ``CreatePerfManager`` 函数创建PerfManager，并在函数中传入所有数据流的唯一标识 ``stream_id`` 和希望保存数据库文件的路径。

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
    if (!pipeline.CreatePerfManager(stream_ids, FLAGS_perf_db_dir)) {  // 传入stream_id和数据库文件储存路径。
      LOG(ERROR) << "Pipeline Create Perf Manager failed.";
      return EXIT_FAILURE;
    }
  }

.. attention::
    |  用户需要在pipeline开始之前，调用 ``CreatePerfManager`` 函数。

.. _自定义性能统计:

自定义性能统计
----------------

除了统计模块及整条pipeline的性能，用户也可以对其他方面的信息进行统计，如模块open的时间，log信息等。本节介绍了如何自定义记录信息以及自定义模块如何统计性能。

自定义记录信息
<<<<<<<<<<<<<<<<<<<<

如果想要对其他方面信息进行统计，用户可以初始化一个PerfManager，调用 ``RegisterPerfType`` 函数注册一个perf类型。随后可通过调用 ``Record`` 函数记录信息。

例如，注册TEST1类型和TEST2类型。

::

  PerfManager perf_manager;

  // 初始化PerfManager。
  perf_manager.Init("db_name.db");

  // 注册TEST1类型。
  perf_manager.RegisterPerfType("TEST1", {"LOGA_time, LOGA_msg"});

  // 注册TEST2类型。
  perf_manager.RegisterPerfType("TEST2", {"ModuleB_open_stime", "ModuleB_open_etime"});

  int64_t pts = 1;

  // LOG(INFO) << "This is a log message.";
  // 记录某一LOG的时间，以及LOG信息
  perf_manager.Record("TEST1", "pts", pts, "LOGA_time");
  perf_manager.Record("TEST1", "pts", pts, "LOGA_msg", "'This is a log message.'");

  // 记录ModuleB open函数的开始时间
  perf_manager.Record(false, "TEST2", "ModuleB_open", pts);
  // Open...
  // 记录ModuleB open函数的结束时间
  perf_manager.Record(true, "TEST2", "ModuleB_open", pts);

自定义模块设置
<<<<<<<<<<<<<<<<

如果希望统计自定义模块的性能，并且自定义模块的 ``Process`` 不在pipeline的 ``TaskLoop`` 函数中调用以及，不通过pipeline的 ``TransmitData`` 函数传递数据，则用户需要通过调用 ``GetPerfManager`` 函数获得PerfManager。

调用 pipeline的 ``CreatePerfManager`` 函数后，其他模块即可通过调用以下函数访问到各视频流的PerfManager。

::

  std::shared_ptr<PerfManager> GetPerfManager(const std::string &stream_id);

并在模块的Process开始处记录开始时间戳，处理完毕后记录结束时间戳。

例如，记录数据流stream_0中数据帧的pts为1的开始结束时间：

::

  std::shared_ptr<PerfManager> perf_manager_ptr = GetPerfManager("stream_0");

  int64_t pts = 1;
  // 记录UserModule process的开始时间
  perf_manager_ptr->Record(false, "PROCESS", "UserModule", pts);
  // Process...
  // 记录UserModule process的结束时间
  perf_manager_ptr->Record(false, "PROCESS", "UserModule", pts);

.. attention::
  |  一般来说，自定义的source模块需要在模块内部记录处理每一帧数据的开始时间。
