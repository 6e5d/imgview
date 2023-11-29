#ifndef INCLUDEGUARD_IMGVIEW_UNIFORMH
#define INCLUDEGUARD_IMGVIEW_UNIFORMH

#include <cglm/cglm.h>

typedef struct {
	mat4 model; // use fixed rectangle [0, 1]
	mat4 proj; // window
	mat4 view; // camera
} ImgviewUniform;

#endif
