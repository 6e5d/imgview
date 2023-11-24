#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <cglm/cglm.h>
#include <vulkan/vulkan.h>

#include "../../camcon2/include/camcon2.h"
#include "../../vkbasic/include/vkbasic.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../../vkhelper/include/buffer.h"
#include "../../vkhelper/include/desc.h"
#include "../../vkhelper/include/pipeline.h"
#include "../../vkhelper/include/renderpass.h"
#include "../../wlezwrap/include/mview.h"
#include "../../wlezwrap/include/wlezwrap.h"
#include "../include/uniform.h"

typedef struct Imgview Imgview;

struct Imgview {
	Wlezwrap wew;
	WlezwrapMview wewmv;
	Vkstatic vks;
	Vkbasic vb;
	uint32_t iid;
	uint32_t window_size[2];

	bool quit;
	bool resize;
	// todo: damage imageview(long term)
	bool dirty; // for simple sleep based fps lock
	void (*event)(Imgview* iv, uint8_t type, WlezwrapEvent *event);
	void *data;

	Camcon2 camcon;
	ImgviewUniform uniform;
	VkhelperBuffer ubufg;
	VkhelperImage img;
	VkhelperDesc desc;
	VkSampler sampler;

	VkRenderPass rp;
	VkPipeline ppl_view;
	VkPipelineLayout ppll_view;
	VkPipeline ppl_grid;
	VkPipelineLayout ppll_grid;
};

void imgview_s2w(Imgview *iv, vec2 s, vec2 w);
void imgview_render_prepare(Imgview *iv);
void imgview_render(Imgview *iv);
void imgview_init(Imgview* iv, uint32_t img_width, uint32_t img_height);
void imgview_deinit(Imgview* iv);
