#include <stdbool.h>
#include <stdint.h>

#include "../../camcon2/include/camcon2.h"
#include "../../vkbasic/include/vkbasic.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../../vkbasic2d/include/vkbasic2d.h"
#include "../../wlezwrap/include/mview.h"
#include "../../wlezwrap/include/wlezwrap.h"
#include "../../simpleimg/include/simpleimg.h"

typedef struct {
	Wlezwrap wew;
	Vkstatic vks;
	Vkbasic vb;
	Vkbasic2d vb2;
	Camcon2 camcon;
	WlezwrapMview wewmv;
	uint32_t iid;
	bool quit;
	bool resize;
	uint32_t width;
	uint32_t height;
	uint32_t damage[4];
} Imgview;

void imgview_init(Imgview* iv, char* path);
void imgview_deinit(Imgview* iv);
