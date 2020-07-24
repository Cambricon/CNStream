.. _inspect:

Inspect工具
=============

Inspect工具是CNStream提供的一个用来扫描模块以及检查配置文件的工具。主要功能包括：

- 查看框架支持的所有模块。
- 查看某个模块在使用时需要用到的参数。
- 检查配置文件的合法性。
- 打印CNStream的版本信息。

如果使用自定义模块，用户需要先注册自定义的模块，才能使用该工具。详情请参照 `配置Inspect工具`_ 。

工具命令的使用
---------------

:ref:`install` 后，输入下面命令进入工具所在目录：

::

  cd $CNSTREAM_HOME/tools/bin

``bin`` 目录是编译成功后创建的。

打印工具帮助信息
>>>>>>>>>>>>>>>>>>

输入下面命令打印工具帮助信息：

::

  ./cnstream_inspect -h

命令返回如下内容：

::

  Usage:
      inspect-tool [OPTION...] [MODULE-NAME]
  Options:
      -h, --help                            Show usage
      -a, --all                             Print all modules
      -m, --module-name                     List the module parameters
      -c, --check                           Check the config file
      -v, --version                         Print version information

查看框架支持的所有模块
>>>>>>>>>>>>>>>>>>>>>>>>>

输入下面命令查看框架支持的所有模块：

::

  ./cnstream_inspect -a

命令返回示例如下：

::

  Module Name                             Description
    cnstream::DataSource                    DataSource is a module for handling input data.
    ...                                     ...

查看某个模块的参数
>>>>>>>>>>>>>>>>>>>>>

输入下面命令查看某个模块的参数，以DataSource为例：

::

  ./cnstream_inspect -m DataSource

命令返回示例如下：

::

  DataSource Details:
  Common Parameter              Description
  class_name                    Module class name.
  ...                           ...

检查配置文件的合法性
>>>>>>>>>>>>>>>>>>>>>

输入下面命令检查配置文件合法性：

::

  ./cnstream_inspect -c $CNSTREAM_HOME/samples/demo/detection_config.json

配置文件的检查包括模块、模块参数以及模块前后的连接。如果检查没有错误，则会显示如下信息：

::

  Check module config file successfully!

否则，请根据提示信息修改配置文件。

例如，配置文件中模块名字写错，将DataSource写成DataSourc：

::

  {
    "source" : {
      "class_name" : "cnstream::DataSourc",
      ...
    },
  }

命令返回示例如下：

::

  Check module configuration failed, Module name : [source] class_name : [cnstream::DataSourc] non-existent.

打印CNStream的版本信息
>>>>>>>>>>>>>>>>>>>>>>>>>

输入下面命令打印CNStream的版本信息：

::

  ./cnstream_inspect -v

命令返回示例如下，版本号为CNStream最新版本号：

::

  CNStream: v4.0.0

配置Inspect工具
----------------

执行下面步骤完成自定义模块工具的配置。CNStream内置模块无需配置，直接调用工具相关指令即可。

1. 每个自定义的模块在声明时，需要继承 **Module** 和 **ModuleCreator** 类，以Encoder模块为例：

   ::

     class Encoder: public Module, public ModuleCreator<Encoder> {
      ...
     }

2. 添加自定义模块的描述信息。param_register_是ParamRegister类型的 **Module** 类的成员变量，以Encoder模块为例。

   ::

     param_register_.SetModuleDesc("Encoder is a module for encode the video or image.");

3. 注册自定义模块所支持的参数。param_register_是ParamRegister类型的 **Module** 类的成员变量，以Encoder模块为例。

   ::

     param_register_.Register("param_name", "param description");


4. 声明  **ParamRegister** 类。

   ::

     class ParamRegister {
      private:
       std::vector<std::pair<std::string /*key*/, std::string /*desc*/>> module_params_;
       std::string module_desc_;
      public:
       void Register(const std::string &key, const std::string &desc); // 注册函数。
       // 通过该接口获取子模块已注册的参数。
       std::vector<std::pair<std::string, std::string>> GetParams();
       // 判断key是否是已注册的。也可以判断配置文件中是否配置了module不支持的参数。 	   
       bool IsRegisted(const std::string& key);
       void SetModuleDesc(const std::string& desc); // 设置模块描述。
     };

5. 为了检查配置文件中参数的合法性，还需要实现父类 **cnstream::Module** 的 ``CheckParamSet`` 函数。

   ::

     virtual bool CheckParamSet(ModuleParamSet paramSet) { return true; }

   例如：

   ::

     bool Inferencer::CheckParamSet(ModuleParamSet paramSet) {
       ParametersChecker checker;
    
       // 对配置文件中的配置项判断是否是已注册的，如不是，给出WARNING信息。
       for (auto& it : paramSet) {
         if (!param_register_.IsRegisted(it.first)) {
           LOG(WARNING) << "[Inferencer] Unknown param: " << it.first;
         }
       }

       // 对一些必要参数进行检查配置文件是否配置。
       if (paramSet.find("model_path") == paramSet.end()
           || paramSet.find("func_name") == paramSet.end()
           || paramSet.find("postproc_name") == paramSet.end()) {
         LOG(ERROR) << "Inferencer must specify [model_path], [func_name], [postproc_name].";
         return false;
       }
    
       // 检查模块路径是否存在。
       if (!checker.CheckPath(paramSet["model_path"], paramSet)) {
         LOG(ERROR) << "[Inferencer] [model_path] : " << paramSet["model_path"] << " non-existence.";
         return false;
       }
    
       // 检查batching_timeout和device_id是否设为数字。
       std::string err_msg;
       if (!checker.IsNum({"batching_timeout", "device_id"}, paramSet, err_msg)) {
         LOG(ERROR) << "[Inferencer] " << err_msg;
         return false;
       }

       return true;
     }
