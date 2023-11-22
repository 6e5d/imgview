#pragma once

#include <stdint.h>

#include "../../ltree/include/ltree.h"
#include "../../simpleimg/include/simpleimg.h"
#include "../../wholefile/include/wholefile.h"

// LaYerCollection
typedef struct {
	int32_t lid;
	int32_t offset[2];
	Simpleimg img;
} ImgviewLyc;

size_t imgview_lyc_load(ImgviewLyc **lycp, char* path);
void imgview_lyc_deinit(ImgviewLyc *lycp);
