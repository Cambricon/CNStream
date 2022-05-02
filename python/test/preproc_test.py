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
# from cnstream import *
import cnstream
import cnstream_cpptest

# in order to determine whether the python function is called by cpp
init_called = False
execute_called = False
preproc_params = None

class CustomPreproc(cnstream.Preproc):
    def __init__(self):
        cnstream.Preproc.__init__(self)

    def init(self, params):
        global init_called
        global preproc_params
        init_called = True
        preproc_params = params
        return True

    def execute(self, input_shapes, finfo):
        global execute_called
        execute_called = True
        results = []
        # the shape of return value must be equal to input_shapes
        for sp in input_shapes:
            data_count = 1
            for i in range(len(sp) - 1):
                data_count *= sp[i]
            results.append(range(data_count))
        return results

class TestPreproc(object):
    @staticmethod
    def test_init():
        params = {'pyclass_name' : 'test.preproc_test.CustomPreproc', 'param' : 'value'}
        assert cnstream_cpptest.cpptest_pypreproc(params)
        # test cpp call python init function success
        assert init_called
        # test custom parameters from cpp pass to python success
        assert preproc_params['param'] == 'value'
        # test cpp call python execute function success
        assert execute_called
