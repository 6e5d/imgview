#ifndef INCLUDEGUARD_IMGVIEW
#define INCLUDEGUARD_IMGVIEW

#include <vulkan/vulkan.h>
#include <wayland-client.h>

typedef struct Imgview Imgview;

#include "../../dmgrect/include/dmgrect.h"
#include "../../vkbasic/include/vkbasic.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../../vkhelper2/include/vkhelper2.h"
#include "../include/uniform.h"

struct Imgview {
	ImgviewUniform uniform;
	Vkhelper2Buffer ubufg;
	Vkhelper2Image img;
	Vkhelper2Desc desc;
	VkSampler sampler;

	VkRenderPass rp;
	VkPipeline ppl_view;
	VkPipelineLayout ppll_view;
	VkPipeline ppl_grid;
	VkPipelineLayout ppll_grid;

	bool present;
	bool resize;
	Vkstatic vks;
	Vkbasic vb;
	uint32_t iid;
	uint32_t window_size[2];
};

void imgview_render_prepare(Imgview *iv);
void imgview_render(Imgview *iv, Vkhelper2Image *image);
void imgview_init(Imgview* iv,
struct wl_display *display, struct wl_surface *surface, Dmgrect *rect);
void imgview_deinit(Imgview* iv);
void imgview_resize(Imgview *iv, struct wl_surface *surface,
	uint32_t w, uint32_t h);

#endif
