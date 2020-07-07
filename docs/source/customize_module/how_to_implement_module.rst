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
      // Your codes. 
      return true;
    }
    void Close() override { std::cout << this->GetName() << " Close called" << std::endl; }
    int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
      // Your codes.
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
      // Your codes.
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
      // Your codes.
      return true;
    }
    void Close() override {
      // Your codes.
    }
    int Process(FrameInfoPtr data) override {
      // Your codes.
      /*notify that data handle by the module*/
      return 1;
    }
  
   private:
    ExampleModuleEx(const ExampleModuleEx &) = delete;
    ExampleModuleEx &operator=(ExampleModuleEx const &) = delete;
  };
