#ifndef DEMO
#define DEMO

#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, int classes, int frame_skip, char *prefix, char *out_filename);
void demo1();

#ifdef __cplusplus
}
#endif

#endif
