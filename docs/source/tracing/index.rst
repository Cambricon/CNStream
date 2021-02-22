
数据流的追踪
======================

CNStream提供数据流追踪机制用于帮助分析程序执行流程。CNStream的追踪机制主要记录数据在pipeline中流转过程中各节点发生的时间、类型（开始、结束）、事件级别（模块级别、pipeline级别）、事件名称等信息。

.. _打开数据流追踪功能:

开启数据流追踪功能
---------------------

数据追踪功能默认是关闭状态。要打开数据追踪功能，有以下两种方式：

- 通过 ``Pipeline::BuildPipeline`` 函数构建pipeline，将 ``profiler_config.enable_tracing`` 参数设为 **true** 开启数据追踪功能。并设置追踪数据占用的内存空间参数 trace_event_capacity_。示例代码如下：

  ::
  
    cnstream::Pipeline pipeline;
    cnstream::ProfilerConfig profiler_config;
    profiler_config.enable_tracing = true;
    profiler_config.trace_event_capacity = 100000;
    pipeline.BuildPipeline(module_configs, profiler_config);

- 通过配置文件的方式构建pipeline，即使用 ``Pipeline::BuildPipelineByJSONFile`` 函数构建pipeline。在json格式的配置文件中填入数据追踪相关参数，打开数据追踪功能。

  示例配置文件如下。在 ``profiler_config`` 项中 ``enable_tracing`` 子项被置为 **true**，表示打开pipeline的数据追踪功能。并设置追踪数据占用的内存空间参数 trace_event_capacity_。 ``module1`` 和 ``module2`` 为两个模块的配置。pipeline配置文件中 ``profiler_config`` 项和模块配置项之间没有顺序要求。
   
   ::
   
     {
       "profiler_config" : {
         "enable_tracing" : true,
         "trace_event_capacity" : 100000
       },
     
       "module1" : {
         ...
       },
     
       "module2" : {
         ...
       }
     }


事件记录方式
-----------------------

在数据流追踪中，事件的记录方式分为两种。

- 数据流追踪的同时，对事件进行性能分析，记录各处理过程的开始事件和结束事件。即在 ``ModuleProfiler::RecordProcessStart`` 和 ``ModuleProfiler::RecordProcessEnd`` 调用时，会记录相关处理过程的追踪事件。详情可查看 :ref:`性能统计` 。

- 仅做数据流追踪，不做性能分析。记录事件还可以使用 ``PipelineTracer::RecordEvent`` 函数进行。调用该函数需要传入一个名为 ``event`` 的参数，类型为 ``TraceEvent``。
  
  ``TraceEvent`` 需要填入以下信息:
  
  .. tabularcolumns:: |m{0.2\textwidth}|m{0.6\textwidth}|
  .. table:: 数据流追踪字段说明
  
      +--------------+-------------------------------------------------------------------+
      | 字段名称     |                描述                                               |
      +==============+===================================================================+
      | key          |key为一个数据的唯一标识，一般可通过                                |
      |              |``std::make_pair(CNFrameInfo::stream_id, CNFrameInfo::timestamp)`` |
      |              |的方式构造。                                                       |
      +--------------+-------------------------------------------------------------------+
      | module_name  |事件发生的模块名。                                                 |    
      +--------------+-------------------------------------------------------------------+
      | process_name |事件名称。当追踪事件是伴随着性能统计发生的，那么 ``process_name``  |
      |              |为注册在性能统计功能中的某一个处理过程的名称。                     |
      +--------------+-------------------------------------------------------------------+
      | time         |发生事件的事件点，一般可调用 ``cnstream::Clock::now()`` 获得。     |
      +--------------+-------------------------------------------------------------------+
      | level        |可选值为 ``cnstream::TraceEvent::Level::PIPELINE`` 及              |
      |              |``cnstream::TraceEvent::Level::MODULE``。 分别表示pipeline端到端   |
      |              |的事件和模块级别的事件。                                           |
      |              |                                                                   |
      |              |该值决定了获取到的事件数据中这个事件的存放位置。                   |
      |              |详情请查看 `获取追踪数据`_ 。                                      |
      +--------------+-------------------------------------------------------------------+
      | type         |可选值为 ``cnstream::TraceEvent::Type::START`` 及                  |
      |              |``cnstream::TraceEvent::Type::END`` 。                             |
      |              |                                                                   |
      |              |若追踪事件是伴随着性能统计发生的，那么当此事件是某个处理过程的开   |
      |              |始，则type为 ``cnstream::TraceEvent::Type::START`` ，              |
      |              |否则为 ``cnstream::TraceEvent::Type::END`` 。                      |
      +--------------+-------------------------------------------------------------------+
  
.. _trace_event_capacity:

追踪数据占用的内存空间
----------------------------

追踪记录的数据存储在内存中，通过循环数组进行存储。循环数组的大小可在构建pipeline时设定。

通过 ``Pipeline::BuildPipeline`` 传入的 ``profiler_config`` 参数组中的 ``trace_event_capacity`` 参数指定最大存储的追踪事件个数。每个事件占用的内存大小是固定的，该参数可间接调整追踪功能的最大内存占用大小。

``trace_event_capacity`` 默认值为100000，即最大存储100000条记录。

获取追踪数据
----------------------------

``PipelineTracer`` 提供 ``GetTrace`` 函数供获取追踪功能记录的事件。

``GetTrace`` 函数可以获取某一段时间的事件信息。

该函数返回内存中指定时间段的事件，使用 ``PipelineTrace`` 结构存储。

``PipelineTrace`` 结构中的两个字段说明:

process_traces
>>>>>>>>>>>>>>>>>>>

该字段存储 ``level`` 为 ``cnstream::TraceEvent::Level::PIPELINE`` 的事件。按 ``process_name`` 归类，分别存储为 ``ProcessTrace`` 结构。

module_traces
>>>>>>>>>>>>>>>>>

该字段存储 ``level`` 为 ``cnstream::TraceEvent::Level::MODULE`` 的事件。按 ``module_name`` 归类，分别存储为 ``ModuleTrace`` 结构。

``ModuleTrace`` 中又按 ``process_name`` 归类，分别存储为 ``ProcessTrace`` 结构。

.. attention::
   |  CNStream不保证追踪数据的完整性，即有可能丢失追踪数据。保证追踪数据的完整性需要配合调整 ``trace_event_capacity`` 参数和获取追踪数据的方式。
   |  CNStream中使用循环数组来存储事件，即随着事件的不断发生，新的事件将覆盖老的事件。任一时刻，能获取到最久远的事件即为最新发生的事件之前的第 ``trace_event_capacity`` 个事件，再之前的事件则被丢弃。
   |  故想要获取完整的事件信息，应该保证事件获取速度大于事件被覆盖的速度。

追踪数据的处理
-------------------------

CNStream提供简单的追踪事件的获取方式，参考 `获取追踪数据`_ 。追踪数据的处理属于一个开放性命题，用户获取到追踪数据后，可以以任何方式进行处理。CNStream目前提供一个简单的追踪数据表现方式，参见 `追踪数据可视化`_ 。

追踪数据可视化
-----------------------

CNStream中的追踪数据可视化依赖
Google Chrome的chrome tracing功能实现。**TraceSerializeHelper** 类提供方法把追踪功能记录的数据存储为chrome tracing需要的json格式。

使用 **TraceSerializeHelper** 存储的追踪数据文件可以使用chrome tracing可视化。使用方法可参考CNStream源代码 ``samples/demo/demo.cpp`` 中的实现。

执行下面步骤追踪数据可视化:

1. 在配置文件中打开追踪功能。详情参考 `打开数据流追踪功能`_ 。

2. 指定 ``trace_data_dir`` 参数。为可执行文件 ``CNStream/samples/bin/demo`` 指定参数 ``trace_data_dir`` 为追踪数据存放目录。其中，``CNStream/samples/bin`` 是在编译CNStream后自动生成的文件夹。

3. 生成可视化JSON文件。在运行完程序后，在 ``trace_data_dir`` 参数指定的目录中即会生成 ``cnstream_trace_data.json`` 文件。

4. 可视化追踪的数据。在chrome浏览器地址栏输入 ``chrome:://tracing``，把 ``cnstream_trace_data.json`` 拖入浏览器即可查看追踪数据的可视化结果。

   可视化内容为每一帧数据在经过每一个处理过程的时间点和经历的时间长度。
   
   可视化示例：
   
   .. figure:: ../images/trace_data_visualization_example.png
   
      追踪数据可视化
