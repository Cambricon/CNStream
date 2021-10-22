import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *

# for test stream msg observer keep alive with pipeline
smo_del_called = False

class CustomSMO(StreamMsgObserver):
    def __init__(self):
        StreamMsgObserver.__init__(self)

    def __del__(self):
        global smo_del_called
        smo_del_called = True

    def update(self, msg):
        pass

def CreateConfigWithSourceModule():
        config = CNGraphConfig()
        config.name = "test_pipeline"
        config.profiler_config.enable_profiling = True
        mconfig = CNModuleConfig()
        mconfig.name = "test_module"
        mconfig.class_name = "cnstream::DataSource"
        config.module_configs = [mconfig]
        return config


class TestPipeline:
    def test_name(self):
        pipeline = Pipeline("test_pipeline")
        assert "test_pipeline" == pipeline.get_name()

    def test_build_pipeline(self):
        # TODO: wait for config py api merge
        pipeline = Pipeline("test_pipeline")
        config = CreateConfigWithSourceModule()
        assert pipeline.build_pipeline(config)
        source_module = pipeline.get_source_module("test_module")

    def test_start(self):
        pipeline = Pipeline("test_pipeline")
        assert pipeline.start()
        assert pipeline.is_running()

    def test_get_source_module(self):
        pipeline = Pipeline("test_pipeline")
        config = CreateConfigWithSourceModule()
        assert pipeline.build_pipeline(config)
        assert None != pipeline.get_source_module("test_module")
        assert None == pipeline.get_source_module("test_none")
        

    def test_get_module_config(self):
        pipeline = Pipeline("test_pipeline")
        config = CreateConfigWithSourceModule()
        assert pipeline.build_pipeline(config)
        mconfig = pipeline.get_module_config("test_module")
        assert "cnstream::DataSource" == mconfig.class_name

    def test_is_profiling_enabled(self):
        pipeline = Pipeline("test_pipeline")
        config = CNGraphConfig()
        config.profiler_config.enable_profiling = True
        assert pipeline.build_pipeline(config)
        assert pipeline.is_profiling_enabled()

    def test_is_tracing_enabled(self):
        pipeline = Pipeline("test_pipeline")
        config = CNGraphConfig()
        config.profiler_config.enable_tracing = True
        assert pipeline.build_pipeline(config)
        assert pipeline.is_tracing_enabled()

    def test_provide_data(self):
        pipeline = Pipeline("test_pipeline")
        config = CreateConfigWithSourceModule()
        assert pipeline.build_pipeline(config)
        assert pipeline.start()
        source_module = pipeline.get_source_module("test_module")
        assert pipeline.provide_data(source_module, CNFrameInfo("test_stream"))
        pipeline.stop()

    def test_stream_msg_observer(self):
        smo = CustomSMO()
        pipeline = Pipeline("test_pipeline")
        pipeline.stream_msg_observer = smo
        # test smo keep alive with pipeline
        smo = None
        global smo_del_called
        assert not smo_del_called
        # TODO: test stream message update

    def test_is_root_node(self):
        pipeline = Pipeline("test_pipeline")
        config = CreateConfigWithSourceModule()
        assert pipeline.build_pipeline(config)
        assert pipeline.is_root_node("test_module")
        assert not pipeline.is_root_node("test_not")

    def test_register_frame_done_cb(self):
        # TODO: wait for data handler api
        pass

