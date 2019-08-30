#ifndef _DEEP_SORT_H_
#define _DEEP_SORT_H_

#include <vector>
//#define FEATURE_MATCH_EN
//#ifdef __cplusplus
//extern "C" {
//#endif

typedef struct
{
	int x;
	int y;
	int width;
	int height;
}DS_Rect;

typedef struct
{
	int class_id;
	DS_Rect rect;
	float confidence;
        std::vector<float> feature;
}DS_DetectObject;

typedef struct
{
	int track_id;
	int class_id;
	float confidence;
	DS_Rect rect;
}DS_TrackObject;


typedef void * DS_Tracker;
typedef std::vector<DS_DetectObject> DS_DetectObjects;
typedef std::vector<DS_TrackObject> DS_TrackObjects;


DS_Tracker DS_Create(
	float max_cosine_distance=0.2, 
	int nn_budget=100, 
    float max_iou_distance = 0.7, 
    int max_age = 30, 
	int n_init=3);
bool DS_Delete(DS_Tracker h_tracker);
bool DS_Update(
	DS_Tracker h_tracker, 
	DS_DetectObjects detect_objects, 
	DS_TrackObjects &track_objects);


//#ifdef __cplusplus
//}
//#endif

#endif


