框架介绍
===========

核心框架
----------

CNStream SDK基于管道（Pipeline）和事件总线（EventBus）实现了模块式数据处理流程。

Pipeline类似一个流水线，把复杂问题的解决方案分解成一个个处理阶段，然后依次处理。一个处理阶段的结果是下一个处理阶段的输入。
Pipeline模式的类模型由三部分组成：

- Pipeline：代表执行流。
- Module：代表执行流中的一个阶段。
- Context：是Module执行时的上下文信息。

EventBus模式主要用来处理事件，包括三个部分：

- 事件源（Event Source）：将消息发布到事件总线上。

- 事件监听器（Observer/Listener）：监听器订阅事件。

- 事件总线（EventBus）：事件发布到总线上时被监听器接收。

Pipeline和EventBus模式实现了CNStream框架。相关组成以及在CNStream SDK实现中对应关系如下：

- Pipeline：对应 **cnstream::Pipeline** 类。
- Module：Pipeline的每个处理阶段是一个组件，对应 **cnstream::Module** 类。每一个具体的mdule都是 **cnstream::Module** 的派生类。
- FrameInfo：Pipeline模式的Context，对应 **cnstream::CNFrameInfo** 类。
- Event-bus和Event：分别对应 **cnstream::EventBus** 类和 **cnstream::Event** 类。

CNStream既支持构造线性模式的pipeline，也支持搭建非线性形状的pipeline，例如split、join模式，如下所示：

::

    ModuleA------ModuleB------ModuleC


::

                |------ModuleB------|
    ModuleA---- |                   | ---- ModuleD
                |------ModuleC------|

cnstream::Pipeline类
---------------------

**cnstream::Pipeline** 类实现了pipeline的搭建、module管理、以及module的调度执行。在module自身不传递数据时，负责module之间的数据传递。此外，该类集成事件总线，提供注册事件监听器的机制，使用户能够接收事件。例如stream EOS等。Pipeline通过隐含的深度可控的队列来连接module，使用module的输入队列连接上游的module。CNStream也提供了根据JSON配置文件来搭建pipeline的接口。在不重新编译源码的情况下，通过修改配置文件搭建不同的pipeline。

.. attention::
  |  Pipeline的source module是没有输入队列，pipeline中不会为source module启动线程，也就是说pipeline不会调度source module。source module通过pipeline的 ``ProvideData`` 接口向下游模块发送数据和启动内部线程。

**cnstream::Pipeline** 类在 ``cnstream_pipeline.hpp`` 文件内定义，主要接口如下。 ``cnstream_pipeline.hpp`` 文件存放于 ``modules/core/include`` 目录下。源代码中有详细的注释，这里仅给出必要的说明。

::

  class Pipeline {
    ...
   public:
    //  根据ModuleConfigs或者JSON配置文件来搭建pipeline。
    //  实现这两者前提是能够根据类名字创建类实例即反射(reflection)机制。
    //  在cnstream::Module类介绍中会进行描述。
    int BuildPipeline(const std::vector<CNModuleConfig>& configs);
    int BuildPipelineByJSONFile(const std::string& config_file) noexcept(false);

    ...

    // 向某个module发送CNFrameInfo,比如向一个pipeline的source module发送图像数据。
    bool ProvideData(const Module* module, std::shared_ptr<CNFrameInfo> data);

    ...
    // 开始和结束pipeline service。
    bool Start();
    bool Stop();

    ...
    // 根据moduleName获得module instance。
    Module* GetModule(const std::string& moduleName);
    ...
  };

ModuleConfigs（JSON）的示例如下。JSON配置文件支持C和C++风格的注释。

::

  {
    {
      "source" : {
       "class_name" : "DataSource",     //指定module使用哪个类来创建。
       "parallelism" : 0, //framework创建的module线程数目。source module不使用这个字段。
       "next_modules" : ["inference"], //下一个连接模块的名字，可以有多个。
       "custom_params" : {             //当前module的参数。
         "source_type" : "ffmpeg",    //使用ffmpeg作为demuxer。
         "output_type" : "mlu",      //解码图像输出到MLU内存。
         "decoder_type" : "mlu",    //使用CNDecoder。
         "reuse_cndec_buf": "true", //复用CNDecoder的输出image buffer。
         "device_id" : 0           //MLU设备id。
       }
     },

    "inference" : {
      "class_name" : "M220Inference",
      "parallelism" : 16,            //framwork创建的模块线程数，也是输入队列的数目。
      "max_input_queue_size" : 32,   //输入队列的最大长度。
      "next_modules" : ["fps_stats"],
      "custom_params" : {
	    // 使用寒武纪工具生成的离线模型。
        "model_path" : "/data/models/resnet34_ssd.cambricon", 
        "func_name" : "subnet0",
        "device_id" : 0,
        "batch_size" : 4, //M220 Inference实现中batch的最大数目。
        "worker_num" : 8  //M220 Inference内部创建的线程池的线程数目。
      }
    },

    "fps_stats" : {
      "class_name" : "cnstream::FpsStats",
      "parallelism" : 4,
      "max_input_queue_size" : 32
    }
  }

cnstream::Module类
-------------------

CNStream SDK要求所有的Module类使用统一接口和数据结构 **cnstream::CNFrameInfo** 。从框架上要求了module的通用性，并简化了module的编写。实现具体module的方式如下：

- 从 **cnstream::Module** 派生：适合功能单一，内部不需要并发处理的场景。Module实现只需要关注对CNFrameInfo的处理，由框架传递（transmit）CNFrameInfo。
- 从 **cnstream::ModuleEx** 派生： Module除了处理CNFrameInfo之外，还负责CNFrameInfo的传递，以及保证数据顺序带来的灵活性，从而可以实现内部并发。

配置搭建pipeline的基础是实现根据module类名字创建module实例，因此具体module类还需要继承 **cnstream::ModuleCreator** 。

一个module的实例，会使用一个或者多个线程对多路数据流进行处理，每一路数据流使用pipeline范围内唯一的 ``stream_id`` 进行标识。

**cnstream::Module** 类在 ``cnstream_module.hpp`` 文件定义，主要接口如下。``cnstream_module.hpp`` 文件存放在 ``modules/core/include`` 文件夹下。源代码中有详细的注释，这里仅给出必要的说明。

::

  class Module {
   public:

    // 一个pipeline中，每个module名字必须唯一。
    explicit Module(const std::string &name);
    ...

    // 必须实现Open、Close和Process接口。这三个接口会被pipeline调用。
    // 通过Open接口接收参数，分配资源。
    // 通过Close接口释放资源。
    // 通过Process接口接收需要处理的数据，并更新CNFrameInfo。
    virtual bool Open(ModuleParamSet param_set) = 0;
    virtual void Close() = 0;

    // 特别注意：Process处理多个stream的数据, 由多线程调用。
    // 单路stream的CNFrameInfo会在一个线程中处理。
    // Process的返回值：
    //  0 -- 表示已经处理完毕，传递数据操作由框架完成。
    //  1 -- 表示已经接收数据，在后台进行后续处理。传递数据操作由module自身完成。
    //  < 0 -- 表示有错误产生。
    virtual int Process(std::shared_ptr<CNFrameInfo> data) = 0;

    ...
    // 向pipeline发送消息，如Stream EOS。
    bool PostEvent(EventType type, const std::string &msg) const;
  };

cnstream::CNFrameInfo类
------------------------

**cnstream::CNFrameInfo** 类是module之间传递的数据结构，即pipeline的Context。该类在 ``cnstream_frame.hpp`` 文件中定义。``cnstream_frame.hpp`` 文件存放在 ``modules/core/include`` 文件夹下。这个数据结构包括了CNDataFrame和CNFrameInfo。

CNFrameInfo用于数据和推理结果，并对pipeline中单路stream使用的DataFrame的数目进行限制，我们称之为pipeline的并发深度，接口如下：

::

  cnstream::SetParallelism(int value)；

CNDataFrame中集成了SyncedMemory。基于MLU平台的异构性，在应用程序中，当某个具体的module处理的数据可能需要在CPU上或者MLU上时，SyncedMem实现了CPU和MLU（Host和Device）之间的数据同步。通过SyncedMem，module可以自身决定访问保存在MLU或者CPU上的数据，从而简化module的编写，接口如下：

::

  std::shared_ptr<CNSyncedMemory> data[CN_MAX_PLANES];

CNDataFrame中的SyncedMem支持deep copy或者复用已有的内存。当管理CNDecoder和Inference之间的image buffer时，可以进行deep copy和复用decoder的buffer内存。decoder和后续的inference处理完全解耦，但是会带来dev2dev copy的代价。

另外，CNInferObject不仅提供对常规推理结果的数据存储机制，还提供用户自定义数据格式的接口 ``extra_attributes_`` ，方便用户使用其他格式传递数据，如JSON格式。

::

  std::map<std::string, std::string> extra_attributes_;