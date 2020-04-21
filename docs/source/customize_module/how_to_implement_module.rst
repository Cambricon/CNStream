创建自定义模块
=============================

概述
-----------------------------

CNStream支持用户创建自定义模块。使用CNStream框架创建自定义模块非常简单，用户只需根据 ``samples/example`` 目录下的 ``example.cpp`` 文件里给出的例子，即可方便快捷地实现自定义模块的轮廓。自定义模块需要多重继承 **cnstream::Module** 和 **cnstream::ModuleCreator** 两个基类。其中 **cnstream::Module** 是所有模块的基类。**cnstream::ModuleCreator** 实现了反射机制，提供了 ``CreateFunction``， 并注册 ``ModuleClassName`` 和 ``CreateFunction`` 至 ``ModuleFactory`` 中。

自定义普通模块
---------------------

这类模块支持多输入和多输出，数据由pipeline发送，并在模块的成员函数 ``Process`` 中处理。

::

  class ExampleModule : public cnstream::Module, public cnstream::ModuleCreator<ExampleModule> {
    using super = cnstream::Module;
  
   public:
    explicit ExampleModule(const std::string &name) : super(name) {}
    bool Open(cnstream::ModuleParamSet paramSet) override {
      std::cout << this->GetName() << " Open called" << std::endl;
      for (auto &v : paramSet) {
        std::cout << "\t" << v.first << " : " << v.second << std::endl;
      }
      return true;
    }
    void Close() override { std::cout << this->GetName() << " Close called" << std::endl; }
    int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
      // do something ...
      std::unique_lock<std::mutex> lock(print_mutex);
      std::cout << this->GetName() << " process: " << data->frame.stream_id << "--" << data->frame.frame_id;
      std::cout << " : " << std::this_thread::get_id() << std::endl;
      /*continue by the framework*/
      return 0;
    }
  
   private:
    ExampleModule(const ExampleModule &) = delete;
    ExampleModule &operator=(ExampleModule const &) = delete;
  };


自定义数据源模块
---------------------
数据源模块与普通模块基本类似，唯一不同的是这类模块没有输入只有输出，所以模块的成员函数 ``Process`` 不会被框架所调用。

::
  
  class ExampleModuleSource : public cnstream::Module, public cnstream::ModuleCreator<ExampleModuleSource> {
    using super = cnstream::Module;
  
   public:
    explicit ExampleModuleSource(const std::string &name) : super(name) {}
    bool Open(cnstream::ModuleParamSet paramSet) override {
      std::cout << this->GetName() << " Open called" << std::endl;
      for (auto &v : paramSet) {
        std::cout << "\t" << v.first << " : " << v.second << std::endl;
      }
      return true;
    }
    void Close() override { std::cout << this->GetName() << " Close called" << std::endl; }
    int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
      std::cout << "For a source module, Process() will not be invoked\n";
      return 0;
    }
  
   private:
    ExampleModuleSource(const ExampleModuleSource &) = delete;
    ExampleModuleSource &operator=(ExampleModuleSource const &) = delete;
  };



自定义扩展模块
---------------------

这类模块支持多输入和多输出。继承自 **cnstream::ModuleEx** 和 **cnstream::ModuleCreator** 类。与普通模块不同，处理过的数据由模块自行送入下一级模块。此类模块的一个典型应用是在模块内部攒batch，然后再进行批量处理。

::

  class ExampleModuleEx : public cnstream::ModuleEx, public cnstream::ModuleCreator<ExampleModuleEx> {
    using super = cnstream::ModuleEx;
    using FrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;
  
   public:
    explicit ExampleModuleEx(const std::string &name) : super(name) {}
    bool Open(cnstream::ModuleParamSet paramSet) override {
      std::cout << this->GetName() << " Open called" << std::endl;
      for (auto &v : paramSet) {
        std::cout << "\t" << v.first << " : " << v.second << std::endl;
      }
      running_.store(1);
      threads_.push_back(std::thread(&ExampleModuleEx::BackgroundProcess, this));
      return true;
    }
    void Close() override {
      running_.store(0);
      for (auto &thread : threads_) {
        thread.join();
      }
      std::cout << this->GetName() << " Close called" << std::endl;
    }
    int Process(FrameInfoPtr data) override {
      {
        std::unique_lock<std::mutex> lock(print_mutex);
        if (data->frame.flags & cnstream::CN_FRAME_FLAG_EOS) {
          std::cout << this->GetName() << " process: " << data->frame.stream_id << "--EOS";
        } else {
          std::cout << this->GetName() << " process: " << data->frame.stream_id << "--" << data->frame.frame_id;
        }
        std::cout << " : " << std::this_thread::get_id() << std::endl;
      }
      // handle data in background threads...
      q_.enqueue(data);
  
      /*notify that data handle by the module*/
      return 1;
    }
  
   private:
    void BackgroundProcess() {
      /*NOTE: EOS data has no invalid context,
       *    All data recevied including EOS must be forwarded.
       */
      std::vector<FrameInfoPtr> eos_datas;
      std::vector<FrameInfoPtr> datas;
      FrameInfoPtr data;
      while (running_.load()) {
        bool value = q_.wait_dequeue_timed(data, 1000 * 100);
        if (!value) continue;
  
        /*gather data*/
        if (!(data->frame.flags & cnstream::CN_FRAME_FLAG_EOS)) {
          datas.push_back(data);
        } else {
          eos_datas.push_back(data);
        }
  
        if (datas.size() == 4 || (data->frame.flags & cnstream::CN_FRAME_FLAG_EOS)) {
          /*process data...and then forward
           */
          for (auto &v : datas) {
            this->container_->ProvideData(this, v);
            std::unique_lock<std::mutex> lock(print_mutex);
            std::cout << this->GetName() << " forward: " << v->frame.stream_id << "--" << v->frame.frame_id;
            std::cout << " : " << std::this_thread::get_id() << std::endl;
          }
          datas.clear();
        }
  
        /*forward EOS*/
        for (auto &v : eos_datas) {
          this->container_->ProvideData(this, v);
          std::unique_lock<std::mutex> lock(print_mutex);
          std::cout << this->GetName() << " forward: " << v->frame.stream_id << "--EOS ";
          std::cout << " : " << std::this_thread::get_id() << std::endl;
        }
        eos_datas.clear();
      }  // while
    }
  
   private:
    moodycamel::BlockingConcurrentQueue<FrameInfoPtr> q_;
    std::vector<std::thread> threads_;
    std::atomic<int> running_{0};
  
   private:
    ExampleModuleEx(const ExampleModuleEx &) = delete;
    ExampleModuleEx &operator=(ExampleModuleEx const &) = delete;
  };