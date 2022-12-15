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
import cnstream_cpptest

# in order to determine whether the python function is called by cpp
open_param = ""
process_called = False
close_called = False
on_eos_called = False

class CustomModule(cnstream.Module):
  def __init__(self, name):
    cnstream.Module.__init__(self, name)

  def open(self, params):
    global open_param
    open_param = params["param"]
    return True

  def close(self):
    global close_called
    close_called = True

  def process(self, data):
    global process_called
    process_called = True
    return 1

  def on_eos(self, stream_id):
    global on_eos_called
    on_eos_called = True

class CustomModuleEx(cnstream.ModuleEx):
  def __init__(self, name):
    cnstream.ModuleEx.__init__(self, name)

def test_transmit_permissions():
  module = CustomModule("test_module")
  assert not module.has_transmit()
  module = CustomModuleEx("test_module")
  assert module.has_transmit()

def test_get_name():
  module = CustomModule("test_module")
  assert "test_module" == module.get_name()
  module = CustomModuleEx("test_module")
  assert "test_module" == module.get_name()

def test_pymodule():
  global open_param
  global process_called
  global close_called
  global on_eos_called
  open_param = ""
  process_called = False
  close_called = False
  on_eos_called = False
  params = {"pyclass_name" : "test.module_test.CustomModule", "param" : "value"}
  assert cnstream_cpptest.cpptest_pymodule(params)
  assert "value" == open_param
  assert process_called
  assert close_called
  assert on_eos_called
