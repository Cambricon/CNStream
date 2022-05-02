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
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../easydk/infer_server/python/lib")
import cnstream
import cnstream_cpptest
import cnis

# in order to determine whether the python function is called by cpp
init_called = False
execute_called = False
postproc_params = None
received_input_shapes = None
received_output_shape = None

class CustomVideoPostproc(cnstream.VideoPostproc):
    def __init__(self):
        cnstream.VideoPostproc.__init__(self)

    def init(self, params):
        global init_called
        global postproc_params
        init_called = True
        postproc_params = params
        return True

    def execute(self, output_data, net_outputs, model_info):
        global execute_called
        execute_called = True
        global received_input_shapes
        global received_output_shape

        input_shapes = []
        for i in range(model_info.input_num()) :
            input_shapes.append(model_info.input_shape(i).vectorize())
        received_input_shapes = input_shapes
        received_output_shape = net_outputs[0].shape

class TestVideoPostproc(object):
    @staticmethod
    def test_videopostproc():
        params = {'pyclass_name' : 'test.videopostproc_test.CustomVideoPostproc', 'param' : 'value'}
        assert cnstream_cpptest.cpptest_pyvideopostproc(params)
        # test cpp call python init function success
        assert init_called
        # test custom parameters from cpp pass to python success
        assert postproc_params['param'] == 'value'
        # test cpp call python execute function success
        assert execute_called
        # check I/O shapes
        if cnis.get_device_core_version(dev_id=0) == cnis.CoreVersion.MLU270:
          expected_input_shapes = [[4, 160, 40, 4]]
          expected_output_shape = (20, 1, 84)
        else:
          expected_input_shapes = [[4, 128, 64, 3]]
          expected_output_shape = (751,)
        assert expected_input_shapes == received_input_shapes
        assert expected_output_shape == received_output_shape
