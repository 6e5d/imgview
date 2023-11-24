#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

#include "../include/imgview.h"
#include "../../vkstatic/include/oneshot.h"
#include "../../vkhelper/include/barrier.h"
#include "../../vkhelper/include/buffer.h"

static void image_upload(VkCommandBuffer cbuf,
	VkhelperBuffer buf,
	VkhelperImage img) {
	VkImageSubresourceLayers layers = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.layerCount = 1,
	};
	VkOffset3D offset = {0, 0, 0};
	VkExtent3D extent = {img.size[0], img.size[1], 1};
	VkBufferImageCopy icopy = {
		.bufferRowLength = img.size[0],
		.bufferImageHeight = img.size[1],
		.imageSubresource = layers,
		.imageOffset = offset,
		.imageExtent = extent,
	};
	vkhelper_barrier(cbuf,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		img.image);
	vkCmdCopyBufferToImage(cbuf,
		buf.buffer,
		img.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &icopy);
	vkhelper_barrier(cbuf,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		img.image);
}

int main(void) {
	Imgview iv = {0};
	imgview_init(&iv, 640, 480);
	VkhelperBuffer staging;
	uint32_t size = iv.img.size[0] * iv.img.size[1] * 4;
	vkhelper_buffer_init_cpu(&staging, size, iv.vks.device, iv.vks.memprop);
	void *p;
	assert(0 == vkMapMemory(iv.vks.device, staging.memory, 0, size, 0, &p));
	memset(p, 128, size);
	VkCommandBuffer cbuf1 = vkstatic_oneshot_begin(&iv.vks);
	image_upload(cbuf1, staging, iv.img);
	vkstatic_oneshot_end(cbuf1, &iv.vks);
	vkhelper_buffer_deinit(&staging, iv.vks.device);
	while (!iv.quit) {
		imgview_render_prepare(&iv);
		imgview_render(&iv);
		usleep((uint32_t)(10000));
	}
	assert(0 == vkDeviceWaitIdle(iv.vks.device));
	imgview_deinit(&iv);
}
