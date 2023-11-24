#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>
#include <cglm/cglm.h>

#include "../../camcon2/include/camcon2.h"
#include "../../vkbasic/include/vkbasic.h"
#include "../../vkwayland/include/vkwayland.h"
#include "../../vkhelper/include/barrier.h"
#include "../../vkhelper/include/buffer.h"
#include "../../vkhelper/include/shader.h"
#include "../../vkhelper/include/sampler.h"
#include "../../vkhelper/include/desc.h"
#include "../../vkstatic/include/oneshot.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../../wlezwrap/include/mview.h"
#include "../../wlezwrap/include/wlezwrap.h"
#include "../../ppath/include/ppath.h"
#include "../include/imgview.h"

static double angle_norm(double angle) {
	while (angle >= M_PI) {
		angle -= 2.0 * M_PI;
	}
	while (angle < M_PI) {
		angle += 2.0 * M_PI;
	}
	return angle;
}

static void write_cam(Imgview *iv) {
	camcon2_build(&iv->camcon, iv->uniform.view);
}

void imgview_s2w(Imgview *iv, vec2 s, vec2 w) {
	vec4 ss = {
		s[0] - (float)iv->window_size[0] * 0.5f,
		s[1] - (float)iv->window_size[1] * 0.5f,
		0.0f, 1.0f};
	vec4 ww;
	mat4 t;
	camcon2_build(&iv->camcon, t);
	glm_mat4_inv(t, t);
	glm_mat4_mulv(t, ss, ww);
	w[0] = ww[0] / ww[3]; w[1] = ww[1] / ww[3];
}

static void handle_resize(Imgview *iv) {
	assert(0 == vkDeviceWaitIdle(iv->vks.device));
	vkbasic_swapchain_update(&iv->vb, &iv->vks, iv->rp,
		iv->window_size[0], iv->window_size[1]);
	iv->iid = 0;
	wl_surface_commit(iv->wew.wl.surface);
}

static void imgview_build_command(Imgview *iv, VkCommandBuffer cbuf) {
	uint32_t width = iv->window_size[0];
	uint32_t height = iv->window_size[1];
	VkViewport viewport = {0.0f, 0.0f,
		(float)width, (float)height,
		0.0f, 1.0f};
	VkRect2D scissor = {{0.0f, 0.0f}, {width, height}};
	// TODO: viewport-dirty(do not copy uniform every frame)
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
	wl_display_roundtrip(iv->wew.wl.display);
	wl_display_dispatch_pending(iv->wew.wl.display);
	if (iv->resize) {
		handle_resize(iv);
		iv->resize = false;
	}
	vkbasic_next_index(&iv->vb, iv->vks.device, &iv->iid);
	// TODO: handle the many dirty systematically
	// including view dirty(update imgview only)
	// layer dirty and edit dirty
	write_cam(iv);
	VkCommandBufferBeginInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	assert(0 == vkBeginCommandBuffer(iv->vks.cbuf, &info));
}

void imgview_render(Imgview *iv) {
	imgview_build_command(iv, iv->vks.cbuf);
	assert(0 == vkEndCommandBuffer(iv->vks.cbuf));
	vkbasic_present(&iv->vb, iv->vks.queue, iv->vks.cbuf, &iv->iid);
	iv->dirty = false;
}

static void mview_event(WlezwrapMview *wewmv, double x, double y) {
	Imgview *iv = (Imgview*)wewmv->data;
	if (wewmv->button == 0) {
		vec2 wpos = {(float)(x - wewmv->px), (float)(y - wewmv->py)};
		camcon2_s2w(&iv->camcon, wpos, wpos);
		iv->camcon.x += wpos[0];
		iv->camcon.y += wpos[1];
	} else if (wewmv->button == 1) {
		double cx = (double)iv->window_size[0] / 2.0;
		double cy = (double)iv->window_size[1] / 2.0;
		double t0 = atan2(wewmv->py - cy, wewmv->px - cx);
		double t1 = atan2(y - cy, x - cx);
		t1 = angle_norm(t1 - t0);
		iv->camcon.theta += (float)t1;
	} else if (wewmv->button == 2) {
		iv->camcon.k *=  1.0f - (float)(y - wewmv->py) * 0.005f;
	}
	iv->dirty = true;
}

static void f_key(Imgview *iv, char key, bool pressed) {
	if (!pressed) { return; }
	if (key == 'i') {
		iv->camcon.k *= 1.33f;
	} else if (key == 'o') {
		iv->camcon.k /= 1.33f;
	} else {
		return;
	}
	iv->dirty = true;
}

static void f_resize(Imgview *iv, uint32_t w, uint32_t h) {
	iv->window_size[0] = w;
	iv->window_size[1] = h;
	iv->resize = true;
	iv->dirty = true;

	vec4 *mp = (vec4*)iv->uniform.proj;
	glm_mat4_identity(mp);
	mp[0][0] = 2.0f / (float)w;
	mp[1][1] = 2.0f / (float)h;
}

static void f_quit(Imgview *iv) {
	iv->quit = true;
}

static void f_event(void* data, uint8_t type, WlezwrapEvent *e) {
	Imgview* iv = data;
	wlezwrap_mview_update(&iv->wewmv, type, e);
	switch(type) {
	case 0:
		f_quit(iv);
		break;
	case 1:
		f_resize(iv, e->resize[0], e->resize[1]);
		break;
	case 3:
		f_key(iv, e->key[0], (bool)e->key[1]);
		break;
	}
	if (iv->event != NULL) {
		iv->event(iv, type, e);
	}
}

// void imgview_insert_layer(Imgview* iv, ImgviewLyc *lyc) {
// 	uint32_t w = lyc->img.width;
// 	uint32_t h = lyc->img.height;
// 	vkbasic2d_insert_layer(&iv->vb2, &iv->vks, lyc->lid,
// 		lyc->offset[0], lyc->offset[1], w, h);
// 	memcpy(iv->vb2.overlay.data, lyc->img.data, 4 * w * h);
// 	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&iv->vks);
// 	vkbasic2d_build_command_copy_image(&iv->vb2, cbuf, iv->vb2.ldx_focus);
// 	vkstatic_oneshot_end(cbuf, &iv->vks);
// 	memset(iv->vb2.overlay.data, 0, 4 * w * h);
// }


static void imgview_init_pipeline_grid(Imgview *iv, VkDevice device) {
	char *path;
	VkhelperPipelineConfig vpc = {0};
	vkhelper_pipeline_config(&vpc, 0, 0, 0);

	path = ppath_rel(__FILE__, "../../shader/grid_vert.spv");
	vpc.stages[0].module = vkhelper_shader_module(device, path);
	free(path);

	path = ppath_rel(__FILE__, "../../shader/grid_frag.spv");
	vpc.stages[1].module = vkhelper_shader_module(device, path);
	free(path);

	vpc.dss.depthTestEnable = VK_FALSE;
	vpc.dss.depthWriteEnable = VK_FALSE;
	vpc.rast.cullMode = VK_CULL_MODE_NONE;
	vkhelper_pipeline_build(&iv->ppll_grid, &iv->ppl_grid,
		&vpc, iv->rp, device, 0);
	vkhelper_pipeline_config_deinit(&vpc, device);
}

static void imgview_init_pipeline_view(Imgview *iv, VkDevice device) {
	char *path;
	VkhelperPipelineConfig vpc = {0};
	vkhelper_pipeline_config(&vpc, 0, 0, 1);

	path = ppath_rel(__FILE__, "../../shader/view_vert.spv");
	vpc.stages[0].module = vkhelper_shader_module(device, path);
	free(path);

	path = ppath_rel(__FILE__, "../../shader/view_frag.spv");
	vpc.stages[1].module = vkhelper_shader_module(device, path);
	free(path);

	vpc.dss.depthTestEnable = VK_FALSE;
	vpc.dss.depthWriteEnable = VK_FALSE;
	vpc.rast.cullMode = VK_CULL_MODE_NONE;
	vpc.desc[0] = iv->desc.layout;
	vkhelper_pipeline_build(&iv->ppll_view, &iv->ppl_view,
		&vpc, iv->rp, device, 0);
	vkhelper_pipeline_config_deinit(&vpc, device);
}

void imgview_init(Imgview* iv, uint32_t img_width, uint32_t img_height) {
	wlezwrap_confgen(&iv->wew);
	iv->wew.data = (void*)iv;
	iv->wew.event = f_event;
	wlezwrap_init(&iv->wew);
	iv->wewmv.event = mview_event;
	iv->wewmv.data = (void*)iv;
	vkwayland_new(&iv->vks, iv->wew.wl.display, iv->wew.wl.surface);
	vkbasic_init(&iv->vb, iv->vks.device);
	camcon2_init(&iv->camcon);
	iv->camcon.k = 1.0;
	iv->resize = true;
	iv->dirty = true;
	iv->window_size[0] = 640;
	iv->window_size[1] = 480;

	// buffer
	vkhelper_buffer_init_gpu(&iv->ubufg, sizeof(ImgviewUniform),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		iv->vks.device, iv->vks.memprop);
	vkhelper_image_new(
		&iv->img, iv->vks.device, iv->vks.memprop,
		img_width, img_height,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT);
	glm_mat4_identity(iv->uniform.model);
	iv->uniform.model[0][0] = (float)img_width;
	iv->uniform.model[1][1] = (float)img_height;
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&iv->vks);
	vkhelper_barrier(cbuf,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		iv->img.image);
	vkstatic_oneshot_end(cbuf, &iv->vks);

	// desc set
	VkhelperDescConfig descconf;
	vkhelper_desc_config(&descconf, 2);
	vkhelper_desc_config_image(&descconf, 1);
	vkhelper_desc_build(&iv->desc, &descconf, iv->vks.device);
	VkDescriptorBufferInfo bufferinfo = {
		.buffer = iv->ubufg.buffer,
		.offset = 0,
		.range = sizeof(ImgviewUniform),
	};
	VkWriteDescriptorSet writes[2];
	writes[0] = (VkWriteDescriptorSet) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = iv->desc.set,
		.dstBinding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &bufferinfo,
	};
	iv->sampler = vkhelper_sampler(iv->vks.device);
	VkDescriptorImageInfo imageinfo = {
		.imageView = iv->img.imageview,
		.sampler = iv->sampler,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	writes[1] = (VkWriteDescriptorSet) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = iv->desc.set,
		.dstBinding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.pImageInfo = &imageinfo,
	};
	vkUpdateDescriptorSets(iv->vks.device, 2, writes, 0, NULL);

	// graphics
	VkhelperRenderpassConfig renderpass_conf;
	vkhelper_renderpass_config(&renderpass_conf,
		iv->vks.device,
		iv->vks.surface_format.format, iv->vks.depth_format);
	vkhelper_renderpass_build(
		&iv->rp,
		&renderpass_conf,
		iv->vks.device);
	imgview_init_pipeline_grid(iv, iv->vks.device);
	imgview_init_pipeline_view(iv, iv->vks.device);
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
	wlezwrap_deinit(&iv->wew);
}
