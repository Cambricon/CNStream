# ==============================================================================
# Copyright (C) [2022] by Cambricon, Inc. All rights reserved
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# ==============================================================================

import os, sys
import numpy as np
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../easydk/python/lib")
import time
import struct
import cv2

import cnis
import cnstream
import cnstream_cpptest

g_cur_dir = os.path.dirname(os.path.realpath(__file__))


def test_data_source():
  data_source = cnstream.DataSource("test_source")
  params = {"interval" : "wrong_type", "bufpool_size" : "wrong_type"}
  assert False == data_source.check_param_set(params)
  interval = 2
  device_id = 0
  bufpool_size = 16
  params = {"interval" : str(interval), "device_id" : str(device_id), "bufpool_size" : str(bufpool_size)}
  assert True == data_source.open(params)
  assert interval == data_source.get_source_param().interval
  assert bufpool_size == data_source.get_source_param().bufpool_size
  assert device_id == data_source.get_source_param().device_id


def test_file_handler():
  data_source = cnstream.DataSource("test_source")
  params = {"interval" : "1", "device_id" : "0", "bufpool_size" : "16"}
  assert data_source.open(params)

  cpp_test_helper = cnstream_cpptest.CppDataHanlderTestHelper()
  cpp_test_helper.set_observer(data_source)

  stream_id = "stream_id_0"
  mp4_path = g_cur_dir + "/../../modules/unitest/data/img.mp4"
  param = cnstream.FileSourceParam()
  param.filename = mp4_path
  param.framerate = 30
  file_handler = cnstream.create_source(data_source, stream_id, param)
  assert file_handler.open()
  assert stream_id == file_handler.get_stream_id()
  data = file_handler.create_frame_info()
  assert file_handler.send_data(data)
  file_handler.close()
  assert 0 == data_source.add_source(file_handler)
  assert file_handler == data_source.get_source_handler(stream_id)
  time.sleep(1)
  assert 7 == cpp_test_helper.get_count(stream_id)
  assert 0 == data_source.remove_source(stream_id)
  assert None == data_source.get_source_handler(stream_id)


def test_rtsp_handler():
  data_source = cnstream.DataSource("test_source")
  params = {"interval" : "1", "device_id" : "0", "bufpool_size" : "16"}
  assert data_source.open(params)

  cpp_test_helper = cnstream_cpptest.CppDataHanlderTestHelper()
  cpp_test_helper.set_observer(data_source)

  stream_id = "stream_id_1"
  url = "rtsp://admin:hello123@10.100.202.30:554/cam/realmonitor?channel=1&subtype=0"
  param = cnstream.RtspSourceParam()
  param.url_name = url
  rtsp_handler = cnstream.create_source(data_source, stream_id, param)
  assert rtsp_handler.open()
  assert stream_id == rtsp_handler.get_stream_id()
  time.sleep(1)
  data = rtsp_handler.create_frame_info()
  assert rtsp_handler.send_data(data)
  rtsp_handler.close()

  assert 0 == data_source.add_source(rtsp_handler)
  time.sleep(1)
  assert rtsp_handler == data_source.get_source_handler(stream_id)
  assert 0 == data_source.remove_source(stream_id)
  rtsp_handler.stop()
  rtsp_handler.close()
  assert None == data_source.get_source_handler(stream_id)


def test_image_frame_handler():
  data_source = cnstream.DataSource("test_source")
  params = {"interval" : "1", "device_id" : "0", "bufpool_size" : "16"}
  assert data_source.open(params)

  cpp_test_helper = cnstream_cpptest.CppDataHanlderTestHelper()
  cpp_test_helper.set_observer(data_source)

  stream_id = "stream_id_0"
  param = cnstream.ImageFrameSourceParam()
  frame_handler = cnstream.create_source(data_source, stream_id, param)
  assert frame_handler.open()
  assert stream_id == frame_handler.get_stream_id()
  assert 0 == data_source.add_source(frame_handler)
  assert frame_handler == data_source.get_source_handler(stream_id)
  # bgr24/rgb24 height = 720; width = 1280; channel = 3
  data_bgr = np.ones([720, 1280, 3], dtype=np.uint8)
  buf_bgr = cnis.convert_to_buf_surf(data_bgr)
  pts = 0
  buf_bgr.set_pts(pts)
  assert 0 == cnstream.write_image_frame(frame_handler, buf_bgr)

  # yuv nv12/nv21 height = 720; width = 1280; channel = 1
  data_nv12 = np.ones([720 * 3 // 2, 1280], dtype=np.uint8)
  buf_nv12 = cnis.convert_to_buf_surf(data_nv12, cnis.CnedkBufSurfaceColorFormat.NV12)
  pts = pts + 1
  buf_nv12.set_pts(pts)
  assert 0 == cnstream.write_image_frame(frame_handler, buf_nv12)

  # test eos
  assert 0 == cnstream.write_image_frame(frame_handler, None)

  frame_handler.close()
  data_source.close()

  assert 3 == cpp_test_helper.get_count(stream_id)


def test_es_jpeg_mem_handler():
  data_source = cnstream.DataSource("test_source")
  params = {"interval" : "1", "device_id" : "0", "bufpool_size" : "16"}
  assert data_source.open(params)

  cpp_test_helper = cnstream_cpptest.CppDataHanlderTestHelper()
  cpp_test_helper.set_observer(data_source)

  stream_id = "stream_id_0"
  param = cnstream.ESJpegMemSourceParam()
  res = cnstream.Resolution(width = 1920, height = 1080)
  param.max_res = res
  param.out_res = res
  es_jpeg_handler = cnstream.create_source(data_source, stream_id, param)

  assert es_jpeg_handler.open()
  assert stream_id == es_jpeg_handler.get_stream_id()
  assert 0 == data_source.add_source(es_jpeg_handler)
  assert es_jpeg_handler == data_source.get_source_handler(stream_id)

  file_path = g_cur_dir + "/data/test_img_0.jpg"
  binfile = open(file_path, 'rb')
  file_size = os.path.getsize(file_path)
  data = binfile.read(file_size)
  rawdata = struct.unpack(file_size * 'B', data)
  pts = 0
  assert 0 == cnstream.write_jpeg_package(es_jpeg_handler, rawdata, file_size, pts)

  # test eos
  pts = 1
  assert 0 == cnstream.write_jpeg_package(es_jpeg_handler, (), 0, pts)

  es_jpeg_handler.close()
  data_source.close()
  assert 2 == cpp_test_helper.get_count(stream_id)


def test_es_mem_handler():
  data_source = cnstream.DataSource("test_source")
  params = {"interval" : "1", "device_id" : "0", "bufpool_size" : "16"}
  assert data_source.open(params)

  cpp_test_helper = cnstream_cpptest.CppDataHanlderTestHelper()
  cpp_test_helper.set_observer(data_source)

  stream_id = "stream_id_0"
  param = cnstream.ESMemSourceParam()
  param.data_type = cnstream.ESMemSourceParamDataType.H264
  res = cnstream.Resolution(width = 1920, height = 1080)
  param.max_res = res
  param.out_res = res
  es_mem_handler = cnstream.create_source(data_source, stream_id, param)

  assert stream_id == es_mem_handler.get_stream_id()
  assert 0 == data_source.add_source(es_mem_handler)
  assert es_mem_handler == data_source.get_source_handler(stream_id)

  file_path = g_cur_dir + "/../../modules/unitest/data/img.h264"
  binfile = open(file_path, 'rb')
  file_size = os.path.getsize(file_path)
  data = binfile.read(file_size)
  rawdata = struct.unpack(file_size * 'B', data)
  pts = 0
  assert 0 == cnstream.write_mem_package(es_mem_handler, rawdata, file_size, pts)
  time.sleep(1)

  # test eos
  # es_mem_handler.open()
  # pts = 0
  # assert 0 == cnstream.write_mem_package(es_mem_handler, (), 0, pts, True)

  es_mem_handler.close()
  data_source.close()
  assert 5 == cpp_test_helper.get_count(stream_id)

