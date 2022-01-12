import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../easydk/infer_server/python/lib")
from cnstream import *
from cnstream_cpptest import *
from cnis_py import *

# in order to determine whether the python function is called by cpp
init_called = False
execute_called = False
preproc_params = None

class CustomVideoPreproc(VideoPreproc):
    def __init__(self):
        VideoPreproc.__init__(self)

    def init(self, params):
        global init_called
        global preproc_params 
        init_called = True
        preproc_params = params
        return True

    def execute(self, input_data, model_info):
        global execute_called
        execute_called = True
        results = []
        # the shape of return value must be equal to input_shapes
        
        for i in range(model_info.input_num()) :
            sp = model_info.input_shape(i)
            data_count = sp.data_count()
            print(data_count)
            results.append(range(data_count))
        return results

class TestVideoPreproc:
    def test_init(self):
        params = {'pyclass_name' : 'test.videopreproc_test.CustomVideoPreproc', 'param' : 'value'}
        assert cpptest_pyvideopreproc(params)
        # test cpp call python init function success
        assert init_called
        # test custom parameters from cpp pass to python success
        assert preproc_params['param'] == 'value'
        # test cpp call python execute function success
        assert execute_called

