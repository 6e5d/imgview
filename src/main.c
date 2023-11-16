#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "../../camcon2/include/camcon2.h"
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
static int8_t mouse_state;
static double px, py;
static bool pp = false;
static Camcon2 camcon;
static uint32_t damage[4];
static Simpleimg img;

static double angle_norm(double angle) {
	while (angle >= M_PI) {
		angle -= 2.0 * M_PI;
	}
	while (angle < M_PI) {
		angle += 2.0 * M_PI;
	}
	return angle;
}

static void write_cam(void) {
	glm_mat4_identity(vb2.view->view);
	vb2.view->view[0][0] = 2.0f / (float)width;
	vb2.view->view[1][1] = 2.0f / (float)height;
	mat4 t;
	camcon2_build(&camcon, t);
	glm_mat4_mul(vb2.view->view, t, vb2.view->view);
	glm_mat4_identity(t);
	t[0][0] = (float)vb2.img.width;
	t[1][1] = (float)vb2.img.height;
	t[3][0] = -(float)vb2.img.width * 0.5f;
	t[3][1] = -(float)vb2.img.height * 0.5f;
	glm_mat4_mul(vb2.view->view, t, vb2.view->view);
}

static void alldamage(void) {
	damage[0] = 0; damage[1] = 0;
	damage[2] = img.width; damage[3] = img.height;
}

static void handle_resize(void) {
	assert(0 == vkDeviceWaitIdle(vks.device));
	vkbasic_swapchain_update(&vb, &vks, vb2.renderpass,
		width, height);
	iid = 0;
	wl_surface_commit(wew.wl.surface);
	vb2.recreate_pipeline = true;
}

static void render(void) {
	if (resize) {
		handle_resize();
		resize = false;
	}
	write_cam();
	vkbasic_next_index(&vb, vks.device, &iid);
	vkbasic2d_build_command(
		&vb2,
		&vks,
		vks.cbuf,
		vb.vs.elements[iid].framebuffer,
		width,
		height,
		damage
	);
	memset(damage, 0, sizeof(damage));
	vkbasic_present(&vb, vks.queue, vks.cbuf, &iid);
	wl_display_roundtrip(wew.wl.display);
}

static void f_motion(void* data, double x, double y, double pressure) {
	if (mouse_state == 0) { return; }
	if (pp) {
		if (mouse_state == 1) {
			camcon.x += (float)(x - px);
			camcon.y += (float)(y - py);
		} else if (mouse_state == 2) {
			double cx = (double)width / 2.0;
			double cy = (double)height / 2.0;
			double t0 = atan2(py - cy, px - cx);
			double t1 = atan2(y - cy, x - cx);
			t1 = angle_norm(t1 - t0);
			camcon.theta += (float)t1;
		} else if (mouse_state == 3) {
			camcon.k *=  1.0f - (float)(y - py) * 0.005f;
		}
		render();
	}
	pp = true;
	px = x;
	py = y;
}

static void f_button(void* data, uint8_t button, bool pressed) {
	if (!pressed) {
		mouse_state = 0;
		pp = false;
		return;
	}
	if (button == 1) {
		if (mouse_state < 0) {
			mouse_state = -mouse_state;
		} else {
			mouse_state = 1;
		}
	}
}

static void f_key(void* data, char ch, bool pressed) {
	if (!pressed) {
		mouse_state = 0;
		return;
	}
	if (ch == -1) {
		mouse_state = -2;
	} else if (ch == -2) {
		mouse_state = -3;
	}
}

static void f_resize(void* data, uint32_t w, uint32_t h) {
	width = w;
	height = h;
	resize = true;
	render();
}

static void f_quit(void* data) {
	quit = true;
}

int main(int argv, char** argc) {
	assert(argv >= 2);
	wlezwrap_confgen(&wew);
	wew.f_resize = f_resize;
	wew.f_key = f_key;
	wew.f_quit = f_quit;
	wew.f_motion = f_motion;
	wew.f_button = f_button;
	wlezwrap_init(&wew);
	vkwayland_new(&vks, wew.wl.display, wew.wl.surface);
	vkbasic_init(&vb, vks.device);
	camcon2_init(&camcon);
	camcon.k = 0.5;

	// img loading and uploading
	simpleimg_load(&img, argc[1]);
	vb2.img.width = img.width;
	vb2.img.height = img.height;
	vkbasic2d_init(&vb2, &vks);
	memcpy(vb2.img.data, img.data, img.width * img.height * 4);
	simpleimg_deinit(&img);
	alldamage();

	printf("init finished\n");
	render();
	while(wl_display_dispatch(wew.wl.display) && !quit);
	assert(0 == vkDeviceWaitIdle(vks.device));
	vkbasic2d_deinit(&vb2, vks.device);
	vkbasic_deinit(&vb, vks.device, vks.cpool);
	vkstatic_deinit(&vks);
	wlezwrap_deinit(&wew);
	return 0;
}
