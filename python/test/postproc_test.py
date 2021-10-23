import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
from cnstream_cpptest import *

# in order to determine whether the python function is called by cpp
init_called = False
execute_called = False
postproc_params = None
obj_init_called = False
obj_execute_called = False
obj_postproc_params = None
received_input_shapes = None
received_output_shape = None

class CustomPostproc(Postproc):
    def __init__(self):
        Postproc.__init__(self)

    def init(self, params):
        global init_called
        global postproc_params 
        init_called = True
        postproc_params = params
        return True

    def execute(self, net_outputs, input_shapes, finfo):
        global execute_called
        execute_called = True
        global received_input_shapes
        global received_output_shape
        received_input_shapes = input_shapes
        received_output_shape = net_outputs[0].shape

class CustomObjPostproc(ObjPostproc):
    def __init__(self):
        ObjPostproc.__init__(self)

    def init(self, params):
        global obj_init_called
        global obj_postproc_params 
        obj_init_called = True
        obj_postproc_params = params
        return True

    def execute(self, net_outputs, input_shapes, finfo, obj):
        global obj_execute_called
        obj_execute_called = True
        global received_input_shapes
        global received_output_shape
        received_input_shapes = input_shapes
        received_output_shape = net_outputs[0].shape

class TestPostproc:
    def test_postproc(self):
        params = {'pyclass_name' : 'test.postproc_test.CustomPostproc', 'param' : 'value'}
        assert cpptest_pypostproc(params)
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

    def test_obj_postproc(self):
        params = {'pyclass_name' : 'test.postproc_test.CustomObjPostproc', 'param' : 'value'}
        assert cpptest_pyobjpostproc(params)
        # test cpp call python init function success
        assert obj_init_called
        # test custom parameters from cpp pass to python success
        assert obj_postproc_params['param'] == 'value'
        # test cpp call python execute function success
        assert obj_execute_called
        # check I/O shapes
        expected_input_shapes = [[4, 160, 40, 4]]
        expected_output_shape = (20, 1, 84)
        assert expected_input_shapes == received_input_shapes
        assert expected_output_shape == received_output_shape

