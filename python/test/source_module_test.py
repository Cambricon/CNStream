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

class CustomSourceModule(cnstream.SourceModule):
    def __init__(self, name):
        cnstream.SourceModule.__init__(self, name)

    def open(self, params):
        return True

    def close(self):
        return

class CustomSourceHandler(cnstream.SourceHandler):
    def __init__(self, source_module, name):
        cnstream.SourceHandler.__init__(self, source_module, name)
    def open(self):
        return True

    def close(self):
        print("handler close")

def test_source():
    source = CustomSourceModule("source_test")
    handler0 = CustomSourceHandler(source, "stream_id_0")
    handler1 = CustomSourceHandler(source, "stream_id_1")
    handler2 = CustomSourceHandler(source, "stream_id_2")
    assert handler0.open()
    assert "stream_id_0" == handler0.get_stream_id()
    data = handler0.create_frame_info()
    assert handler0.send_data(data)

    assert 0 == source.add_source(handler0)
    assert 0 == source.add_source(handler1)
    assert 0 == source.add_source(handler2)
    assert handler0 == source.get_source_handler("stream_id_0")
    assert 0 == source.remove_source("stream_id_0")
    assert None == source.get_source_handler("stream_id_0")
    assert handler1 == source.get_source_handler("stream_id_1")
    assert 0 == source.remove_source(handler1)
    assert None == source.get_source_handler("stream_id_1")
    assert handler2 == source.get_source_handler("stream_id_2")
    assert 0 == source.remove_sources()
    assert None == source.get_source_handler("stream_id_2")
