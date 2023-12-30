#include <cglm.h>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "../../dmgrect/build/dmgrect.h"
#include "../../vkbasic/build/vkbasic.h"
#include "../../vkstatic/build/vkstatic.h"
#include "../../vkhelper2/build/vkhelper2.h"

const static size_t imgview(maxdot) = 100000;

typedef struct {
	mat4 model; // use fixed rectangle [0, 1]
	mat4 proj; // window
	mat4 view; // camera
} Imgview(Uniform);

typedef struct {
	float x;
	float y;
} Imgview(Dot);

typedef struct {
	Imgview(Uniform) uniform;
	Vkhelper2(Buffer) ubufg;
	Vkhelper2(Buffer) dotsg;
	Vkhelper2(Buffer) dotsc;
	Imgview(Dot) *dots;
	size_t dots_len;
	Vkhelper2(Image) img;
	Vkhelper2(Desc) desc;
	VkSampler sampler;

	VkRenderPass rp;
	VkPipeline ppl_dots;
	VkPipelineLayout ppll_dots;
	VkPipeline ppl_view;
	VkPipelineLayout ppll_view;
	VkPipeline ppl_grid;
	VkPipelineLayout ppll_grid;

	bool show_cursor;
	bool present;
	bool resize;
	Vkstatic() vks;
	Vkbasic() vb;
	uint32_t iid;
	uint32_t window_size[2];
} Imgview();

void imgview(render_prepare)(Imgview() *iv);
void imgview(try_present)(Imgview() *iv); // sync
void imgview(render)(Imgview() *iv, Vkhelper2(Image) *image);
void imgview(init)(Imgview()* iv, struct wl_display *display,
	struct wl_surface *surface, Dmgrect() *rect);
void imgview(draw_dashed_line)(Imgview() *iv,
	int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void imgview(draw_cursor)(Imgview() *iv, float x, float y, float rad);
void imgview(deinit)(Imgview()* iv);
void imgview(resize)(Imgview() *iv, struct wl_surface *surface,
	uint32_t w, uint32_t h);
void imgview(init_render)(Imgview()* iv, Dmgrect() *dmg);
