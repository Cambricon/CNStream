.. _log:

Log工具
=============

Log工具是一个C++流式日志工具，支持分级输出日志到终端或文件等功能，用户无需安装和启动该工具，通过 ``LOGx(category)`` 宏直接使用该工具。

使用说明
-------------

通过调用 ``LOGx(category)`` 宏，CNStream将日志打印在屏幕上。``x`` 代表日志等级，详情参看 日志等级_。``category`` 用于区分日志信息的类别，由用户自定义，可以是模块名称，建议名字全大写，可以使用特殊符号。

日志语句调用示例：

.. code-block:: c++

    LOGE(SOURCE) << "Error number: " << 326623;
  
示例中：

- 日志等级为 ``LOGE``，1级，即返回FATAL和ERROR级别的日志。
- category为 ``SOURCE``。
- 用户的日志信息为 ``Error number: 326623``。

返回日志信息示例如下：

::

    CNSTREAM SOURCE E0103 00:16:30.448275 30804] Error number: 326623

以上信息中：

- ``CNSTREAM`` 为关键字。
- ``SOURCE`` 代表该日志category。
- ``E`` 代表该条日志的等级。
- ``0103 00:16:30.448275`` 是日志输出的时间。
- ``30804`` 为线程ID（TID）。
- ``Error number: 326623`` 是用户的日志信息。
  
显示源码文件名和源码行号
>>>>>>>>>>>>>>>>>>>>>>>>>

如果想要显示源码文件名和源码行号字段，可在CNStream编译时，通过设置 ``-DRELEASE=OFF`` 编译开关开启DEBUG模式，返回信息示例如下：

:: 

    CNSTREAM SOURCE E0103 00:16:30.448275 30804] filename.cpp:100 error messages:326623

``filename.cpp`` 代表这条日志所在的源码文件，``100`` 表示日志所在源码文件中的行号。文件名和行号由日志语句所处实际位置决定。

条件日志
---------------
条件日志 ``LOGx_IF(category, condition)`` 表示在满足给定的 ``condition`` 条件时日志才会输出。使用时仅需在普通日志宏后缀 ``_IF`` 即可。

示例如下：

::
    
    LOGF_IF(CORE, IsFatal()) << "fatal error here, abort!!!";

该语句中：

- ``category`` 为 ``CORE``，表示该条日志的category为CORE。
- ``condition`` 为用户函数 ``IsFatal()``。当 ``IsFatal()`` 返回true时，输出日志信息“fatal error here, abort!!!”。当 ``IsFatal()`` 返回false时，不返回日志信息。

日志过滤
---------------

用户可以选择性输出不同category的日志和日志等级。使用方式如下：

::

  ./app --log_filter=category:log_level, category:log_level

或者：

::

  export CNSTREAM_log_filter=category:log_level, category:log_level
  
其中 ``category`` 用于区分日志信息的类别，``log_level`` 为日志级别。CNStream会返回小于等于该 ``log_level`` 的日志。

例如，打印category为SOURCE，日志等级不大于2的日志，以及category为CORE，日志等级不大于3的日志：

::

    ./app  --log_filter=SOURCE:2, CORE:3   

    export CNSTREAM_log_filter=SOURCE:2, CORE:3

生成日志文件
---------------
CNStream默认不保存日志文件。如果要将日志写入文件，可以通过如下方式开启:

::

    ./app --log_to_file=true  OR export  CNSTREAM_log_to_file=true

另外还需要在程序启动时调用 ``InitCNStreamLogging(const char* log_dir)`` 指定日志文件存放的路径，默认存储在 ``/tmp`` 目录下。并在结束时调用 ``ShutdownCNStreamLogging()``。日志文件名称格式为 “cnstream_年月日-时分秒.微秒.log”。

.. attention:
   | 单个日志文件最大容量为1G，最多存放10个日志文件，写满后会循环覆盖最早的日志文件。

.. _日志等级:

日志等级
---------------

Log工具基于用户设置的日志等级返回不同级别的日志信息。CNStream日志等级说明如下表所示：

.. tabularcolumns:: |m{0.1\textwidth}|m{0.1\textwidth}|m{0.6\textwidth}|
.. table:: CNStream日志等级

    +---------+--------+--------------------------------------------------------------+
    |日志等级 | 对应宏 |显示日志的级别                                                |
    +=========+========+==============================================================+
    | 0       | LOGF   |显示FATAL级别的日志。                                         |
    +---------+--------+--------------------------------------------------------------+
    | 1       | LOGE   |显示FATAL和ERROR级别的日志。                                  |
    +---------+--------+--------------------------------------------------------------+
    | 2       | LOGW   |显示FATAL、ERROR、WARNING级别的日志。                         |
    +---------+--------+--------------------------------------------------------------+
    | 3       | LOGI   |显示FATAL、ERROR、WARNING、INFO级别的日志。                   |
    +---------+--------+--------------------------------------------------------------+
    | 4       | LOGD   |显示FATAL、ERROR、WARNING、INFO、DEBUG级别的日志.             |
    +---------+--------+--------------------------------------------------------------+
    | 5       | LOGT   |显示FATAL、ERROR、WARNING、INFO、DEBUG、TRACE级别的日志。     |
    +---------+--------+--------------------------------------------------------------+
    | 6       | LOGA   |显示FATAL、ERROR、WARNING、INFO、DEBUG、TRACE、ALL级别的日志。|
    +---------+--------+--------------------------------------------------------------+
  
  
用户可以通过环境变量 ``CNSTREAM_min_log_level`` 或者命令行参数 ``min_log_level`` 调整日志输出等级。使用示例如下：

::

    ./app --min_log_level=0   or export CNSTREAM_min_log_level=0  \\显示 FATAL级别的日志
