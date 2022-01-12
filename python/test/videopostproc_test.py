import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../easydk/infer_server/python/lib")
from cnstream import *
from cnstream_cpptest import *
from cnis_py import *

# in order to determine whether the python function is called by cpp
init_called = False
execute_called = False
postproc_params = None
received_input_shapes = None
received_output_shape = None

class CustomVideoPostproc(VideoPostproc):
    def __init__(self):
        VideoPostproc.__init__(self)

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

class TestVideoPostproc:
    def test_videopostproc(self):
        params = {'pyclass_name' : 'test.videopostproc_test.CustomVideoPostproc', 'param' : 'value'}
        assert cpptest_pyvideopostproc(params)
        # test cpp call python init function success
        assert init_called
        # test custom parameters from cpp pass to python success
        assert postproc_params['param'] == 'value'
        # test cpp call python execute function success
        assert execute_called
        # check I/O shapes
        expected_input_shapes = [[4, 160, 40, 4]]
        expected_output_shape = (20, 1, 84)
        assert expected_input_shapes == received_input_shapes
        assert expected_output_shape == received_output_shape
