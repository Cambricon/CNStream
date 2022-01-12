import os, sys
import numpy as np
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
from cnstream_cpptest import *
import time
import struct
import cv2

g_cur_dir = os.path.dirname(os.path.realpath(__file__))


class TestSourceHandler():
    def test_data_source(self):
        data_source = DataSource("test_source")
        params = {"output_type" : "abc", "decoder_type" : "mlu"}
        assert False == data_source.check_param_set(params)
        params = {"output_type" : "cpu", "device_id" : "0", "decoder_type" : "mlu"}
        assert True == data_source.open(params)
        assert OutputType.output_cpu == data_source.get_source_param().output_type
        assert DecoderType.decoder_mlu == data_source.get_source_param().decoder_type

    def test_file_handler(self):
        data_source = DataSource("test_source")
        params = {"output_type" : "mlu", "device_id" : "0", "decoder_type" : "mlu"}
        assert data_source.open(params)

        cpp_test_helper = CppDataHanlderTestHelper()
        cpp_test_helper.set_observer(data_source)

        mp4_path = "../../modules/unitest/source/data/img.mp4"
        file_handler = FileHandler(data_source, "stream_id_0", mp4_path, 30)
        assert file_handler.open()
        assert "stream_id_0" == file_handler.get_stream_id()
        data = file_handler.create_frame_info()
        assert file_handler.send_data(data)
        file_handler.close()
        assert 0 == data_source.add_source(file_handler)
        assert file_handler == data_source.get_source_handler("stream_id_0")
        time.sleep(3)
        assert 7 == cpp_test_helper.get_count("stream_id_0")
        assert 0 == data_source.remove_source("stream_id_0")
        assert None == data_source.get_source_handler("stream_id_0")

    def test_rtsp_handler(self):
        data_source = DataSource("test_source")
        params = {"output_type" : "mlu", "device_id" : "0", "decoder_type" : "mlu"}
        assert data_source.open(params)

        cpp_test_helper = CppDataHanlderTestHelper()
        cpp_test_helper.set_observer(data_source)

        url = "rtsp://admin:abcd@123@192.168.80.47:554/h264/ch1/main/av_stream"
        rtsp_handler = RtspHandler(data_source, "stream_id_1", url)
        assert "stream_id_1" == rtsp_handler.get_stream_id()
        assert rtsp_handler.open()
        time.sleep(1)
        data = rtsp_handler.create_frame_info()
        assert rtsp_handler.send_data(data)
        rtsp_handler.close()
        assert 0 == data_source.add_source(rtsp_handler)
        time.sleep(1)
        assert rtsp_handler == data_source.get_source_handler("stream_id_1")
        assert 0 == data_source.remove_source("stream_id_1")
        assert None == data_source.get_source_handler("stream_id_1")

    def test_raw_img_mem_handler(self):
        data_source = DataSource("test_source")
        params = {"output_type" : "mlu", "device_id" : "0", "decoder_type" : "mlu"}
        assert data_source.open(params)

        cpp_test_helper = CppDataHanlderTestHelper()
        cpp_test_helper.set_observer(data_source)

        file_handler = RawImgMemHandler(data_source, "stream_id_0")
        assert file_handler.open()
        assert "stream_id_0" == file_handler.get_stream_id()
        assert 0 == data_source.add_source(file_handler)
        assert file_handler == data_source.get_source_handler("stream_id_0")
        # bgr24/rgb24 height = 720; width = 1280; channel = 3
        data = np.ones([720, 1280, 3], dtype=np.uint8)
        assert 0 == file_handler.write(data, 0, CNDataFormat.CN_PIXEL_FORMAT_BGR24)
        # yuv nv12/nv21 height = 720; width = 1280; channel = 1
        data_nv21 = np.ones([720 * 3 // 2, 1280], dtype=np.uint8)
        assert 0 == file_handler.write(data_nv21, 0, CNDataFormat.CN_PIXEL_FORMAT_YUV420_NV21)
        # read cv mat
        img_path = g_cur_dir + "/data/test_img_0.jpg"
        img = cv2.imread(img_path)
        assert 0 == file_handler.write(img, 0)

        assert 3 == cpp_test_helper.get_count("stream_id_0")

        file_handler.close()
        data_source.close()

    def test_es_jpeg_mem_handler(self):
        data_source = DataSource("test_source")
        params = {"output_type" : "mlu", "device_id" : "0", "decoder_type" : "mlu"}
        assert data_source.open(params)

        cpp_test_helper = CppDataHanlderTestHelper()
        cpp_test_helper.set_observer(data_source)

        handler = ESJpegMemHandler(data_source, "stream_id_0", 1280, 720)
        assert handler.open()
        assert "stream_id_0" == handler.get_stream_id()
        assert 0 == data_source.add_source(handler)
        assert handler == data_source.get_source_handler("stream_id_0")

        file_path = g_cur_dir + "/data/test_img_0.jpg"
        binfile = open(file_path, 'rb')
        file_size = os.path.getsize(file_path)
        data = binfile.read(file_size)
        rawdata = struct.unpack(file_size * 'B', data)
        assert 0 == handler.write(rawdata, file_size, 0)

        # test eos
        assert -1 == handler.write([], 0, 0, True)
        handler.close()
        data_source.close()