#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "../../vkbasic/include/vkbasic.h"
#include "../../vkbasic/include/wayland.h"
#include "../../vkbasic/include/common.h"
#include "../../vkbasic2d/include/vkbasic2d.h"
#include "../../wlbasic/include/wlbasic.h"
#include "../../simpleimg/include/simpleimg.h"

static Vkbasic* vb;
static Vkbasic2d* vb2;
static Wlbasic* wl;
static uint32_t index = 0;

void handle_resize(void) {
	if (wl->resize) {
		printf("Resize: %u %u\n", wl->width, wl->height);
		vkbasic_check(vkDeviceWaitIdle(vb->device));
		vkbasic_swapchain_update(
			vb, vb2->renderpass, wl->width, wl->height);
		index = 0;
		wl->resize = false;
		wl_surface_commit(wl->surface);
	}
}

int main(int argv, char** argc) {
	// if (argv <= 1) {
	// 	printf("need file name\n");
	// 	exit(1);
	// }
	// Simpleimg* img = simpleimg_load(argc[1]);
	Simpleimg* img = simpleimg_load("/tmp/test.jpg");
	// simpleimg_print(img);
	simpleimg_destroy(img);
	// exit(0);
	wl = wlbasic_init();
	vb = vkbasic_new_wayland(wl->display, wl->surface);
	vb2 = vkbasic2d_new(vb);

	printf("init finished\n");
	while (!wl->quit) {
		handle_resize();
		vkbasic_next_index(vb, &index);
		vkbasic2d_build_command(
			vb2,
			vb->cbuf,
			vb->vs->elements[index].framebuffer,
			wl->width,
			wl->height
		);
		vkbasic_check(vkEndCommandBuffer(vb->cbuf));
		vkbasic_present(vb, &index);
		wl_display_roundtrip(wl->display);
	}
	vkbasic_check(vkDeviceWaitIdle(vb->device));

	vkbasic2d_destroy(vb2, vb->device);
	vkbasic_destroy(vb);
	wlbasic_destroy(wl);
	return 0;
}
