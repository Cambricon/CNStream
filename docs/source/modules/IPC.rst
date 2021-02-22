
.. _多进程:

多进程操作
---------------

由于pipeline只能进行单进程操作，用户可以通过ModuleIPC模块将pipeline拆分成多个进程，并完成进程间数据传输和通信，例如最常见的解码和推理进程分离等。ModuleIPC模块继承自CNStream中的Module类。两个ModuleIPC模块组成一个完整的进程间通信。此外，通过定义模块的 ``memmap_type`` 参数，可以选择进程间的内存共享方式。

CNStream支持在单个pipeline中，不同的进程使用不同的MLU卡执行任务。用户可以通过设置模块的 ``device_id`` 参数指定使用的MLU设备。

使用示例
^^^^^^^^^

下面以进程1做解码，进程2做推理为例，展示了如何使用ModuleIPC模块完成多进程设置和通信，以及设置各进程使用不同的MLU卡。

1. 创建配置文件，如 ``config_process1.json``。在配置文件中设置进程1。第一个模块配置为解码模块，然后设置一个ModuleIPC模块。主要参数设置如下：
   
   - 设置 ``ipc_type`` 参数值为 **client**，做为多进程通信的客户端。
   - 设置 ``memmap_type`` 参数值为 **cpu**。当前仅支持CPU内存共享方式。后续会支持MLU内存共享方式。
   - 设置 ``socket_address`` 参数值为进程间通信地址。用户需定义一个字符串来表示通信地址。
   - 设置不同进程使用不同的MLU卡：设置Decode进程使用MLU卡0。但配置ModuleIPC模块时，无需设置 ``device_id``。另外，多进程使用中，不建议在source module中复用codec的buffer，即应设置 ``reuse_codec_buf`` 设为false。
   
   示例如下：

   ::
   
     {
       "source" : {
         "class_name" : "cnstream::DataSource",	 // 数据源类名。
         "parallelism" : 0,			 // 并行度。无效值，设置为0即可。
         "next_modules" : ["ipc"],		 // 下一个连接模块的名称。
         "custom_params" : {			 // 特有参数设置。
           "source_type" : "ffmpeg",		 // source类型。
           "reuse_cndec_buf" : "false",		 // 是否复用codec的buffer。
           "output_type" : "mlu",	         // 输出类型，可以设置为MLU或CPU。
           "decoder_type" : "mlu",		 // decoder类型，可以设置为MLU或CPU。
           "device_id" :0			 // 设备id，用于标识多卡机器的设备唯一标号。
         }
       },
     
       "ipc" : {
         "class_name" : "cnstream::ModuleIPC",	// 进程间通信类名。
         "parallelism" :1 ,		        // 并行度，针对client端，设置为1。
         "max_input_queue_size" : 20,		// 最大队列深度。
         "custom_params" : {			// 特有参数设置。
           "ipc_type" : "client",		// 进程间通信类型，可设为client和server。上游进程设置为client，下游进程设置为server。
           "memmap_type" : "cpu",		// 进程间内存共享类型，可以设置为CPU。
           "max_cachedframe_size" : "40",       // 最大缓存已处理帧队列深度，仅client端有该参数。
           "socket_address" : "test_ipc"        // 进程间通信地址，一对通信的进程，需要设置为相同的通信地址。
         }
       }
     }
     

2. 创建配置文件，如 ``config_process2.json``。在配置文件中设置进程2。第一个模块配置为ModuleIPC模块，然后设置一个推理模块。主要参数设置如下：
   
   - 在ModuleIPC模块中，设置 ``ipc_type`` 参数值为 **server**，做为多进程通信的服务器端。
   - 在ModuleIPC模块中，设置 ``memmap_type`` 参数值为 **cpu**。当前仅支持CPU内存共享方式。后续会支持MLU内存共享方式。
   - 在ModuleIPC模块中，设置 ``socket_address`` 参数值为进程间通信地址。用户需定义一个字符串来表示通信地址。
   - 设置不同进程使用不同的MLU卡：设置Inference进程使用MLU卡1。但配置ModuleIPC模块时，需要指定 ``device_id``。该 ``device_id`` 的值应与推理模块设置的 ``device_id`` 的值保持一致。
   
   .. attention::
       | ``memmap_type`` 与 ``socket_address`` 的参数值设置需要与进程1中ModuleIPC模块的相关参数设置保持一致。

   ::

     {
       "ipc" : {
         "class_name" : "cnstream::ModuleIPC",	// 进程间通信类名。
         "parallelism" : 0,			// 并行度，无效值，针对server端，设置为0即可。
         "next_modules" : ["infer"],		// 下一个连接模块名称。
         "custom_params" : {			// 特有参数设置。
           "ipc_type" : "server",		// 进程间通信类型，可设为client和server。上游进程设置为client，下游进程设置为server。
           "memmap_type" : "cpu",		// 进程间内存共享类型，可以设置为CPU。
           "socket_address" : "test_ipc",       // 进程间通信地址，一对通信的进程，需要设置为相同的通信地址。
           "device_id":1                        // 设备id，用于标识多卡机器的设备唯一标号。 
         }
       },
     
       "infer" : {				
         "class_name" : "cnstream::Inferencer",	  // 推理类名。
         "parallelism" : 1,			  // 并行度。
         "max_input_queue_size" : 20,		  // 最大队列深度。
         "custom_params" : {			  // 特有参数设置。
           "model_path" : "../../data/models/MLU270/Classification/resnet50/resnet50_offline.cambricon",   // 模型路径。
           "func_name" : "subnet0",		          // 模型函数名。
           "postproc_name" : "PostprocClassification",	  // 后处理类名。
           "batching_timeout" : 60,			  // 攒batch的超时时间。
           "device_id" : 1				  // 设备id，用于标识多卡机器的设备唯一标号。
         }
       }
     }


