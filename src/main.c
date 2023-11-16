#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
static uint32_t iid = 0;
static bool quit = false;
static bool resize = true;
static uint32_t width = 640;
static uint32_t height = 640;

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
		iid = 0;
		resize = false;
		wl_surface_commit(wew.wl.surface);
		vb2.recreate_pipeline = true;
	}
}

int main(int argv, char** argc) {
	assert(argv >= 2);
	Simpleimg img, buf;
	simpleimg_load(&img, argc[1]);
	wlezwrap_confgen(&wew);
	wew.f_resize = f_resize;
	wew.f_quit = f_quit;
	wlezwrap_init(&wew);

	vkwayland_new(&vks, wew.wl.display, wew.wl.surface);
	vkbasic_init(&vb, vks.device);
	vkbasic2d_init(&vb2, &vks);
	glm_mat4_identity(vb2.view->view);

	buf.data = (uint8_t*)vb2.imdata;
	buf.width = VKBASIC2D_BUFFER_WIDTH;
	buf.height = VKBASIC2D_BUFFER_HEIGHT;

	simpleimg_paste(&img, &buf, 0, 0);

	printf("init finished\n");
	while (!quit) {
		handle_resize();
		vkbasic_next_index(&vb, vks.device, &iid);
		vkbasic2d_build_command(
			&vb2,
			&vks,
			vks.cbuf,
			vb.vs.elements[iid].framebuffer,
			width,
			height
		);
		vkbasic_present(&vb, vks.queue, vks.cbuf, &iid);
		wl_display_roundtrip(wew.wl.display);
		usleep(10000);
	}
	assert(0 == vkDeviceWaitIdle(vks.device));
	vkbasic2d_deinit(&vb2, vks.device);
	vkbasic_deinit(&vb, vks.device, vks.cpool);
	vkstatic_deinit(&vks);
	wlezwrap_deinit(&wew);
	simpleimg_deinit(&img);
	return 0;
}
