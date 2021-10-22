import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
from cnstream_cpptest import *

# in order to determine whether the python function is called by cpp
open_param = ""
process_called = False
close_called = False
on_eos_called = False

class CustomModule(Module):
    def __init__(self, name):
        Module.__init__(self, name)

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

class CustomModuleEx(ModuleEx):
    def __init__(self, name):
        ModuleEx.__init__(self, name)

class TestModule:
    def test_transmit_permissions(self):
        module = CustomModule("test_module")
        assert not module.has_transmit()
        module = CustomModuleEx("test_module")
        assert module.has_transmit()

    def test_get_name(self):
        module = CustomModule("test_module")
        assert "test_module" == module.get_name()
        module = CustomModuleEx("test_module")
        assert "test_module" == module.get_name()

    def test_pymodule(self):
        global open_param
        global process_called
        global close_called
        global on_eos_called
        open_param = ""
        process_called = False
        close_called = False
        on_eos_called = False
        params = {"pyclass_name" : "test.module_test.CustomModule", "param" : "value"}
        assert cpptest_pymodule(params)
        assert "value" == open_param
        assert process_called
        assert close_called
        assert on_eos_called

