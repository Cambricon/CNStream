.. _性能统计:

性能统计
====================

CNStream提供性能统计机制帮助用户分析程序的性能，主要包括pipeline各部分的时延和吞吐信息。

除此之外，CNStream还提供自定义时间统计机制，帮助用户在自定义模块中，统计模块内部性能数据，并最后与整体性能数据汇总，帮助分析pipeline性能瓶颈。

性能统计机制的实现和定义在 CNStream源代码目录下 ``framework/core/include/profiler`` 和 ``framework/core/src/profiler`` 目录中。

机制原理
-------------

性能统计通过在pipeline中各个处理过程的开始和结束点上打桩，记录打桩时间点，并基于打桩时间点来计算时延、吞吐等信息。

性能统计机制以某个处理过程为对象进行性能统计。一个处理过程可以是一个函数调用、一段代码，或是pipeline中两个处理节点之间的过程。在CNStream的性能统计机制中，每个处理过程通过字符串进行映射。

对于每个pipeline实例，通过创建一个 ``PipelineProfiler`` 实例来管理该pipeline中性能统计的运作。通过 ``PipelineProfiler`` 实例为pipeline中的每个模块创建一个 ``ModuleProfiler`` 实例来管理模块中性能统计。并通过 ``ModuleProfiler`` 的 ``RegisterProcessName`` 接口注册需要进行性能统计的处理过程，使用 ``ModuleProfiler::RecordProcessStart`` 和 ``ModuleProfiler::RecordProcessEnd`` 在各处理过程的开始和结束时间点打桩，供CNStream进行性能统计。

.. _开启性能统计功能:

开启性能统计功能
-----------------------

性能统计功能默认是关闭状态。要打开性能统计功能，有以下两种方式：

- 通过 ``Pipeline::BuildPipeline`` 接口构建pipeline，通过设置 ``enable_profiling`` 参数为 **true** 打开性能统计功能。示例代码如下：

  ::
  
    cnstream::Pipeline pipeline;
    cnstream::ProfilerConfig profiler_config;
    profiler_config.enable_profiling = true;
    pipeline.BuildPipeline(module_configs, profiler_config);

- 通过配置文件的方式构建pipeline，即使用 ``Pipeline::BuildPipelineByJSONFile`` 接口构建pipeline。在json格式的配置文件中设置 ``enable_profiling`` 参数为 **true** ，打开性能统计功能。

  示例配置文件如下。``profiler_config`` 项中 ``enable_profiling`` 子项被置为true，表示打开pipeline的性能统计功能。 ``module1`` 和 ``module2`` 为两个模块的配置。pipeline配置文件中 ``profiler_config`` 项和模块配置项之间没有顺序要求。
  
  ::
  
    {
      "profiler_config" : {
        "enable_profiling" : true
      },
    
      "module1" : {
        ...
      },
    
      "module2" : {
        ...
      }
    }
  



内置处理过程
-------------------

由于模块输入队列和模块Process函数的性能统计通常至关重要，CNStream对pipeline中每个模块的输入队列和模块Process函数提供性能统计。CNStream内部通过 ``PipelineProfiler`` 将这个两个处理过程注册为两个默认的处理过程。用户只需 开启性能统计功能_，即可查看模块输入队列的性能数据和Process函数的性能数据。

获取模块输入队列性能数据
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

对于模块的输入队列，下面两个时间点会被打桩：

- 数据入队的时间点。
- 数据出队的时间点。

通过记录数据进出队列的时间节点，来统计模块输入队列的性能数据。

数据通过模块输入数据队列的过程作为一个处理过程，被CNStream默认注册在各模块的性能统计功能中。常量字符串 ``cnstream::kINPUT_PROFILER_NAME`` 作为这一处理过程的标识，可通过该字符串获取插件输入队列相关的性能数据。

使用该常量字符串，参考 `获取指定处理过程的性能数据`_ 即可获得模块输入数据队列的性能数据。

获取模块Process函数性能数据
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

对于模块的Process函数，下面两个时间点会被打桩：

- 调用Process函数之前一刻记录开始时间。
- 数据经过Process处理，并且调用 ``TransmitData`` 接口时，记录时间作为Process结束时间。

通过记录这两个时间节点，来统计模块Process函数的性能数据。

数据通过模块Process函数的过程作为一个处理过程，被CNStream默认注册在各模块的性能统计功能中。常量字符串 ``cnstream::kPROCESS_PROFILER_NAME`` 作为这一处理过程的标识，可通过该字符串获取模块 ``Process`` 函数的性能数据。

使用该常量字符串，参考 `获取指定处理过程的性能数据`_ 即可获得 ``Process`` 函数的性能数据。

.. _统计模块的性能:

统计模块的性能
---------------------

CNStream通过 ``ModuleProfiler::RegisterProcessName`` 函数来自定义模块的性能统计。

通过 ``ModuleProfiler::RegisterProcessName`` 函数传入一个字符串，这个字符串用来标识某一个处理过程。在调用 ``ModuleProfiler::RecordProcessStart`` 和 ``ModuleProfiler::RecordProcessEnd`` 时，通过传入这个字符串，来标识当前是对哪个处理过程进行性能统计。

以下用自定义模块来模拟使用流程：

1. `开启性能统计功能`_。

2. 在自定义模块的 ``Open`` 函数中调用 ``ModuleProfiler::RegisterProcessName`` 注册一个自定义性能统计过程。示例代码如下：

   ::
   
     static const std::string my_process_name = "AffineTransformation";
   
     bool YourModule::Open(ModuleParamSet params) {
       ModuleProfiler* profiler = this->GetProfiler();
       if (profiler) {
         if (!profiler->RegisterProcessName(my_process_name)) {
           LOG << "Register [" << my_process_name << "] failed.";
           return false;
         }
       }
       return true;
     }
   
   .. attention::
      | ``ModuleProfiler::RegisterProcessName`` 函数中传递的字符串应保证唯一性，即已经注册使用过的字符串不能再次被注册使用，否则注册将失败，接口返回false。
      | ``cnstream::kPROCESS_PROFILER_NAME`` 和 ``cnstream::kINPUT_PROFILER_NAME`` 两个字符串已经被CNStream作为模块 ``Process`` 函数和模块输入队列的性能统计标识注册使用，请不要再使用同名字符串。
   
3. 在需要进行性能统计的代码前后分别调用 ``ModuleProfiler::RecordProcessStart`` 和 ``ModuleProfiler::RecordProcessEnd``。下面以统计 ``AffineTransformation`` 函数的性能数据为例，在 ``AffineTransformation`` 函数前后打桩。

   ::
   
     void AffineTransformation(std::shared_ptr<cnstream::CNFrameInfo> frame_info);
   
     int YourModule::Process(std::shared_ptr<cnstream::CNFrameInfo> frame_info) {
       ...
   
       cnstream::RecordKey key = std::make_pair(frame_info->stream_id, frame_info->timestamp);
   
       if (this->GetProfiler()) {
         this->GetProfiler()->RecordProcessStart(my_process_name);
       }
   
       AffineTransformation(frame_info);
   
       if (this->GetProfiler()) {
         this->GetProfiler()->RecordProcessStart(my_process_name);
       }
   
       ...
   
       return 0;
     }
   
   代码中， ``key`` 为一帧数据的唯一标识，由 ``CNFrameInfo`` 结构中的 ``stream_id`` 字段和 ``timestamp`` 字段构成。


4. 使用注册处理过程时的字符串，获取自定义处理过程的性能统计数据。详情参考 `获取指定处理过程的性能数据`_。

.. _Pipeline端到端的性能统计:

Pipeline端到端的性能统计
--------------------------

pipeline端到端的性能统计，在数据进入pipeline和数据离开pipeline两个时间点分别记录时间，来统计性能。不包括统计pipeline中各模块、各处理过程等。用户可以通过 ``PipelineProfiler`` 实例来完成性能统计。

pipeline端到端的性能统计结果存放在 ``PipelineProfile::overall_profile`` 中。详情查看 获取Pipeline整体性能数据_。

获取性能统计结果
-------------------------

获取Pipeline整体性能数据
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

pipeline整体性能数据的统计包括各模块、各处理过程、各数据流以及pipeline端到端的性能统计结果。从时间轴上可以分为：从开始到结束的性能数据和某一个时间段的性能数据。

通过 ``PipelineProfiler`` 提供的 ``GetProfile`` 重载函数、 ``GetProfileBefore`` 、 ``GetProfileAfter`` 函数以获取pipeline的整体性能统计结果。这些函数都返回类型为 ``PipelineProfile`` 的数据。

获取从开始到结束的性能数据
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

通过 ``PipelineProfiler::GetProfile`` 的无参数版本函数用来获取从pipeline开始执行到pipeline停止执行这段时间内的性能数据。

使用示例:

::

  cnstream::PipelineProfile profile = pipeline.GetProfile();

.. attention::
   |  - 要使用上述接口获取性能数据需要打开性能统计功能，性能统计功能打开方式请参阅 `开启性能统计功能`_ 。
   |  - 若未正确打开性能统计功能，调用上述接口将返回空数据。

获取某一个时间段的性能数据
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

通过 ``PipelineProfiler::GetProfile`` 的两个参数版本函数和 ``PipelineProfiler::GetProfileBefore`` 以及 ``PipelineProfiler::GetProfileAfter`` 三个函数用来获取pipeline执行过程中某一段时间的性能数据。

以下提供使用两个参数版本的 ``PipelineProfiler::GetProfile`` 的使用示例，来获取 ``start`` 到 ``end`` 之间这段时间内的性能统计结果。其它两个接口的使用说明请参阅头 ``framework/core/include/profiler/pipeline_profiler.hpp`` 文件声明或参考《寒武纪CNStream开发者手册》。

::

  cnstream::Time start = cnstream::Clock::now();
  sleep(2);
  cnstream::Time end = cnstream::Clock::now();

  cnstream::PipelineProfile profile = pipeline.GetProfile(start, end);


.. attention::
   |  - 要使用上述三个接口获取指定时间段的性能数据，需要打开性能统计功能和数据流追踪功能。打开方式请参阅 `开启性能统计功能`_ 及 :ref:`打开数据流追踪功能` 。
   |  - 若未正确打开性能统计功能，调用上述接口将返回空数据。
   |  - 若未正确打开追踪功能，调用上述接口将返回空数据，并打印一条WARNING级别的日志。

获取pipeline端到端的性能数据
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

``PipelineProfile`` 结构中的 ``overall_profile`` 字段存储了数据从进入pipeline到离开pipeline这个过程的性能数据，被用来评估pipeline处理数据的能力。

``overall_profile`` 字段的类型为 ``ProcessProfile``，其中带有吞吐、处理的数据帧数量、时延等一系列用来评估pipelne性能的数据。详情可参考 ``framework/core/include/profiler/profile.hpp`` 头文件或者《寒武纪CNStream开发者手册》中对ProcessProfile结构体的说明。

获取指定模块的性能数据
>>>>>>>>>>>>>>>>>>>>>>>>>>>>

``PipelineProfile`` 结构中的 ``module_profiles`` 字段存储了所有模块的性能数据。

它的类型为 ``std::vector<ModuleProfile>`` 。``ModuleProfile::module_name`` 中存储着模块名字，要获取指定模块的性能数据可通过模块名字从 ``module_profiles`` 中查找。

示例代码如下：

::

  cnstream::PipelineProfile pipeline_profile = pipeline.GetProfile();
  const std::string my_module_name = "MyModule";
  cnstream::ModuleProfile my_module_profile;
  for (const cnstream::ModuleProfile& module_profile : pipeline_profile.module_profiles) {
    if (my_module_name == module_profile.module_name) {
      my_module_profile = module_profile;
      break;
    }
  }

获取指定处理过程的性能数据
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

``ModuleProfile`` 结构中的 ``process_profiles`` 存放着模块注册的所有处理过程的性能数据，包括两个 `内置处理过程`_ 的性能统计结果和自定义处理过程的性能统计结果。

``process_profiles`` 的类型为 ``std::vector<ProcessProfile>`` 。 ``ProcessProfile::process_name`` 为注册处理过程时提供的处理过程唯一标识字符串。

要获取指定处理过程的性能数据可通过处理过程的唯一标识字符串来查找。

示例代码如下：

::

  cnstream::ModuleProfile module_profile;
  const std::string my_process_name = "AffineTransformation";
  cnstream::ProcessProfile my_process_profile;
  for (const cnstream::ProcessProfile& process_profile : module_profile.process_profiles) {
    if (process_profile.process_name == my_process_name) {
      my_process_profile = process_profile;
      break;
    }
  }

``ProcessProfile`` 结构中还存有吞吐速度、时延、最大最小时延、处理的数据帧数目、丢弃的数据帧数目等性能参考数据。详情可查看 ``framework/core/include/profiler/profile.hpp`` 或参看《寒武纪CNStream开发者手册》中对该结构的说明。

获取每一路数据流的性能数据
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

每个处理过程都包含经过这个处理过程的所有数据流的性能数据。存放于 ``ProcessProfile::stream_profiles`` 中。

``ProcessProfile::stream_profiles`` 的类型为 ``std::vector<StreamProfile>``。

``StreamProfile::stream_name`` 即往pipeline中加入数据流时指定的数据流名称。

``StreamProfile`` 结构中还存有吞吐速度、时延、最大最小时延、处理的数据帧数目、丢弃的数据帧数目等性能参考数据。详情可查看 ``framework/core/include/profiler/profile.hpp`` 或《寒武纪CNStream开发者手册》中对该结构的说明。

示例代码如下：

::

  cnstream::ProcessProfile process_profile;
  for (const cnstream::StreamProfile& stream_profile : process_profile.stream_profiles) {
    // stream_profile.stream_name : stream id.
    // stream_profile.fps : throughput.
    // stream_profile.latency : average latency
  }

性能统计数据说明
--------------------

性能统计功能的基本对象是一个处理过程。对于每个处理过程，会统计总体的性能数据并存放在 ``ProcessProfile`` 结构的各字段中。每个处理过程还会分别统计每路数据流经过该处理过程的性能数据，存放在 ``ProcessProfile`` 结构的 ``stream_profiles`` 字段中。

每路数据流的性能由 ``StreamProfile`` 结构表示，内部的性能数据与 ``ProcessProfile`` 结构中表示性能数据的字段名与含义一致， ``ongoing`` 字段除外，它只存在于 ``ProcessProfile`` 结构中， ``StreamProfile`` 中不统计这个性能数据。

``ProcessProfile`` 中各字段及其表示的含义如下：

.. tabularcolumns:: |m{0.2\textwidth}|m{0.6\textwidth}|
.. table:: 性能统计字段说明

    +-----------------+---------------------------------------------------------------+
    | 字段名称        |                描述                                           |
    +=================+===============================================================+
    | completed       |表示已经处理完毕的数据总量，不包括丢弃的数据帧。               |
    +-----------------+---------------------------------------------------------------+
    | dropped         |表示被丢弃的数据总量。                                         |  
    |                 |                                                               |  
    |                 |当一个数据记录了开始时间，但是比它更后记录开始时间的数据       |  
    |                 |已经结束了超过16个（取自h.264、h.265 spec中的MaxDpbSize），    |  
    |                 |则视为该数据帧已经丢弃。例如一个模块中存在丢帧逻辑，则会出     |  
    |                 |现数据经过模块的 ``Process`` 函数，但是 ``TransmitData`` 不    |  
    |                 |会被调用的情况，此时则会把这样的数据帧数量累加到dropped字段上。|  
    +-----------------+---------------------------------------------------------------+
    | counter         |表示统计到的对应处理过程已经处理完毕的数据的总量。被丢弃的     |
    |                 |数据也视为处理完毕的数据，会被累加在到counter上。              |
    |                 |``counter`` = ``completed`` + ``dropped``。                    |
    +-----------------+---------------------------------------------------------------+
    | ongoing         |表示正在处理，但是未被处理完毕的数据总量。即已经记录到开始时间 |
    |                 |但是未记录到结束时间的数据总量。                               |
    +-----------------+---------------------------------------------------------------+
    | latency         |平均时延，单位为毫秒。                                         |
    +-----------------+---------------------------------------------------------------+
    | maximum_latency |最大处理时延，单位为毫秒。                                     |
    +-----------------+---------------------------------------------------------------+
    | minimum_latency |最小处理时延，单位为毫秒。                                     |
    +-----------------+---------------------------------------------------------------+
    | fps             |平均吞吐速度，单位为帧/秒。                                    |
    +-----------------+---------------------------------------------------------------+


示例代码
---------------------------------

CNStream提供示例代码存放在 ``samples/demo/demo.cpp`` 中。该示例展示了如何每隔两秒获取一次性能数据，并且打印完整的性能数据和最近两秒的性能数据。

``samples/bin/demo`` 可执行文件中使用 ``perf_level`` 参数控制打印的性能数据的详细程度。

``perf_level`` 可选值有[0, 1, 2, 3]，默认值为0：

- 当 ``perf_level`` 为0时，只打印各处理过程的 ``counter`` 统计值与 ``fps`` （吞吐）统计值。

- 当 ``perf_level`` 为1时，在0的基础上加上 ``latency`` 、``maximum_latency`` 、 ``minimum_latency`` 三个统计值的打印。

- 当 ``perf_level`` 为2时，打印 ``ProcessProfile`` 结构中的所有性能统计值。

- 当 ``perf_level`` 为3时，在2的基础上打印每路数据流的性能统计数据。



完整性能数据示例
>>>>>>>>>>>>>>>>>>>>>>

完整性能打印示例如下：

::

  **********************  Performance Print Start  (Whole)  **********************
  ===========================  Pipeline: [MyPipeline]  ===========================
  ------------------------------ Module: [displayer] -----------------------------
  ----------Process Name: [INPUT_QUEUE]
  [Counter]: 592, [Throughput]: 35118.1fps
  ----------Process Name: [PROCESS]
  [Counter]: 592, [Throughput]: 135526fps
  --------------------------------- Module: [osd] --------------------------------
  ----------Process Name: [INPUT_QUEUE]
  [Counter]: 592, [Throughput]: 748.563fps
  ----------Process Name: [PROCESS]
  [Counter]: 592, [Throughput]: 680.162fps
  ------------------------------- Module: [source] -------------------------------
  ----------Process Name: [PROCESS]
  [Counter]: 597, [Throughput]: 59.7144fps
  ------------------------------ Module: [detector] ------------------------------ (slowest)
  ----------Process Name: [RUN MODEL]
  [Counter]: 592, [Throughput]: 444.986fps
  ----------Process Name: [RESIZE CONVERT]
  [Counter]: 592, [Throughput]: 6569.07fps
  ----------Process Name: [PROCESS]
  [Counter]: 592, [Throughput]: 59.6681fps
  ----------Process Name: [INPUT_QUEUE]
  [Counter]: 597, [Throughput]: 11810.1fps

  -----------------------------------  Overall  ----------------------------------
  [Counter]: 592, [Throughput]: 59.2285fps
  ***********************  Performance Print End  (Whole)  ***********************

最近两秒的性能数据打印示例
>>>>>>>>>>>>>>>>>>>>>>>>>>>

最近两秒的性能数据打印示例如下：

::

  *****************  Performance Print Start  (Last two seconds)  ****************
  ===========================  Pipeline: [MyPipeline]  ===========================
  ------------------------------ Module: [displayer] -----------------------------
  ----------Process Name: [PROCESS]
  [Counter]: 112, [Throughput]: 134805fps
  ----------Process Name: [INPUT_QUEUE]
  [Counter]: 112, [Throughput]: 35815.7fps
  --------------------------------- Module: [osd] --------------------------------
  ----------Process Name: [PROCESS]
  [Counter]: 112, [Throughput]: 686.523fps
  ----------Process Name: [INPUT_QUEUE]
  [Counter]: 112, [Throughput]: 753.897fps
  ------------------------------- Module: [source] -------------------------------
  ----------Process Name: [PROCESS]
  [Counter]: 119, [Throughput]: 61.1041fps
  ------------------------------ Module: [detector] ------------------------------ (slowest)
  ----------Process Name: [RUN MODEL]
  [Counter]: 112, [Throughput]: 443.628fps
  ----------Process Name: [RESIZE CONVERT]
  [Counter]: 112, [Throughput]: 6710.72fps
  ----------Process Name: [INPUT_QUEUE]
  [Counter]: 119, [Throughput]: 20385.1fps
  ----------Process Name: [PROCESS]
  [Counter]: 112, [Throughput]: 56.9688fps

  -----------------------------------  Overall  ----------------------------------
  [Counter]: 112, [Throughput]: 56.7687fps
  ******************  Performance Print End  (Last two seconds)  *****************
