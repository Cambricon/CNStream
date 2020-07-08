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
- Module：Pipeline的每个处理阶段是一个组件，对应 **cnstream::Module** 类。每一个具体的module都是 **cnstream::Module** 的派生类。
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

**cnstream::Pipeline** 类实现了pipeline的搭建、module管理、以及module的调度执行。在module自身不传递数据时，负责module之间的数据传递。此外，该类集成事件总线，提供注册事件监听器的机制，使用户能够接收事件。例如stream EOS（End of Stream）等。Pipeline通过隐含的深度可控的队列来连接module，使用module的输入队列连接上游的module。CNStream也提供了根据JSON配置文件来搭建pipeline的接口。在不重新编译源码的情况下，通过修改配置文件搭建不同的pipeline。

.. attention::
  |  Pipeline的source module是没有输入队列，pipeline中不会为source module启动线程，也就是说pipeline不会调度source module。source module通过pipeline的 ``ProvideData`` 接口向下游模块发送数据和启动内部线程。

**cnstream::Pipeline** 类在 ``cnstream_pipeline.hpp`` 文件内定义，主要接口如下。 ``cnstream_pipeline.hpp`` 文件存放于 ``framework/core/include`` 目录下。源代码中有详细的注释，这里仅给出必要的说明。接口详情，查看《CNStream Developer Guide》。

::

  class Pipeline {
    ...
   public:
    //  根据ModuleConfigs或者JSON配置文件来搭建pipeline。
    //  实现这两者前提是能够根据类名字创建类实例即反射(reflection)机制。
    //  在cnstream::Module类介绍中会进行描述。
    int BuildPipeline(const std::vector<CNModuleConfig>& configs);
    int BuildPipelineByJSONFile(const std::string& config_file);

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
       "class_name" : "cnstream::DataSource",  //指定module使用哪个类来创建。
       "parallelism" : 0, //框架创建的module线程数目。source module不使用这个字段。
       "next_modules" : ["inference"],        //下一个连接模块的名字，可以有多个。
       "custom_params" : {                    //当前module的参数。
         "source_type" : "ffmpeg",            //使用ffmpeg作为demuxer。
         "output_type" : "mlu",               //解码图像输出到MLU内存。
         "decoder_type" : "mlu",              //使用MLU解码。
         "device_id" : 0                      //MLU设备id。
       }
     },

    "inference" : {
      "class_name" : "cnstream::Inferencer",
      "parallelism" : 16,            //框架创建的模块线程数，也是输入队列的数目。
      "max_input_queue_size" : 32,   //输入队列的最大长度。
      "custom_params" : {
	// 使用寒武纪工具生成的离线模型，支持绝对路径和JSON文件的相对路径。
        "model_path" : "/data/models/resnet34_ssd.cambricon",  
        "func_name" : "subnet0",
        "device_id" : 0
      }
    }
  }

.. _module:

cnstream::Module类
-------------------

CNStream SDK要求所有的Module类使用统一接口和数据结构 **cnstream::CNFrameInfo** 。从框架上要求了module的通用性，并简化了module的编写。实现具体module的方式如下：

- 从 **cnstream::Module** 派生：适合功能单一，内部不需要并发处理的场景。Module实现只需要关注对CNFrameInfo的处理，由框架传递（transmit）CNFrameInfo。
- 从 **cnstream::ModuleEx** 派生： Module除了处理CNFrameInfo之外，还负责CNFrameInfo的传递，以及保证数据顺序带来的灵活性，从而可以实现内部并发。

配置搭建pipeline的基础是实现根据module类名字创建module实例，因此具体module类还需要继承 **cnstream::ModuleCreator** 。

一个module的实例，会使用一个或者多个线程对多路数据流进行处理，每一路数据流使用pipeline范围内唯一的 ``stream_id`` 进行标识。此外从 **cnstream::IModuleObserver** 接口类继承实现一个观察者，并通过 ``SetObserver`` 注册到module中，应用程序就可以观察每个module处理结果。详细代码编写结构，可参考 ``example/example.cpp``。

**cnstream::Module** 类在 ``cnstream_module.hpp`` 文件定义，主要接口如下。``cnstream_module.hpp`` 文件存放在 ``framework/core/include`` 文件夹下。源代码中有详细的注释，这里仅给出必要的说明。接口详情，查看《CNStream Developer Guide》。

::

  class Module {
   public:

    // 一个pipeline中，每个module名字必须唯一。
    explicit Module(const std::string &name) : name_(name) { }
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
    //  0   -- 表示已经处理完毕，传递数据操作由框架完成。
    //  > 0 -- 表示已经接收数据，在后台进行后续处理。传递数据操作由module自身完成。
    //  < 0 -- 表示有错误产生。
    virtual int Process(std::shared_ptr<CNFrameInfo> data) = 0;

    ...
    // 向pipeline发送消息，如Stream EOS。
    bool PostEvent(EventType type, const std::string &msg) ;
 
    // 注册一个观察者。
    void SetObserver(IModuleObserver *observer);
  };

.. _CNFrameInfo:

cnstream::CNFrameInfo类
------------------------

**cnstream::CNFrameInfo** 类是module之间传递的数据结构，即pipeline的Context。该类在 ``cnstream_frame.hpp`` 文件中定义。``cnstream_frame.hpp`` 文件存放在 ``framework/core/include`` 文件夹下。这个类主要包括 ``ThreadSafeUnorderedMap<int, cnstream::any> datas`` ，用于存放任意数据类型的数据帧和推理结果。

``cnstream::any`` 等同于C++17标准的 ``std::any`` 类型, 即C++弱类型特性。使用该类型定义 ``datas`` 能够支持用户自定义任意数据类型，且依然具备类型安全功能。比如CNStream内置插件库中针对智能视频分析场景专门定义了 ``CNDataFrame`` 和 ``CNInferObject``, 分别用于存放视频帧数据和神经网络推理结果，详述如下。用户使用该类型数据前需要转换成自定义类型，例如：

::

  auto frame = cnstream::any_cast<CNDataFrame>(datas[CNDataFrameKey])

CNDataFrame中集成了SyncedMemory。基于MLU平台的异构性，在应用程序中，当某个具体的module处理的数据可能需要在CPU上或者MLU上时，SyncedMem实现了CPU和MLU（Host和Device）之间的数据同步。通过SyncedMem，module可以自身决定访问保存在MLU或者CPU上的数据，从而简化module的编写，接口如下：

::

  std::shared_ptr<CNSyncedMemory> data[CN_MAX_PLANES];

CNDataFrame中的SyncedMem支持deep copy或者复用已有的内存。当管理Codec和Inference之间的image buffer时，可以进行deep copy和复用decoder的buffer内存。decoder和后续的inference处理完全解耦，但是会带来dev2dev copy的代价。

另外，CNInferObject不仅提供对常规推理结果的数据存储机制，还提供用户自定义数据格式的接口 ``AddExtraAttribute`` ，方便用户使用其他格式传递数据，如JSON格式。

::

  bool AddExtraAttribute(const std::vector<std::pair<std::string, std::string>>& attributes);
  std::string GetExtraAttribute(const std::string& key);

cnstream::EventBus类
---------------------

**cnstream::EventBus** 类是各个模块与pipeline通信的事件总线。各模块发布事件到总线上，由总线监听器接收。一条事件总线可以拥有多个监听器。

每条pipeline有一条事件总线及对应的一个默认事件监听器。pipeline会对事件总线进行轮询，收到事件后分发给监听器。

**cnstream::EventBus** 类在 ``cnstream_eventbus.hpp`` 文件中定义，主要接口如下。``cnstream_eventbus.hpp`` 文件存放在 ``framework/core/include`` 文件夹下。源代码中有详细的注释，这里仅给出必要的说明。接口详情，查看《CNStream Developer Guide》。

::

  class EventBus {
   public:

    // 向事件总线上发布一个事件。
    bool PostEvent(Event event);

    // 添加事件总线的监听器。
    uint32_t AddBusWatch(BusWatcher func, Pipeline *watcher);
    ......
  };

cnstream::Event类
---------------------

**cnstream::Event** 类是模块和pipepline之间通信的基本单元，即事件。事件由四个部分组成：事件类型、消息、发布事件的模块、发布事件的线程号。消息类型包括：无效、错误、警告、EOS(End of Stream)、停止，以及一个预留类型。

**cnstream::Event** 类在 ``cnstream_eventbus.hpp`` 文件定义，``cnstream_eventbus.hpp`` 文件存放在 ``framework/core/include`` 文件夹下。接口详情，查看《CNStream Developer Guide》。

