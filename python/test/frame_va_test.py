import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
from cnstream_cpptest import *

# import cv2

def assert_eq(actual_val, expect_val):
  assert actual_val == expect_val, "Actual value is " + str(actual_val) + ". Expect " + str(expect_val)

class TestFrameVa:
    def test_dataframe(self):
      frame = CNFrameInfo("stream_id_0")
      frame_id = 111
      width = 1280
      height = 720
      stride = [1282, 1284]
      fmt = CNDataFormat.CN_PIXEL_FORMAT_YUV420_NV12
      dev_type = DevType.MLU
      dev_id = 0
      dst_device_id = 1
    
      src_data_frame = CNDataFrame()
      src_data_frame.frame_id = frame_id
      src_data_frame.width = width
      src_data_frame.height = height
      src_data_frame.stride = stride
      src_data_frame.fmt = fmt
      src_data_frame.ctx.dev_type = dev_type
      src_data_frame.ctx.dev_id = dev_id
      src_data_frame.dst_device_id = dst_device_id
    
      set_data_frame(frame, src_data_frame)
    
      data_frame = frame.get_cn_data_frame()
      # data
      src_data_y_size = stride[0] * height
      src_data_uv_size = stride[1] * height / 2
      
      assert_eq(data_frame.data(0).get_size(), src_data_y_size)
      assert_eq(data_frame.data(1).get_size(), src_data_uv_size)
    
      assert_eq(data_frame.frame_id, frame_id)
      assert_eq(data_frame.fmt, fmt)
      assert_eq(data_frame.width, width)
      assert_eq(data_frame.height, height)
    
      # stride
      assert_eq(data_frame.stride[0], stride[0])
      assert_eq(data_frame.stride[1], stride[1])
      modify_stride = [1286, 1288]
      data_frame.stride = modify_stride
      data_frame_tmp = frame.get_cn_data_frame()
      assert_eq(data_frame_tmp.stride[0], modify_stride[0])
      assert_eq(data_frame_tmp.stride[1], modify_stride[1])
      data_frame.stride = stride
    
      assert_eq(data_frame.ctx.dev_type, dev_type)
      assert_eq(data_frame.ctx.dev_id, dev_id)
      assert_eq(data_frame.dst_device_id, dst_device_id)
      modify_dst_dev_id = 4
      data_frame.dst_device_id = modify_dst_dev_id
      data_frame_tmp = frame.get_cn_data_frame()
      assert_eq(data_frame_tmp.dst_device_id, modify_dst_dev_id)
    
      # functions
      assert_eq(data_frame.get_planes(), 2)
      assert_eq(data_frame.get_plane_bytes(0), src_data_y_size)
      assert_eq(data_frame.get_plane_bytes(1), src_data_uv_size)
      assert_eq(data_frame.get_bytes(), src_data_y_size + src_data_uv_size)
      assert_eq(data_frame.has_bgr_image(), False)
      # cv Mat
      img = data_frame.image_bgr()
      assert_eq(img.shape, (height, width, 3))
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
    
    def test_cninfer_objects(self):
      frame = CNFrameInfo("stream_id_0")
    
      set_infer_objs(frame, CNInferObjs())
    
      objs_holder = frame.get_cn_infer_objects()
      # no object is in objs_holder
      assert_eq(len(objs_holder.objs), 0)
    
      # Add an object to objs_holder
      class_id = "1"
      track_id = "2"
      score = 0.5
      bbox = CNInferBoundingBox(0.1, 0.2, 0.5, 0.6)
      user_data = "hi cnstream"
    
      obj = CNInferObject()
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
      attr0 = CNInferAttr(attr0_id, attr0_value, attr0_score)
      objs_holder.objs[0].add_attribute("attr0", attr0)
    
      attr1 = CNInferAttr(attr0_id, attr0_value, attr0_score)
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
