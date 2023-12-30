#include <math.h>

#include "../include/imgview.h"

void imgview(try_present)(Imgview() *iv) {
	if (iv->present) {
		vkbasic(present)(&iv->vb, iv->vks.queue, &iv->iid);
		iv->present = false;
	}
}

void imgview(resize)(Imgview() *iv, struct wl_surface *surface,
	uint32_t w, uint32_t h
) {
	assert(0 == vkDeviceWaitIdle(iv->vks.device));
	imgview(try_present)(iv);
	iv->window_size[0] = w;
	iv->window_size[1] = h;
	vkbasic(swapchain_update)(&iv->vb, &iv->vks, iv->rp, w, h);

	vec4 *mp = (vec4*)iv->uniform.proj;
	glm_mat4_identity(mp);
	mp[0][0] = 2.0f / (float)w;
	mp[1][1] = 2.0f / (float)h;

	iv->iid = 0;
	wl_surface_commit(surface);
}

static void imgview(draw_point)(Imgview() *iv, int32_t x, int32_t y) {
	if (iv->dots_len >= imgview(maxdot)) {
		fprintf(stderr, "error: dots len overflow\n");
		return;
	}
	if (x < 0 || y < 0 ||
		x >= (int32_t)iv->window_size[0] ||
		y >= (int32_t)iv->window_size[1])
	{
		return;
	}
	Imgview(Dot) *dot = &iv->dots[iv->dots_len];
	dot->x = 2.0f * (float)x / (float)iv->window_size[0] - 1.0f;
	dot->y = 2.0f * (float)y / (float)iv->window_size[1] - 1.0f;
	iv->dots_len += 1;
}

void imgview(draw_dashed_line)(Imgview() *iv,
	int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
	int dx, sx, dy, sy, dash = 0;
	if (x1 < x2) {
		dx = x2 - x1;
		sx = 1;
	} else {
		dx = x1 - x2;
		sx = -1;
	}
	if (y1 < y2) {
		dy = y2 - y1;
		sy = 1;
	} else {
		dy = y1 - y2;
		sy = -1;
	}
	int ge, ge2;
	if (dx > dy) {
		ge = dx / 2;
	} else {
		ge = -dy / 2;
	}
	while (1) {
		if (dash < 10) {
			imgview(draw_point)(iv, x1, y1);
		}
		if (x1 == x2 && y1 == y2) {
			break;
		}
		ge2 = ge;
		if (ge2 > -dx) {
			ge -= dy;
			x1 += sx;
		}
		if (ge2 < dy) {
			ge += dx;
			y1 += sy;
		}
		dash += 1;
		dash %= 20;
	}
}

void imgview(draw_cursor)(Imgview() *iv, float x, float y, float rad) {
	if (!iv->show_cursor) { return; }
	for (float dx = 0; dx < rad / 1.414213f; dx += 1.0f) {
		float dy = sqrtf(rad * rad - (float)(dx * dx));
		imgview(draw_point)(iv, (int32_t)(x - dx), (int32_t)(y - dy));
		imgview(draw_point)(iv, (int32_t)(x - dy), (int32_t)(y - dx));
		imgview(draw_point)(iv, (int32_t)(x - dx), (int32_t)(y + dy));
		imgview(draw_point)(iv, (int32_t)(x - dy), (int32_t)(y + dx));
		imgview(draw_point)(iv, (int32_t)(x + dx), (int32_t)(y - dy));
		imgview(draw_point)(iv, (int32_t)(x + dy), (int32_t)(y - dx));
		imgview(draw_point)(iv, (int32_t)(x + dx), (int32_t)(y + dy));
		imgview(draw_point)(iv, (int32_t)(x + dy), (int32_t)(y + dx));
	}
}

static void imgview(build_command)(Imgview() *iv, VkCommandBuffer cbuf) {
	uint32_t width = iv->window_size[0];
	uint32_t height = iv->window_size[1];
	vkCmdUpdateBuffer(cbuf, iv->ubufg.buffer,
		0, sizeof(Imgview(Uniform)), &iv->uniform);
	if (iv->dots_len > 0) {
		VkBufferCopy copy = {
			.size = iv->dots_len * sizeof(Imgview(Dot))
		};
		vkCmdCopyBuffer(cbuf, iv->dotsc.buffer, iv->dotsg.buffer,
			1, &copy);
	}
	vkhelper2(dynstate_vs)(cbuf, width, height);

	VkFramebuffer fb = iv->vb.vs.elements[iv->iid].framebuffer;
	vkhelper2(renderpass_begin)(cbuf, iv->rp, fb,
		iv->window_size[0], iv->window_size[1]);
	vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, iv->ppl_grid);
	vkCmdDraw(cbuf, 6, 1, 0, 0);
	vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, iv->ppl_view);
	vkCmdBindDescriptorSets(
		cbuf,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		iv->ppll_view,
		0, 1, &iv->desc.set, 0, NULL);
	vkCmdDraw(cbuf, 6, 1, 0, 0);
	if (iv->dots_len > 0) {
		vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			iv->ppl_dots);
		VkDeviceSize zero = 0;
		vkCmdBindVertexBuffers(cbuf, 0, 1, &iv->dotsg.buffer, &zero);
		vkCmdDraw(cbuf, (uint32_t)iv->dots_len, 1, 0, 0);
		iv->dots_len = 0;
	}
	vkCmdEndRenderPass(cbuf);
}

void imgview(render_prepare)(Imgview() *iv) {
	imgview(try_present)(iv);
	vkbasic(next_index)(&iv->vb, iv->vks.device, &iv->iid);
	vkstatic(begin)(&iv->vks);
}

static void blit(VkCommandBuffer cbuf,
	Vkhelper2(Image) *src, Vkhelper2(Image) *dst) {
	assert(src->size[0] == dst->size[0]);
	assert(src->size[1] == dst->size[1]);
	VkImageLayout src_layout = src->layout;
	VkImageLayout dst_layout = dst->layout;
	VkOffset3D offsets1[2] = {
		{0, 0, 0},
		{(int32_t)dst->size[0], (int32_t)dst->size[1], 1},
	};
	VkOffset3D offsets2[2] = {
		{0, 0, 0},
		{(int32_t)dst->size[0], (int32_t)dst->size[1], 1},
	};
	VkImageBlit *iblits = malloc(sizeof(VkImageBlit) * dst->mip);
	for (size_t i = 0; i < dst->mip; i += 1) {
		VkImageSubresourceLayers src_layer = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
		};
		VkImageSubresourceLayers dst_layer = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
			.mipLevel = (uint32_t)i,
		};
		iblits[i] = (VkImageBlit) {
			.srcSubresource = src_layer,
			.dstSubresource = dst_layer,
		};
		memcpy(iblits[i].srcOffsets, offsets1, 2 * sizeof(VkOffset3D));
		memcpy(iblits[i].dstOffsets, offsets2, 2 * sizeof(VkOffset3D));
		offsets2[1].x /= 2;
		offsets2[1].y /= 2;
		if (offsets2[1].x == 0) { offsets2[1].x = 1; }
		if (offsets2[1].y == 0) { offsets2[1].y = 1; }
	}
	vkhelper2(barrier)(cbuf, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		src);
	vkhelper2(barrier)(cbuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		dst);
	vkCmdBlitImage(cbuf,
		src->image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		dst->mip, iblits, VK_FILTER_LINEAR);
	vkhelper2(barrier)(cbuf, dst_layout,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		dst);
	vkhelper2(barrier)(cbuf, src_layout,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		src);
	free(iblits);
}

void imgview(render)(Imgview() *iv, Vkhelper2(Image) *image) {
	VkCommandBuffer cbuf = iv->vks.cbuf;
	blit(cbuf, image, &iv->img);
	imgview(build_command)(iv, cbuf);
	assert(0 == vkEndCommandBuffer(cbuf));
	vkbasic(submit)(&iv->vb, iv->vks.queue, cbuf);
	iv->present = true;
}

void imgview(init)(Imgview()* iv,
	struct wl_display *display, struct wl_surface *surface,
	Dmgrect() *rect
) {
	vkwayland(new)(&iv->vks, display, surface);
	vkbasic(init)(&iv->vb, iv->vks.device);
	iv->resize = true;
	iv->window_size[0] = 640;
	iv->window_size[1] = 480;
	imgview(init_render)(iv, rect);
}

void imgview(deinit)(Imgview()* iv) {
	VkDevice device = iv->vks.device;
	vkhelper2(desc_deinit)(&iv->desc, device);
	vkhelper2(buffer_deinit)(&iv->ubufg, device);
	vkhelper2(buffer_deinit)(&iv->dotsc, device);
	vkhelper2(buffer_deinit)(&iv->dotsg, device);
	vkhelper2(image_deinit)(&iv->img, device);
	vkDestroySampler(device, iv->sampler, NULL);
	vkDestroyRenderPass(device, iv->rp, NULL);
	vkDestroyPipeline(device, iv->ppl_grid, NULL);
	vkDestroyPipelineLayout(device, iv->ppll_grid, NULL);
	vkDestroyPipeline(device, iv->ppl_view, NULL);
	vkDestroyPipelineLayout(device, iv->ppll_view, NULL);
	vkDestroyPipeline(device, iv->ppl_dots, NULL);
	vkDestroyPipelineLayout(device, iv->ppll_dots, NULL);
	vkbasic(deinit)(&iv->vb, device);
	vkstatic(deinit)(&iv->vks);
}
