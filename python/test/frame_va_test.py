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
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../easydk/python/lib")
import cnis
import cnstream
import cnstream_cpptest

def assert_eq(actual_val, expect_val):
  assert actual_val == expect_val, "Actual value is " + str(actual_val) + ". Expect " + str(expect_val)

def test_dataframe():
  frame = cnstream.CNFrameInfo("stream_id_0")
  frame_id = 111
  src_data_frame = cnstream.CNDataFrame()
  src_data_frame.frame_id = frame_id
  # Create a BufSurface
  params = cnis.CnedkBufSurfaceCreateParams()
  params.mem_type = cnis.CnedkBufSurfaceMemType.DEVICE
  params.width = 1280
  params.height = 720
  params.color_format = cnis.CnedkBufSurfaceColorFormat.NV12
  params.batch_size = 1
  params.device_id = 0
  params.force_align_1 = True
  mlu_buffer = cnis.cnedk_buf_surface_create(params)
  mlu_buffer_wrapper = cnis.CnedkBufSurfaceWrapper(mlu_buffer)
  src_data_frame.buf_surf = mlu_buffer_wrapper

  cnstream_cpptest.set_data_frame(frame, src_data_frame)

  data_frame = frame.get_cn_data_frame()
  # data
  src_data_y_size = params.width * params.height
  src_data_uv_size = params.width * params.height / 2
  assert_eq(data_frame.frame_id, frame_id)
  assert_eq(data_frame.buf_surf.get_plane_num(), 2)
  assert_eq(data_frame.buf_surf.get_plane_bytes(0), src_data_y_size)
  assert_eq(data_frame.buf_surf.get_plane_bytes(1), src_data_uv_size)
  assert_eq(data_frame.buf_surf.get_width(), params.width)
  assert_eq(data_frame.buf_surf.get_height(), params.height)
  assert_eq(data_frame.buf_surf.get_stride(0), params.width)
  assert_eq(data_frame.buf_surf.get_stride(1), params.width)
  assert_eq(data_frame.buf_surf.get_color_format(), params.color_format)
  assert_eq(data_frame.buf_surf.get_mem_type(), params.mem_type)
  assert_eq(data_frame.buf_surf.get_device_id(), params.device_id)

  assert_eq(data_frame.has_bgr_image(), False)
  # cv Mat
  img = data_frame.image_bgr()
  assert_eq(img.shape, (params.height, params.width, 3))
  assert_eq(data_frame.has_bgr_image(), True)
  # if modify the numpy array, the image_bgr of the data_frame will be modified
  # cv2.imwrite("./test_img.jpg", img)
  for i in range(300):
    for j in range(200):
      img[i, j, 0] = 0
      img[i, j, 1] = 0
      img[i, j, 2] = 0
  img_res = data_frame.image_bgr()
  assert img.all() == img_res.all()
  # cv2.imwrite("./test_img_res.jpg", img_res)

def test_cninfer_objects():
  frame = cnstream.CNFrameInfo("stream_id_0")

  cnstream_cpptest.set_infer_objs(frame, cnstream.CNInferObjs())

  objs_holder = frame.get_cn_infer_objects()
  # no object is in objs_holder
  assert_eq(len(objs_holder.objs), 0)

  # Add an object to objs_holder
  class_id = "1"
  track_id = "2"
  score = 0.5
  bbox = cnis.CNInferBoundingBox(0.1, 0.2, 0.5, 0.6)
  user_data = "hi cnstream"
  obj = cnstream.CNInferObject()
  obj.id = class_id
  obj.track_id = track_id
  obj.score = score
  obj.bbox = bbox
  py_collection = obj.get_py_collection()
  py_collection["user_data"] = user_data

  objs_holder.push_back(obj)

  # Check the object be added
  assert_eq(len(objs_holder.objs), 1)
  assert_eq(objs_holder.objs[0].id, class_id)
  assert_eq(objs_holder.objs[0].track_id, track_id)
  assert_eq(objs_holder.objs[0].score, score)
  assert_eq(objs_holder.objs[0].bbox.x, bbox.x)
  assert_eq(objs_holder.objs[0].bbox.y, bbox.y)
  assert_eq(objs_holder.objs[0].bbox.w, bbox.w)
  assert_eq(objs_holder.objs[0].bbox.h, bbox.h)
  assert_eq(len(objs_holder.objs[0].get_py_collection()), 1)
  assert "user_data" in objs_holder.objs[0].get_py_collection()
  assert_eq(objs_holder.objs[0].get_py_collection()["user_data"], user_data)

  # Add an attr to the object
  attr0_id = 0
  attr0_value = 5
  attr0_score = 0.8
  attr0 = cnstream.CNInferAttr(attr0_id, attr0_value, attr0_score)
  objs_holder.objs[0].add_attribute("attr0", attr0)
  attr1 = cnstream.CNInferAttr(attr0_id, attr0_value, attr0_score)
  attr1.id = 1
  attr1.value = 4
  attr1.score = 0.6
  objs_holder.objs[0].add_attribute("attr1", attr1)

  assert_eq(objs_holder.objs[0].get_attribute("attr0").id, attr0_id)
  assert_eq(objs_holder.objs[0].get_attribute("attr0").value, attr0_value)
  assert abs(objs_holder.objs[0].get_attribute("attr0").score - attr0_score) < 0.000001

  assert_eq(objs_holder.objs[0].get_attribute("attr1").id, attr1.id)
  assert_eq(objs_holder.objs[0].get_attribute("attr1").value, attr1.value)
  assert_eq(objs_holder.objs[0].get_attribute("attr1").score, attr1.score)

  # Add an extra attr to the object
  extra_attr_val = "extra_attribute"
  objs_holder.objs[0].add_extra_attribute("extra0", extra_attr_val)
  assert_eq(objs_holder.objs[0].get_extra_attribute("extra0"), extra_attr_val)

  # Add a feature to the object
  feature0 = [0.15, 0.22, 0.37]
  objs_holder.objs[0].add_feature("feat0", feature0)
  feature1 = [1, 2, 3, 4]
  objs_holder.objs[0].add_feature("feat1", feature1)
  diff = [abs(objs_holder.objs[0].get_feature("feat0")[i] - feature0[i]) for i in range(3)]
  assert diff < [0.000001, 0.000001, 0.000001], diff
  assert_eq(objs_holder.objs[0].get_feature("feat1"), feature1)
  assert_eq(len(objs_holder.objs[0].get_features()), 2)
