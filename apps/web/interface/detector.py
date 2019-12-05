from ctypes import *
lib = cdll.LoadLibrary("./../../lib/libpydetector.so")

class Detector:
    def __init__(self):
        self.detector = lib.Detector_new()
    def build_pipeline_by_JSONFile(self, config_path):
        config_path = create_string_buffer(config_path.encode("utf-8"))
        lib.Detector_buildPipelineByJSONFile(self.detector, byref(config_path))
    def detect_image(self, image_path):
        image_path = create_string_buffer(image_path.encode("utf-8"))
        return lib.Detector_addImageSource(self.detector, byref(image_path))
    def wait_for_stop(self):
        lib.Detector_waitForStop(self.detector)

