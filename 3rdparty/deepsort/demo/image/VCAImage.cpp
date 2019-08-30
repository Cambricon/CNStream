#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctime>
#include <math.h> 
#include <stdio.h>
#include "opencv2/highgui/highgui.hpp"

using namespace std;
using namespace cv;

#include "VCAImage.h"
#define VCA_SPRINTF_S snprintf
Scalar g_red(0, 0, 255);
Scalar g_cyan(255, 255, 0);
Scalar g_gray(200, 200, 200);
Scalar g_white(255, 255, 255);
Scalar g_black(0, 0, 0);
Scalar g_green(0, 255, 0);
Scalar g_yellow(0, 255, 255);
Scalar g_pinkish_red(255, 0, 255);
Scalar g_blue(255, 0, 0);


void ShowTagColor(Mat &input_image, Point *p_pt, const char *p_msg_buffer, Scalar color, bool show_tag_bg)
{
	Rect msg_rect;

	msg_rect.width=(int)((float)strlen(p_msg_buffer)*(float)5.2)+3;
	msg_rect.height=13;
	msg_rect.x=p_pt->x;
	msg_rect.y=p_pt->y;
	if(msg_rect.x<0)
	{
		msg_rect.x=0;
	}
	if(msg_rect.y<0)
	{
		msg_rect.y=0;
	}
	if((msg_rect.x+msg_rect.width)>input_image.cols)
	{
		msg_rect.x=input_image.cols-msg_rect.width;
	}
	if((msg_rect.y+msg_rect.height)>input_image.rows)
	{
		msg_rect.y=input_image.rows-msg_rect.height;
	}
	if(show_tag_bg)
	{
		rectangle(input_image,msg_rect,color,-1, CV_AA);
		putText(input_image, p_msg_buffer, cvPoint(msg_rect.x,msg_rect.y+9),CV_FONT_HERSHEY_SIMPLEX,(double)0.3,Scalar(0,0,0),1,CV_AA);
	}
	else
	{
		putText(input_image, p_msg_buffer, cvPoint(msg_rect.x,msg_rect.y+9),CV_FONT_HERSHEY_SIMPLEX,(double)0.3,color,1,CV_AA);
	}
}


void ShowTagRMCT(Mat &input_image, Rect o_rect, const char *p_msg_buffer, Scalar o_scalar, int thickness, bool show_position, bool show_tag_bg, int tag_position)
{
	char msg_buffer[256];
	Point tag_pt;
	int offset_length=0;

	memset(msg_buffer, 0, sizeof(msg_buffer));
	if(p_msg_buffer)
	{
		offset_length+=VCA_SPRINTF_S(msg_buffer, sizeof(msg_buffer)-offset_length, "%s", p_msg_buffer);
	}
	if(show_position)
	{
		offset_length+=VCA_SPRINTF_S(&msg_buffer[offset_length], sizeof(msg_buffer)-offset_length, "%s(%u,%u,%u,%u)", p_msg_buffer, o_rect.x, o_rect.y, o_rect.width, o_rect.height);
	}
	if(offset_length>0)
	{
		offset_length+=VCA_SPRINTF_S(&msg_buffer[offset_length], sizeof(msg_buffer)-offset_length, "  ");
	}

	tag_pt.x=o_rect.x;
	if(tag_position==0)
	{
		tag_pt.y=o_rect.y-13;
	}
	else if(tag_position==1)
	{
		tag_pt.y=o_rect.y;
	}
	else if(tag_position==2)
	{
		tag_pt.y=o_rect.y+o_rect.height-13;
	}
	else if(tag_position==3)
	{
		tag_pt.y=o_rect.y+o_rect.height;
	}
	rectangle(input_image, o_rect, o_scalar, thickness, CV_AA);
	//ShowCoordinate(input_image, o_rect, o_scalar, thickness);
	ShowTagColor(input_image, &tag_pt, msg_buffer, o_scalar, show_tag_bg);
}

#define ShowLine_LEFT 1
#define ShowLine_RIGHT 2
#define ShowLine_UP 4
#define ShowLine_DOWN 8
void ShowLine(Mat &input_image, Point pt1, int len, int direction, Scalar o_scalar, int thickness)
{
	Point pt2;
	if(direction&ShowLine_LEFT)
	{
		pt2.x=pt1.x-len;
		pt2.y=pt1.y;
		line(input_image, pt1, pt2, o_scalar, thickness, CV_AA);
	}
	if(direction&ShowLine_RIGHT)
	{
		pt2.x=pt1.x+len;
		pt2.y=pt1.y;
		line(input_image, pt1, pt2, o_scalar, thickness, CV_AA);
	}
	if(direction&ShowLine_UP)
	{
		pt2.x=pt1.x;
		pt2.y=pt1.y-len;
		line(input_image, pt1, pt2, o_scalar, thickness, CV_AA);
	}
	if(direction&ShowLine_DOWN)
	{
		pt2.x=pt1.x;
		pt2.y=pt1.y+len;
		line(input_image, pt1, pt2, o_scalar, thickness, CV_AA);
	}
}

void ShowCoordinate(Mat &input_image, Rect o_rect, Scalar o_scalar, int thickness)
{
	int line_len=thickness*10;
	int temp_len;
	temp_len=o_rect.width/4;
	if(line_len>temp_len)
	{
		line_len=temp_len;
	}
	temp_len=o_rect.height/4;
	if(line_len>temp_len)
	{
		line_len=temp_len;
	}
	ShowLine(input_image, 
		Point(o_rect.x, o_rect.y), line_len, 
		ShowLine_RIGHT|ShowLine_DOWN, o_scalar, thickness);
	ShowLine(input_image, 
		Point(o_rect.x+o_rect.width, o_rect.y), line_len, 
		ShowLine_LEFT|ShowLine_DOWN, o_scalar, thickness);
	ShowLine(input_image, 
		Point(o_rect.x+o_rect.width, o_rect.y+o_rect.height), line_len, 
		ShowLine_LEFT|ShowLine_UP, o_scalar, thickness);
	ShowLine(input_image, 
		Point(o_rect.x, o_rect.y+o_rect.height), line_len, 
		ShowLine_RIGHT|ShowLine_UP, o_scalar, thickness);
}

#define RP1(rect) Point(rect.x,rect.y)
#define RP2(rect) Point(rect.x+rect.width,rect.y)
#define RP3(rect) Point(rect.x+rect.width,rect.y+rect.height)
#define RP4(rect) Point(rect.x,rect.y+rect.height)
void ShowTrail(Mat &input_image, Rect trail1, Rect trail2, Scalar o_scalar, int thickness)
{
	line(input_image, RP1(trail1), RP1(trail2), o_scalar, thickness, CV_AA);
	line(input_image, RP2(trail1), RP2(trail2), o_scalar, thickness, CV_AA);
	line(input_image, RP3(trail1), RP3(trail2), o_scalar, thickness, CV_AA);
	line(input_image, RP4(trail1), RP4(trail2), o_scalar, thickness, CV_AA);
}



