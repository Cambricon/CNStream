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
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../easydk/python/lib")

import cnis
import cnstream
import cnstream_cpptest

# in order to determine whether the python function is called by cpp
init_called = False
execute_called = False
on_tensor_params_called = False
preproc_params = None

class CustomPreproc(cnstream.Preproc):
  def __init__(self):
    cnstream.Preproc.__init__(self)

  def init(self, params):
    global init_called, preproc_params
    init_called = True
    preproc_params = params
    return 0

  def on_tensor_params(self, param):
    print("on_tensor_params")
    global on_tensor_params_called
    on_tensor_params_called = True
    return 0

  def execute(self, src, dst, src_rects):
    print("execute")
    global execute_called
    execute_called = True
    return 0

def test_preproc():
  global init_called, preproc_params, execute_called, on_tensor_params_called

  model_file = os.path.join(os.path.abspath(os.path.dirname(__file__)),
      "../../data/models/yolov5m_v0.13.0_4b_rgb_uint8.magicmind")
  if not os.path.exists(model_file):
    os.makedirs(os.path.dirname(model_file),exist_ok=True)
    import urllib.request
    url_str = "http://video.cambricon.com/models/magicmind/v0.13.0/yolov5m_v0.13.0_4b_rgb_uint8.magicmind"
    print('Downloading {} ...'.format(url_str))
    urllib.request.urlretrieve(url_str, model_file)

  params = {'pyclass_name' : 'test.preproc_test.CustomPreproc', 'param' : 'value'}
  assert cnstream_cpptest.cpptest_pypreproc(params) == 0
  # test cpp call python init function success
  assert init_called
  # test custom parameters from cpp pass to python success
  assert preproc_params['param'] == 'value'
  # test cpp call python execute function success
  assert execute_called
  assert on_tensor_params_called
