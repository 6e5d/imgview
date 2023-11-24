#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

typedef struct Imgview Imgview;

#include "../../vkbasic/include/vkbasic.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../../vkhelper/include/buffer.h"
#include "../../vkhelper/include/desc.h"
#include "../../vkhelper/include/pipeline.h"
#include "../../vkhelper/include/renderpass.h"
#include "../include/uniform.h"

struct Imgview {
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

	bool present;
	bool resize;
	Vkstatic vks;
	Vkbasic vb;
	uint32_t iid;
	uint32_t window_size[2];
};

void imgview_render_prepare(Imgview *iv);
void imgview_render(Imgview *iv);
void imgview_init(Imgview* iv,
	struct wl_display *display, struct wl_surface *surface,
	uint32_t img_width, uint32_t img_height);
void imgview_deinit(Imgview* iv);
void imgview_resize(Imgview *iv, struct wl_surface *surface,
	uint32_t w, uint32_t h);
