#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "../../vkbasic/include/vkbasic.h"
#include "../../vkwayland/include/vkwayland.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../../vkbasic2d/include/vkbasic2d.h"
#include "../../wlezwrap/include/wlezwrap.h"
#include "../../simpleimg/include/simpleimg.h"

static Vkbasic vb;
static Vkbasic2d vb2;
static Vkstatic vks;
static Wlezwrap wew;
static uint32_t index = 0;
static bool quit = false;
static bool resize = true;
static uint32_t width = 640;
static uint32_t height = 640;
static uint32_t index;

static void f_resize(void* data, uint32_t w, uint32_t h) {
	width = w;
	height = h;
	resize = true;
}

static void f_quit(void* data) {
	quit = true;
}

static void handle_resize(void) {
	if (resize) {
		printf("Resize: %u %u\n", width, height);
		assert(0 == vkDeviceWaitIdle(vks.device));
		vkbasic_swapchain_update(&vb, &vks, vb2.renderpass,
			width, height);
		index = 0;
		resize = false;
		wl_surface_commit(wew.wl.surface);
		vb2.recreate_pipeline = true;
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
	wlezwrap_confgen(&wew);
	wew.f_resize = f_resize;
	wew.f_quit = f_quit;
	wlezwrap_init(&wew);

	vkwayland_new(&vks, wew.wl.display, wew.wl.surface);
	vkbasic_init(&vb, vks.device);
	vkbasic2d_init(&vb2, &vks);

	printf("init finished\n");
	while (!quit) {
		handle_resize();
		vkbasic_next_index(&vb, vks.device, &index);
		vkbasic2d_build_command(
			&vb2,
			&vks,
			vks.cbuf,
			vb.vs.elements[index].framebuffer,
			width,
			height
		);
		assert(0 == vkEndCommandBuffer(vks.cbuf));
		vkbasic_present(&vb, vks.queue, vks.cbuf, &index);
		wl_display_roundtrip(wew.wl.display);
	}
	assert(0 == vkDeviceWaitIdle(vks.device));
	vkbasic2d_deinit(&vb2, vks.device);
	vkbasic_deinit(&vb, vks.device, vks.cpool);
	vkstatic_deinit(&vks);
	wlezwrap_deinit(&wew);
	return 0;
}
