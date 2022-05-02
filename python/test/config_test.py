# ==============================================================================
# Copyright (C) [2022] by Cambricon, Inc. All rights reserved
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# ==============================================================================

import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
import cnstream

class TestConfig(object):
    @staticmethod
    def test_profiler_config():
        json_str = '{'  \
              '  "enable_profiling" : true,'  \
              '  "enable_tracing" : false,'  \
              '  "trace_event_capacity" : 100'  \
              '}'
        config = cnstream.ProfilerConfig()
        assert config.parse_by_json_str(json_str)
        assert config.enable_profiling
        assert not config.enable_tracing
        assert 100 == config.trace_event_capacity

    @staticmethod
    def test_module_config():
        json_str = '{'  \
              '  "class_name" : "test_class_name",'  \
              '  "max_input_queue_size" : 200,'  \
              '  "parallelism" : 100,'  \
              '  "next_modules" : ["next1", "next2"],'  \
              '  "custom_params" : {'  \
              '     "param1" : "value1",'  \
              '     "param2" : "value2"'  \
              '  }'  \
              '}'
        config = cnstream.CNModuleConfig()
        assert config.parse_by_json_str(json_str)
        assert "test_class_name" == config.class_name
        assert 100 == config.parallelism
        assert 200 == config.max_input_queue_size
        assert "next1" in config.next
        assert "next2" in config.next
        assert "value1" == config.parameters["param1"]
        assert "value2" == config.parameters["param2"]

    @staticmethod
    def test_subgraph_config():
        json_str = '{'  \
              '  "config_path" : "test_config_path",'  \
              '  "next_modules" : ["next1", "next2"]'  \
              '}'
        config = cnstream.CNSubgraphConfig()
        assert config.parse_by_json_str(json_str)
        assert "test_config_path" == config.config_path
        assert "next1" in config.next
        assert "next2" in config.next

    @staticmethod
    def test_graph_config():
        json_str = '{'  \
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
        config = cnstream.CNGraphConfig()
        assert config.parse_by_json_str(json_str)
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

    @staticmethod
    def test_get_relative_path():
        module_params = {"json_file_dir" : "/home/"}
        relative_path = cnstream.get_path_relative_to_config_file("test.data", module_params)
        assert "/home/test.data" == relative_path
