import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *

class TestConfig:
    def test_profiler_config(self):
        str = '{'  \
              '  "enable_profiling" : true,'  \
              '  "enable_tracing" : false,'  \
              '  "trace_event_capacity" : 100'  \
              '}'
        config = ProfilerConfig()
        assert config.parse_by_json_str(str)
        assert config.enable_profiling
        assert not config.enable_tracing
        assert 100 == config.trace_event_capacity

    def test_module_config(self):
        str = '{'  \
              '  "class_name" : "test_class_name",'  \
              '  "max_input_queue_size" : 200,'  \
              '  "parallelism" : 100,'  \
              '  "next_modules" : ["next1", "next2"],'  \
              '  "custom_params" : {'  \
              '     "param1" : "value1",'  \
              '     "param2" : "value2"'  \
              '  }'  \
              '}'
        config = CNModuleConfig()
        assert config.parse_by_json_str(str)
        assert "test_class_name" == config.class_name
        assert 100 == config.parallelism
        assert 200 == config.max_input_queue_size
        assert "next1" in config.next
        assert "next2" in config.next
        assert "value1" == config.parameters["param1"]
        assert "value2" == config.parameters["param2"]

    def test_subgraph_config(self):
        str = '{'  \
              '  "config_path" : "test_config_path",'  \
              '  "next_modules" : ["next1", "next2"]'  \
              '}'
        config = CNSubgraphConfig()
        assert config.parse_by_json_str(str)
        assert "test_config_path" == config.config_path
        assert "next1" in config.next
        assert "next2" in config.next

    def test_graph_config(self):
        str = '{'  \
              '  "profiler_config" : {'  \
              '    "enable_profiling" : true,'  \
              '    "enable_tracing" : true'  \
              '  },'  \
              '  "module1": {'  \
              '    "parallelism": 3,'  \
              '    "max_input_queue_size": 20,'  \
              '    "class_name": "test_class_name",'  \
              '    "next_modules": ["subgraph:subgraph1"],'  \
              '    "custom_params" : {'  \
              '      "param" : "value"'  \
              '    }'  \
              '  },'  \
              '  "subgraph:subgraph1" : {'  \
              '    "config_path" : "test_config_file"'  \
              '  }'  \
              '}'
        config = CNGraphConfig()
        assert config.parse_by_json_str(str)
        assert config.profiler_config.enable_profiling
        assert config.profiler_config.enable_tracing
        assert 1 == len(config.module_configs)
        assert "module1" == config.module_configs[0].name
        assert 3 == config.module_configs[0].parallelism
        assert 20 == config.module_configs[0].max_input_queue_size
        assert "test_class_name" == config.module_configs[0].class_name
        assert "subgraph:subgraph1" in config.module_configs[0].next
        assert "value" == config.module_configs[0].parameters["param"]
        assert 1 == len(config.subgraph_configs)
        assert "subgraph:subgraph1" == config.subgraph_configs[0].name
        assert "test_config_file" == config.subgraph_configs[0].config_path

    def test_get_relative_path(self):
        module_params = {"json_file_dir" : "/home/"}
        relative_path = get_path_relative_to_config_file("test.data", module_params)
        assert "/home/test.data" == relative_path

