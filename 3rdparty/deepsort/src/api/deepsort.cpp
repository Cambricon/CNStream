#include <iostream>
#include <sstream>
#include <stdio.h>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include "tracker.h"
#include "deepsort.h"

DS_Tracker DS_Create(
	float max_cosine_distance,
	int nn_budget,
    float max_iou_distance,
    int max_age,
	int n_init)
{
    return (DS_Tracker)(new tracker(max_cosine_distance, nn_budget, max_iou_distance, max_age, n_init));
}

bool DS_Delete(DS_Tracker h_tracker)
{
    delete((tracker *)h_tracker);
    return true;
}
#if 0
bool DS_Update(
    DS_Tracker h_tracker,
	DS_DetectObject *p_detects,
	int detect_num,
	DS_TrackObject *p_tracks,
	int *p_tracks_num,
	int max_tracks_num)
{
    tracker *p_tracker=(tracker *)h_tracker;
    DETECTION_ROW temp_object;
    DETECTIONS detections;
    for(int iloop=0;iloop<detect_num;iloop++)
    {
        temp_object.confidence=p_detects[iloop].confidence;
        temp_object.tlwh = DETECTBOX(p_detects[iloop].x, p_detects[iloop].y, p_detects[iloop].width, p_detects[iloop].height);
#ifdef FEATURE_MATCH_EN
        temp_object.feature.setZero();
#endif
        detections.push_back(temp_object);
    }
    p_tracker->predict();
	p_tracker->update(detections);
    DETECTBOX output_box;
    int output_num=0;
    for(Track& track : p_tracker->tracks)
    {
        if(!track.is_confirmed() || track.time_since_update > 1) continue;
        output_box=track.to_tlwh();

        p_tracks[output_num].track_id=track.track_id;
        p_tracks[output_num].x=output_box(0);
        p_tracks[output_num].y=output_box(1);
        p_tracks[output_num].width=output_box(2);
        p_tracks[output_num].height=output_box(3);
        output_num++;
        if(output_num>=max_tracks_num)
        {
            break;
        }
    }
    *p_tracks_num=output_num;
    return true;
}
#endif

bool DS_Update(
	DS_Tracker h_tracker,
	DS_DetectObjects detect_objects,
	DS_TrackObjects &track_objects)
{
    tracker *p_tracker=(tracker *)h_tracker;
    DETECTION_ROW temp_object;
    DETECTIONS detections;
    for(uint32_t iloop=0;iloop<detect_objects.size();iloop++)
    {
        temp_object.class_id=detect_objects[iloop].class_id;
        temp_object.confidence=detect_objects[iloop].confidence;
        temp_object.tlwh = DETECTBOX(
            detect_objects[iloop].rect.x,
            detect_objects[iloop].rect.y,
            detect_objects[iloop].rect.width,
            detect_objects[iloop].rect.height);
#ifdef FEATURE_MATCH_EN
        std::vector<float> temp_feature;
        temp_feature = detect_objects[iloop].feature;
        for (size_t i = 0; i < temp_feature.size(); i ++) {
          temp_object.feature[i] = temp_feature[i];
        }
        //std::cout << "temp_object.feature" << std::endl;
        //std::cout << temp_object.feature << std::endl;
#endif
        detections.push_back(temp_object);
    }
    p_tracker->predict();
	p_tracker->update(detections);
    DETECTBOX output_box;
    DS_TrackObject track_object;
    track_objects.clear();
    for(Track& track : p_tracker->tracks)
    {
        if(!track.is_confirmed() || track.time_since_update > 1) continue;
        output_box=track.to_tlwh();

        track_object.track_id=track.track_id;
        track_object.class_id=track.class_id;
        track_object.confidence=track.confidence;
        track_object.rect.x=output_box(0);
        track_object.rect.y=output_box(1);
        track_object.rect.width=output_box(2);
        track_object.rect.height=output_box(3);
        track_objects.push_back(track_object);
    }
    return true;
}
