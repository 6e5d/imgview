#include <assert.h>
#include <wayland-client.h>

#include "../include/imgview.h"

int main(int argc, char **argv) {
	assert(argc >= 2);
	Imgview iv = {0};
	imgview_init(&iv, 640, 480);
	while(wl_display_dispatch(iv.wew.wl.display) && !iv.quit);
	imgview_deinit(&iv);
	return 0;
}
