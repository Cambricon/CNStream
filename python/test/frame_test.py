import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
from cnstream_cpptest import *

class TestCNFrameInfo():
    def test_frame_info(self):
        stream_id = "stream_id_0"
        frame = CNFrameInfo(stream_id)
        ts = 1000
        frame.timestamp = ts

        assert not frame.is_eos()
        assert not frame.is_removed()
        assert not frame.is_invalid()

        assert stream_id == frame.stream_id
        assert ts == frame.timestamp

        # eos frame
        frame = CNFrameInfo(stream_id, True)
        assert frame.is_eos()

    def test_py_collection(self):
      class userData:
        def add(self, a ,b):
          return a + b
    
      # frame info created in cpp
      cpp_test_helper = CppCNFrameInfoTestHelper()
      frame = cpp_test_helper.get_frame_info()
      # pyCollection
      user_dict = frame.get_py_collection()
      # insert data to pyCollection
      user_dict[1] = "hi"
      user_dict["name"] = "cns"
      user_dict["user_data"] = userData()
    
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
    
