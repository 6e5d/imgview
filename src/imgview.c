#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "../../camcon2/include/camcon2.h"
#include "../../vkbasic/include/vkbasic.h"
#include "../../vkwayland/include/vkwayland.h"
#include "../../vkstatic/include/vkstatic.h"
#include "../../vkbasic2d/include/vkbasic2d.h"
#include "../../wlezwrap/include/mview.h"
#include "../../wlezwrap/include/wlezwrap.h"
#include "../../simpleimg/include/simpleimg.h"
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
	glm_mat4_identity(iv->vb2.view->view);
	iv->vb2.view->view[0][0] = 2.0f / (float)iv->width;
	iv->vb2.view->view[1][1] = 2.0f / (float)iv->height;
	mat4 t;
	camcon2_build(&iv->camcon, t);
	glm_mat4_mul(iv->vb2.view->view, t, iv->vb2.view->view);
	glm_mat4_identity(t);
	t[0][0] = (float)iv->vb2.img.width;
	t[1][1] = (float)iv->vb2.img.height;
	t[3][0] = -(float)iv->vb2.img.width * 0.5f;
	t[3][1] = -(float)iv->vb2.img.height * 0.5f;
	glm_mat4_mul(iv->vb2.view->view, t, iv->vb2.view->view);
}

static void alldamage(Imgview *iv) {
	iv->damage[0] = 0;
	iv->damage[1] = 0;
	iv->damage[2] = iv->vb2.img.width;
	iv->damage[3] = iv->vb2.img.height;
}

static void handle_resize(Imgview *iv) {
	assert(0 == vkDeviceWaitIdle(iv->vks.device));
	vkbasic_swapchain_update(&iv->vb, &iv->vks, iv->vb2.renderpass,
		iv->width, iv->height);
	iv->iid = 0;
	wl_surface_commit(iv->wew.wl.surface);
	iv->vb2.recreate_pipeline = true;
}

static void render(Imgview *iv) {
	if (iv->resize) {
		handle_resize(iv);
		iv->resize = false;
	}
	write_cam(iv);
	vkbasic_next_index(&iv->vb, iv->vks.device, &iv->iid);
	vkbasic2d_build_command(
		&iv->vb2,
		&iv->vks,
		iv->vks.cbuf,
		iv->vb.vs.elements[iv->iid].framebuffer,
		iv->width,
		iv->height,
		iv->damage
	);
	memset(iv->damage, 0, sizeof(iv->damage));
	vkbasic_present(&iv->vb, iv->vks.queue, iv->vks.cbuf, &iv->iid);
	wl_display_roundtrip(iv->wew.wl.display);
}

static void mview_event(WlezwrapMview *wewmv, double x, double y) {
	Imgview *iv = (Imgview*)wewmv->data;
	if (wewmv->button == 0) {
		iv->camcon.x += (float)(x - wewmv->px);
		iv->camcon.y += (float)(y - wewmv->py);
	} else if (wewmv->button == 1) {
		double cx = (double)iv->width / 2.0;
		double cy = (double)iv->height / 2.0;
		double t0 = atan2(wewmv->py - cy, wewmv->px - cx);
		double t1 = atan2(y - cy, x - cx);
		t1 = angle_norm(t1 - t0);
		iv->camcon.theta += (float)t1;
	} else if (wewmv->button == 2) {
		iv->camcon.k *=  1.0f - (float)(y - wewmv->py) * 0.005f;
	}
	render(iv);
}

static void f_key(Imgview *iv, char key, bool pressed) {
	float move_distance = 0.1f * iv->camcon.k;
	if (!pressed) { return; }
	if (key == 'j') {
		iv->camcon.y -= move_distance * (float)iv->vb2.img.height;
		render(iv);
	} else if (key == 'k') {
		iv->camcon.y += move_distance * (float)iv->vb2.img.height;
		render(iv);
	} else if (key == 'h') {
		iv->camcon.x += move_distance * (float)iv->vb2.img.width;
		render(iv);
	} else if (key == 'l') {
		iv->camcon.x -= move_distance * (float)iv->vb2.img.width;
		render(iv);
	} else if (key == 'i') {
		iv->camcon.k *= 1.33f;
		render(iv);
	} else if (key == 'o') {
		iv->camcon.k /= 1.33f;
		render(iv);
	}
}

static void f_resize(Imgview *iv, uint32_t w, uint32_t h) {
	iv->width = w;
	iv->height = h;
	iv->resize = true;
	render(iv);
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
}

void imgview_init(Imgview* iv, char* path) {
	wlezwrap_confgen(&iv->wew);
	iv->wew.data = (void*)iv;
	iv->wew.event = f_event;
	wlezwrap_init(&iv->wew);
	iv->wewmv.event = mview_event;
	iv->wewmv.data = (void*)iv;

	vkwayland_new(&iv->vks, iv->wew.wl.display, iv->wew.wl.surface);
	vkbasic_init(&iv->vb, iv->vks.device);

	// img loading and uploading
	Simpleimg img;
	simpleimg_load(&img, path);
	iv->vb2.img.width = img.width;
	iv->vb2.img.height = img.height;
	vkbasic2d_init(&iv->vb2, &iv->vks);
	memcpy(iv->vb2.img.data, img.data, img.width * img.height * 4);
	simpleimg_deinit(&img);

	camcon2_init(&iv->camcon);
	iv->camcon.k = 0.5;
	alldamage(iv);

	iv->resize = true;
	iv->width = 640;
	iv->height = 640;
	printf("init finished\n");
	render(iv);
}

void imgview_deinit(Imgview* iv) {
	assert(0 == vkDeviceWaitIdle(iv->vks.device));
	vkbasic2d_deinit(&iv->vb2, iv->vks.device);
	vkbasic_deinit(&iv->vb, iv->vks.device, iv->vks.cpool);
	vkstatic_deinit(&iv->vks);
	wlezwrap_deinit(&iv->wew);
}
