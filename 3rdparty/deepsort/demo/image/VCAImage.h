#ifndef _VCA_IMAGE_LIB_H_
#define _VCA_IMAGE_LIB_H_

#include "opencv2/opencv.hpp"
//using namespace std;
using namespace cv;

extern Scalar g_red;
extern Scalar g_cyan;
extern Scalar g_gray;
extern Scalar g_white;
extern Scalar g_black;
extern Scalar g_green;
extern Scalar g_blue;
extern Scalar g_yellow;
extern Scalar g_pinkish_red;


void ShowTagColor(Mat &input_image, Point *p_pt, const char *p_msg_buffer, Scalar o_scalar=Scalar(255,255,0), bool show_tag_bg=true);
void ShowTagRMCT(Mat &input_image, Rect o_rect, const char *p_msg_buffer=NULL, Scalar o_scalar=Scalar(255,255,0), int thickness=1, bool show_position=true, bool show_tag_bg=true, int tag_position=0);
void ShowCoordinate(Mat &input_image, Rect o_rect, Scalar o_scalar, int thickness);
void ShowTrail(Mat &input_image, Rect trail1, Rect trail2, Scalar o_scalar, int thickness);
#endif


