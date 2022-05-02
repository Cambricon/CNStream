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

class TestCNFrameInfo(object):
    @staticmethod
    def test_frame_info():
        stream_id = "stream_id_0"
        frame = cnstream.CNFrameInfo(stream_id)
        ts = 1000
        frame.timestamp = ts

        assert not frame.is_eos()
        assert not frame.is_removed()
        assert not frame.is_invalid()

        assert stream_id == frame.stream_id
        assert ts == frame.timestamp

        # eos frame
        frame = cnstream.CNFrameInfo(stream_id, True)
        assert frame.is_eos()

    @staticmethod
    def test_py_collection():
      class UserData(object):
        @staticmethod
        def add(a ,b):
          return a + b

      # frame info created in cpp
      cpp_test_helper = cnstream_cpptest.CppCNFrameInfoTestHelper()
      frame = cpp_test_helper.get_frame_info()
      # pyCollection
      user_dict = frame.get_py_collection()
      # insert data to pyCollection
      user_dict[1] = "hi"
      user_dict["name"] = "cns"
      user_dict["user_data"] = UserData()

      # get data from pyCollection
      user_dict = frame.get_py_collection()
      assert len(frame.get_py_collection().keys()) == 3
      assert user_dict[1] == frame.get_py_collection()[1]
      assert user_dict["name"] == frame.get_py_collection()["name"]
      assert user_dict["user_data"] == frame.get_py_collection()["user_data"]
      assert 3 == frame.get_py_collection()["user_data"].add(1, 2)

      # delete a key-value from pyCollection
      del user_dict["user_data"]
      assert len(frame.get_py_collection().keys()) == 2
      assert not "user_data" in frame.get_py_collection().keys()

      # clear pyCollection
      user_dict.clear()
      assert len(frame.get_py_collection().keys()) == 0

      # add a key-value to pyCollection
      user_dict["age"] = 25
      assert "age" in frame.get_py_collection()
      assert frame.get_py_collection()["age"] == 25

      user_dict = None
      assert "age" in frame.get_py_collection()
      assert frame.get_py_collection()["age"] == 25
