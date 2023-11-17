#include <stdbool.h>
#include <stdint.h>
#include <cglm/cglm.h>

#include "../../camcon2/include/camcon2.h"
#include "../../vkbasic/include/vkbasic.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../../vkbasic2d/include/vkbasic2d.h"
#include "../../wlezwrap/include/mview.h"
#include "../../wlezwrap/include/wlezwrap.h"
#include "../../simpleimg/include/simpleimg.h"

typedef struct Imgview Imgview;

struct Imgview {
	Wlezwrap wew;
	Vkstatic vks;
	Vkbasic vb;
	Vkbasic2d vb2;
	Camcon2 camcon;
	WlezwrapMview wewmv;
	uint32_t iid;
	bool quit;
	uint32_t width;
	uint32_t height;
	uint32_t damage[4];
	bool resize;
	bool dirty; // damage/resize must be dirty
	void (*event)(Imgview* iv, uint8_t type, WlezwrapEvent *event);
	void *data;
};

void imgview_s2w(Imgview *iv, vec2 s, vec2 w);
void imgview_damage_all(Imgview *iv);
void imgview_render(Imgview *iv);
void imgview_init(Imgview* iv, uint32_t width, uint32_t height);
void imgview_deinit(Imgview* iv);
