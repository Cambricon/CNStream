import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
from cnstream_cpptest import *

class CustomSourceModule(SourceModule):
    def __init__(self, name):
        SourceModule.__init__(self, name)

    def open(self, params):
        return True

    def close(self):
        return 

class CustomSourceHandler(SourceHandler):
    def __init__(self, source_module, name):
        SourceHandler.__init__(self, source_module, name)
    
    def open(self):
        return True

    def close(self):
        print("handler close")
        return


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
