
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include "deepsort.h"
#include "VCATime.h"
#include "VCAImage.h"
#include <gflags/gflags.h>
using namespace std;

static vector<pair<int, DS_DetectObjects>>frames;
static int frame_index=0;
static Scalar array_color[8]=
{
g_red,
g_cyan,
g_gray,
g_white,
g_green,
g_blue,
g_yellow,
g_pinkish_red
};

bool LoadDetection(const char *p_file_name)
{
	printf("Load detection label: %s\n", p_file_name);
	FILE *fp=NULL;
	fp=fopen(p_file_name, "rb");
	if(NULL == fp)
	{
		printf("Label file not exist!");
		return false;
	}
	int current_frame_index=1;
	char line_buffer[1024];
	DS_DetectObject temp_object;
	DS_DetectObjects detections;
	detections.clear();
	frames.clear();
	frame_index=0;
	int read_frame_index, reserved;
	float read_rect[4], read_confidence;
	while(1)
	{
		if(NULL!=fgets(line_buffer, sizeof(line_buffer), fp))
		{
			sscanf(line_buffer, "%d,%d,%f,%f,%f,%f,%f",
				&read_frame_index,
				&reserved,
				&read_rect[0],
				&read_rect[1],
				&read_rect[2],
				&read_rect[3],
				&read_confidence
				);
			if(current_frame_index!=read_frame_index)
			{
				frames.push_back(make_pair(current_frame_index, detections));
				detections.clear();
				current_frame_index=read_frame_index;
			}
			temp_object.class_id=0;
			temp_object.confidence=read_confidence/100;
			temp_object.rect.x=read_rect[0];
			temp_object.rect.y=read_rect[1];
			temp_object.rect.width=read_rect[2];
			temp_object.rect.height=read_rect[3];
			detections.push_back(temp_object);
		}
		else
		{
			frames.push_back(make_pair(current_frame_index, detections));
			detections.clear();
			current_frame_index=0;
			break;
		}
	}
	fclose(fp);
	return true;
}

bool ReadFrame(int &read_frame_index, DS_DetectObjects &detections)
{
	if(frame_index>=static_cast<int>(frames.size()))
	{
		return false;
	}
	read_frame_index=frames[frame_index].first;
	detections=frames[frame_index].second;
	frame_index++;
	return true;
}

DEFINE_bool(show, false, "Show images");
DEFINE_string(path, "../../data/2DMOT2015/test/PETS09-S2L2", "Detect data path");

#define TO_CVRect(ds_rect) Rect(ds_rect.x, ds_rect.y, ds_rect.width, ds_rect.height)
int main(int argc, char* argv[])
{
	gflags::SetVersionString("1.0.0");
	gflags::SetUsageMessage("Usage : ./demo ");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	DS_Tracker h_tracker=DS_Create();
	if(NULL==h_tracker)
	{
		printf("DS_CreateTracker error.\n");
		return 0;
	}
	int frame_count=0;
	CTickTime tick_time;

	char label_file_name[1024];
	sprintf(label_file_name, "%s/det/det.txt", FLAGS_path.c_str());
	if(!LoadDetection(label_file_name))
	{
		return 0;
	}
	Mat show_image;
	DS_DetectObjects detect_objects;
	DS_TrackObjects track_objects;
	int read_frame_index;
	char image_file_name[1024];
	tick_time.Start();
        int _n = 3;
	while(_n--)
	{
                ReadFrame(read_frame_index, detect_objects);
		DS_Update(h_tracker, detect_objects, track_objects);
		printf("\n%-10s%-10s%-10s%s\n", "track_id", "class_id", "confidence", "position");
		printf("-------------------------------\n");
		for(auto oloop : track_objects)
		{
			printf("%-10d%-10d%-10f%d,%d,%d,%d\n", oloop.track_id, oloop.class_id, oloop.confidence,
				oloop.rect.x,oloop.rect.y,oloop.rect.width,oloop.rect.height);
		}

		if(FLAGS_show)
		{
			sprintf(image_file_name, "%s/img1/%06d.jpg", FLAGS_path.c_str(), read_frame_index);
			show_image=imread(image_file_name);
			if(show_image.empty())
			{
				sprintf(image_file_name, "%s/det/000001-acf.jpg", FLAGS_path.c_str());
				show_image=imread(image_file_name);
				if(show_image.empty())
				{
					printf("Error.imread(%s)\n", image_file_name);
					return 0;
				}
				show_image.setTo(g_black);
			}
			for(auto oloop : track_objects)
			{
				char caption[32];
				sprintf(caption, "%d", oloop.track_id);
				ShowTagRMCT(show_image, TO_CVRect(oloop.rect), caption, array_color[oloop.track_id%8], 1, false);
			}

			static bool pause=false;
			imshow("DeepSort", show_image);
			int input_key=cvWaitKey(pause? 0:40);
			if(input_key==32)
			{
				pause=!pause;
			}
			else if(input_key==27)
			{
				break;
			}
		}
		frame_count++;
	}

	printf("Frame count: %d\n", frame_count);
	printf("Going time: %f\n", tick_time.GoingTime());
	printf("Speed: %f\n", frame_count*1000.0/tick_time.GoingTime());
	printf("\n");
        printf("Press any key to exit.\n");
        getchar();
	DS_Delete(h_tracker);
	return 0;
}
