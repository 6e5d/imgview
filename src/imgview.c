#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "../../chrono/include/chrono.h"
#include "../../vkbasic/include/vkbasic.h"
#include "../../vkwayland/include/vkwayland.h"
#include "../../vkhelper/include/buffer.h"
#include "../../vkhelper/include/shader.h"
#include "../../vkhelper/include/desc.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../include/imgview.h"
#include "../include/pipeline.h"

static void imgview_try_present(Imgview *iv) {
	if (iv->present) {
		vkbasic_present(&iv->vb, iv->vks.queue, &iv->iid);
		iv->present = false;
	}
}

void imgview_resize(Imgview *iv, struct wl_surface *surface,
	uint32_t w, uint32_t h
) {
	assert(0 == vkDeviceWaitIdle(iv->vks.device));
	imgview_try_present(iv);
	iv->window_size[0] = w;
	iv->window_size[1] = h;
	vkbasic_swapchain_update(&iv->vb, &iv->vks, iv->rp, w, h);

	vec4 *mp = (vec4*)iv->uniform.proj;
	glm_mat4_identity(mp);
	mp[0][0] = 2.0f / (float)w;
	mp[1][1] = 2.0f / (float)h;

	iv->iid = 0;
	wl_surface_commit(surface);
}

static void imgview_build_command(Imgview *iv, VkCommandBuffer cbuf) {
	uint32_t width = iv->window_size[0];
	uint32_t height = iv->window_size[1];
	VkViewport viewport = {0.0f, 0.0f,
		(float)width, (float)height,
		0.0f, 1.0f};
	VkRect2D scissor = {{0.0f, 0.0f}, {width, height}};
	vkCmdUpdateBuffer(cbuf, iv->ubufg.buffer,
		0, sizeof(ImgviewUniform), &iv->uniform);
	vkCmdSetViewport(cbuf, 0, 1, &viewport);
	vkCmdSetScissor(cbuf, 0, 1, &scissor);

	VkFramebuffer fb = iv->vb.vs.elements[iv->iid].framebuffer;
	VkRenderPassBeginInfo rp_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = iv->rp,
		.framebuffer = fb,
		.renderArea.offset.x = 0,
		.renderArea.offset.y = 0,
		.renderArea.extent.width = iv->window_size[0],
		.renderArea.extent.height = iv->window_size[1],
	};
	vkCmdBeginRenderPass(cbuf, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(
		cbuf,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		iv->ppl_grid);
	vkCmdDraw(cbuf, 6, 1, 0, 0);
	vkCmdBindPipeline(
		cbuf,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		iv->ppl_view);
	vkCmdBindDescriptorSets(
		cbuf,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		iv->ppll_view,
		0, 1, &iv->desc.set, 0, NULL);
	vkCmdDraw(cbuf, 6, 1, 0, 0);
	vkCmdEndRenderPass(cbuf);
}

void imgview_render_prepare(Imgview *iv) {
	ChronoTimer timer;
	chrono_timer_reset(&timer);
	imgview_try_present(iv);
	vkbasic_next_index(&iv->vb, iv->vks.device, &iv->iid);
	VkCommandBufferBeginInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	assert(0 == vkBeginCommandBuffer(iv->vks.cbuf, &info));
}

void imgview_render(Imgview *iv) {
	imgview_build_command(iv, iv->vks.cbuf);
	assert(0 == vkEndCommandBuffer(iv->vks.cbuf));
	vkbasic_submit(&iv->vb, iv->vks.queue, iv->vks.cbuf, &iv->iid);
	iv->present = true;
}

void imgview_init(Imgview* iv,
	struct wl_display *display, struct wl_surface *surface,
	uint32_t img_width, uint32_t img_height
) {
	vkwayland_new(&iv->vks, display, surface);
	vkbasic_init(&iv->vb, iv->vks.device);
	iv->resize = true;
	iv->window_size[0] = 640;
	iv->window_size[1] = 480;
	imgview_init_render(iv, img_width, img_height);
}

void imgview_deinit(Imgview* iv) {
	VkDevice device = iv->vks.device;
	vkhelper_desc_deinit(&iv->desc, device);
	vkhelper_buffer_deinit(&iv->ubufg, device);
	vkhelper_image_deinit(&iv->img, device);
	vkDestroySampler(device, iv->sampler, NULL);
	vkDestroyRenderPass(device, iv->rp, NULL);
	vkDestroyPipeline(device, iv->ppl_grid, NULL);
	vkDestroyPipelineLayout(device, iv->ppll_grid, NULL);
	vkDestroyPipeline(device, iv->ppl_view, NULL);
	vkDestroyPipelineLayout(device, iv->ppll_view, NULL);
	vkbasic_deinit(&iv->vb, device, iv->vks.cpool);
	vkstatic_deinit(&iv->vks);
}
