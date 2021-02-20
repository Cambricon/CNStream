
.. _单进程单Pipeline:

单进程单Pipeline中使用多个设备
---------------------------------

在单进程、单个pipeline场景下，CNStream支持不同模块在不同的MLU卡上运行。用户可以通过设置模块的 ``device_id`` 参数指定使用的MLU卡。

下面以Decode和Inference模块使用场景为例，配置Decode模块使用MLU卡0，Inference模块使用MLU卡1。单进程中建议在source module中复用codec的buffer，即应设置 ``reuse_codec_buf`` 为 ``true``。

::

  {
    "source" : {
      "class_name" : "cnstream::DataSource",	// 数据源类名。
      "parallelism" : 0,			// 并行度。无效值，设置为0即可。
      "next_modules" : ["ipc"],		        // 下一个连接模块名称。
      "custom_params" : {			// 特有参数。
        "source_type" : "ffmpeg",	        // source类型。
        "reuse_cndec_buf" : "true",	        // 是否复用codec的buffer。
        "output_type" : "mlu",		        // 输出类型，可以设置为MLU或CPU。
        "decoder_type" : "mlu",		        // decoder类型，可以设置为MLU或CPU。
        "device_id" : 0			        // 设备id，用于标识多卡机器的设备唯一标号。
      }
    },

     "infer" : {				
      "class_name" : "cnstream::Inferencer",	// 推理类名。
      "parallelism" : 1,			// 并行度。
      "max_input_queue_size" : 20,		// 最大队列深度。
      "custom_params" : {			// 特有参数。
        //模型路径。
        "model_path" : "../../data/models/MLU270/Classification/resnet50/resnet50_offline.cambricon",
        "func_name" : "subnet0",		     // 模型函数名。
        "postproc_name" : "PostprocClassification",  // 后处理类名。
        "batching_timeout" : 60,		     // 攒batch的超时时间。
        "device_id" : 1				     // 设备id，用于标识多卡机器的设备唯一标号。
  }
    }
  }  

