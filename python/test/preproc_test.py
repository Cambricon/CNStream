import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
from cnstream_cpptest import *

# in order to determine whether the python function is called by cpp
init_called = False
execute_called = False
preproc_params = None

class CustomPreproc(Preproc):
    def __init__(self):
        Preproc.__init__(self)

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

class TestPreproc:
    def test_init(self):
        params = {'pyclass_name' : 'test.preproc_test.CustomPreproc', 'param' : 'value'}
        assert cpptest_pypreproc(params)
        # test cpp call python init function success
        assert init_called
        # test custom parameters from cpp pass to python success
        assert preproc_params['param'] == 'value'
        # test cpp call python execute function success
        assert execute_called

