#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../ltree/include/ltree.h"
#include "../../ppath/include/ppath.h"
#include "../../wholefile/include/wholefile.h"
#include "../include/lyc.h"

size_t imgview_lyc_load(ImgviewLyc **lycp, char* path) {
	uint8_t *buf = NULL;
	wholefile_read(path, &buf);
	char *p = (char*)buf;
	LtreeValue v;
	assert(ltree_parse(&p, &v) == 0);
	free(buf);

	assert(v.ty == 5);
	size_t llen = v.data.vlist.len;
	*lycp = malloc(sizeof(ImgviewLyc) * llen);
	ImgviewLyc *vdlyc = *lycp;
	printf("read %zu layers\n", llen);
	for (size_t i = 0; i < llen; i += 1) {
		LtreeValue *elem = &v.data.vlist.elements[i];
		assert(elem->ty == 6);
		assert(elem->data.vdict.len == 8);
		LtreeValue *elem2 = elem->data.vdict.elements;
		assert(elem2[0].ty == 4);
		assert(0 == strcmp(elem2[0].data.vstring, "lid"));
		assert(elem2[2].ty == 4);
		assert(0 == strcmp(elem2[2].data.vstring, "offset_x"));
		assert(elem2[4].ty == 4);
		assert(0 == strcmp(elem2[4].data.vstring, "offset_y"));
		assert(elem2[6].ty == 4);
		assert(0 == strcmp(elem2[6].data.vstring, "path"));
		assert(elem2[1].ty == 0);
		vdlyc[i].lid = (int32_t)elem2[1].data.vint;
		assert(elem2[3].ty == 0);
		vdlyc[i].offset[0] = (int32_t)elem2[3].data.vint;
		assert(elem2[5].ty == 0);
		vdlyc[i].offset[1] = (int32_t)elem2[5].data.vint;
		assert(elem2[7].ty == 4);
		char *pabs = ppath_abs(path);
		char *result = ppath_rel_new(pabs, "..");
		char *result2 = ppath_rel_new(result, elem2[7].data.vstring);
		printf("loading layer image: %s\n", result2);
		simpleimg_load(&vdlyc[i].img, result2);
		free(pabs);
		free(result);
		free(result2);
	}
	return llen;
}
