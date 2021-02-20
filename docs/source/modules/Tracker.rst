
追踪模块
---------------

追踪模块（Tracker）用于对检测到的物体进行追踪。主要应用于车辆行人等检测物的追踪。目前支持FeatureMatch和KCF两种追踪方法。该模块连接在神经网络推理模块后，通过在配置文件中指定追踪使用的离线模型以及使用的追踪方法来配置模块。

使用说明
^^^^^^^^^

配置追踪模块所需要的离线模型和追踪方法等。

   ::
 
     “tracker” : {
     “class_name” : “cnstream::Tracker”,       // （必设参数）Track的类名。
     “parallelism” : 4,                        // （必设参数）并行度。
     “max_input_queue_size” : 20,              // （必设参数）数据输入队列长度。
     “next_modules” : [“osd”],                 // （必设参数）下一个连接的模块名。
     “custom_params” : {
         // （必设参数）追踪使用的离线模型的路径。该参数支持绝对路径和相对路径。相对路径是相对于JSON配置文件的路径。
         “model_path” : “xxx.cambricon”,  
         “func_name” : “subnet0”,    // 如果设置将使用MLU，如果不设置将使用CPU。模型函数名。
         “track_name” : “KCF”       // （可选参数）追踪方法。支持FeatureMatch和KCF两种追踪方法。
         }
     }
