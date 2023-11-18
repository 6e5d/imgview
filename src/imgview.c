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
	camcon2_build(&iv->camcon, iv->vb2.view->view);
}

void imgview_s2w(Imgview *iv, vec2 s, vec2 w) {
	vec4 ss = {
		s[0] - (float)iv->width * 0.5f,
		s[1] - (float)iv->height * 0.5f,
		0.0f, 1.0f};
	vec4 ww;
	mat4 t;
	camcon2_build(&iv->camcon, t);
	glm_mat4_inv(t, t);
	glm_mat4_mulv(t, ss, ww);
	w[0] = ww[0] / ww[3]; w[1] = ww[1] / ww[3];
	w[0] += (float)iv->vb2.img.width * 0.5f;
	w[1] += (float)iv->vb2.img.height * 0.5f;
}

void imgview_damage_all(Imgview *iv) {
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

void imgview_render(Imgview *iv) {
	if (iv->resize) {
		handle_resize(iv);
		iv->resize = false;
	}
	if (!iv->dirty) { return; }
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
	iv->dirty = false;
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
	iv->dirty = true;
}

static void f_key(Imgview *iv, char key, bool pressed) {
	float move_distance = 0.1f * iv->camcon.k;
	if (!pressed) { return; }
	if (key == 'j') {
		iv->camcon.y -= move_distance * (float)iv->vb2.img.height;
	} else if (key == 'k') {
		iv->camcon.y += move_distance * (float)iv->vb2.img.height;
	} else if (key == 'h') {
		iv->camcon.x += move_distance * (float)iv->vb2.img.width;
	} else if (key == 'l') {
		iv->camcon.x -= move_distance * (float)iv->vb2.img.width;
	} else if (key == 'i') {
		iv->camcon.k *= 1.33f;
	} else if (key == 'o') {
		iv->camcon.k /= 1.33f;
	} else {
		return;
	}
	iv->dirty = true;
}

static void f_resize(Imgview *iv, uint32_t w, uint32_t h) {
	iv->width = w;
	iv->height = h;
	iv->resize = true;
	iv->dirty = true;

	vec4 *mp = (vec4*)iv->vb2.view->proj;
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

static void setup_img(Imgview* iv, uint32_t width, uint32_t height) {
	iv->vb2.img.width = width;
	iv->vb2.img.height = height;
	vkbasic2d_init(&iv->vb2, &iv->vks);
	memset(iv->vb2.img.data, 0, width * height * 4);

	vec4 *mm = (vec4*)iv->vb2.view->model;
	glm_mat4_identity(mm);
	mm[0][0] = (float)width;
	mm[1][1] = (float)height;
	mm[3][0] = -(float)width * 0.5f;
	mm[3][1] = -(float)height * 0.5f;
}

void imgview_init(Imgview* iv, uint32_t width, uint32_t height) {
	wlezwrap_confgen(&iv->wew);
	iv->wew.data = (void*)iv;
	iv->wew.event = f_event;
	wlezwrap_init(&iv->wew);
	iv->wewmv.event = mview_event;
	iv->wewmv.data = (void*)iv;
	vkwayland_new(&iv->vks, iv->wew.wl.display, iv->wew.wl.surface);
	vkbasic_init(&iv->vb, iv->vks.device);
	setup_img(iv, width, height);
	camcon2_init(&iv->camcon);
	iv->camcon.k = 0.5;
	iv->resize = true;
	iv->width = 640;
	iv->height = 640;
	iv->dirty = true;
}

void imgview_deinit(Imgview* iv) {
	assert(0 == vkDeviceWaitIdle(iv->vks.device));
	vkbasic2d_deinit(&iv->vb2, iv->vks.device);
	vkbasic_deinit(&iv->vb, iv->vks.device, iv->vks.cpool);
	vkstatic_deinit(&iv->vks);
	wlezwrap_deinit(&iv->wew);
}
