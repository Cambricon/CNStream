.. _log

Log工具
=============
该Log工具是一个C++流式日志工具，支持分级分模块输出日志到终端或文件等功能，用户无需安装和启动。通过LOGx(category)宏使用该工具，
其中x代表日志级别大写首字母，在“日志等级”章节具体介绍。

日志语句调用示例：

.. code-block:: c++

    LOGE(SOURCE) << "Error number: " << 326623;
  
日志格式为CNSTREAM category level(首字母F, E, W...) time  TID] msg，以上语句将输出：

::

    CNSTREAM SOURCE E0103 00:16:30.448275 30804] Error number: 326623

以上信息中，CNSTREAM为关键字，SOURCE代表该日志来自Source模块，E代表该条日志的等级为Error, 0103 00:16:30.448275是日志输出的时间，
30804为线程ID(TID)，“Error number: 326623”是用户的日志信息。
  
若CNStream编译时通过-DRELEASE=OFF编译开关开启DEBUG模式，日志会增加源码文件和源码行号字段：

:: 

    CNSTREAM SOURCE E0103 00:16:30.448275 30804] filename.cpp:100 error messages:326623

filename.cpp代表这条日志所在的源码文件，100表示日志所在源码文件中的行号。文件名和行号由日志语句所处实际位置决定，此处仅是示例。

日志等级
---------------
日志等级共分为FATAL、ERROR、WARNING、INFO、DEBUG、TRACE，ALL七级，对应LOGF、LOGE、
LOGW、LOGI、LOGD、LOGT、LOGA 七个宏，默认输出等级为INFO。 
用户可以通过环境变量CNSTREAM_min_log_level或者命令行参数min_log_level调整日志输出等级。如：

::

    ./app --min_log_level=4   or export CNSTREAM_min_log_level=4

设置后只会输出FATAL、ERROR、WARNING、INFO、DEBUG等级别的日志。

条件日志
---------------
条件日志LOGx_IF(category, condition)表示在满足condition条件时日志才会输出，使用时仅需在普通日志宏后缀_IF即可，有category和condition两个参数。

示例如下：

::
    
    LOGF_IF(CORE, IsFatal()) << "fatal error here, abort!!!";

该语句中category为CORE，表示该条日志来自core模块，condition为IsFatal()，当IsFatal()返回true时，
输出日志信息“fatal error here, abort!!!”。

日志过滤
---------------
用户可以选择性输出模块日志，使用方式如下：

::

    ./app  --log_filter=SOURCE:2, CORE:3   OR  export CNSTREAM_log_filter=SOURCE:2, CORE:3

设置后只会打印SOURCE模块等级不大于2(WARNING)和CORE模块等级不大于3(INFO)的日志。


日志文件
---------------
CNStream默认不存日志文件。如果要将日志写入文件，可以通过如下方式开启:

::

    ./app --log_to_file=true  OR export  CNSTREAM_log_to_file=true

另外还需要在程序启动时调用 InitCNStreamLogging(const char* log_dir)，并在结束时调用ShutdownCNStreamLogging()，
参数log_dir为日志存放的路径，默认存储在/tmp目录下。

注：单个日志文件最大容量为1G，最多存放10个日志文件，写满后会循环覆盖最早的日志文件。