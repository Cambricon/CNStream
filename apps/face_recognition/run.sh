#/bin/bash
#*************************************************************************#
# @param
# data_path: Video or image list path
# src_frame_rate: frame rate for send data
# wait_time: time of one test case. When set tot 0, it will automatically exit after the eos signal arrives
# rtsp = true: use rtsp
# loop = true: loop through video
# config_fname: pipeline config json file
# nms_threshold: Nonmaximal suppression threshold
# pNet_socre_threshold : mtcnn pNet face box score threshold
# rNet_socre_threshold : mtcnn rNet face box score threshold
# oNet_socre_threshold : mtcnn oNet face box score threshold
# get_faces_lib_flag = false: enable get face template mode
# person_name : face name corresponding to the face template , this parameter is use in which case get_faces_lib_flag = true
# face_lib_file : face template library json file
# face_recognize_socre_threshold : face matching threshold 
#*************************************************************************#
source env.sh

$(cd $(dirname ${BASH_SOURCE[0]});pwd)
pushd $CURRENT_DIR

if [ ! -d "$CURRENT_DIR/mtcnn_model" ]; then

  mkdir -p $CURRENT_DIR/mtcnn_model
  mtcnn_model_name_list=(det1_29x17.cambricon 
                          det1_39x22.cambricon 
                          det1_52x29.cambricon
                          det1_69x39.cambricon
                          det1_92x52.cambricon
                          det1_122x69.cambricon
                          det1_162x92.cambricon
                          det1_216x122.cambricon
                          det1_288x162.cambricon
                          det1_384x216.cambricon
                          det2_24x24.cambricon
                          det3_48x48.cambricon) 
  for model in ${mtcnn_model_name_list[*]};do
    wget -P $CURRENT_DIR/mtcnn_model http://video.cambricon.com/models/MLU100/mtcnn/$model
    echo "$model download successful!"
  done
else 
  echo "mtcnn_model already exist."
fi

if [ ! -d "$CURRENT_DIR/vggface_model" ]; then
  mkdir -p $CURRENT_DIR/vggface_model
  vggface_name=VGG_FACE.cambricon  
  wget -P $CURRENT_DIR/vggface_model http://video.cambricon.com/models/MLU100/vgg_face/$vggface_name
  echo "$vggface_name download successful!"
else 
  echo "vggface_model already exist."
fi

mkdir -p output
./../bin/face_recognition \
    --data_path ./files.list_image \
    --src_frame_rate 30  \
    --wait_time 0 \
    --rtsp=false \
    --loop=false \
    --config_fname "face_recognition.json" \
    --nms_threshold 0.5 \
    --pNet_socre_threshold 0.7 \
    --rNet_socre_threshold 0.8 \
    --oNet_socre_threshold 0.9 \
    --get_faces_lib_flag=false \
    --person_name "莫小贝" \
    --faces_lib_file_path "faces_lib.json" \
    --face_recognize_socre_threshold 0.13  \
    --alsologtostderr
