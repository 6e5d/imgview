#include <vulkan/vulkan.h>

#include "../../dmgrect/include/dmgrect.h"
#include "../../ppath/include/ppath.h"
#include "../../vkhelper/include/barrier.h"
#include "../../vkstatic/include/oneshot.h"
#include "../../vkhelper/include/pipeline.h"
#include "../../vkhelper/include/sampler.h"
#include "../../vkhelper/include/shader.h"
#include "../include/pipeline.h"
#include "../include/imgview.h"

static void imgview_init_pipeline_grid(Imgview *iv, VkDevice device) {
	char *path = NULL;
	VkhelperPipelineConfig vpc = {0};
	vkhelper_pipeline_config(&vpc, 0, 0, 0);

	ppath_rel(&path, __FILE__, "../../shader/grid_vert.spv");
	vpc.stages[0].module = vkhelper_shader_module(device, path);
	ppath_rel(&path, __FILE__, "../../shader/grid_frag.spv");
	vpc.stages[1].module = vkhelper_shader_module(device, path);
	free(path);

	vpc.dss.depthTestEnable = VK_FALSE;
	vpc.dss.depthWriteEnable = VK_FALSE;
	vpc.rast.cullMode = VK_CULL_MODE_NONE;
	vkhelper_pipeline_build(&iv->ppll_grid, &iv->ppl_grid,
		&vpc, iv->rp, device, 0);
	vkhelper_pipeline_config_deinit(&vpc, device);
}

static void imgview_init_pipeline_view(Imgview *iv, VkDevice device) {
	char *path = NULL;
	VkhelperPipelineConfig vpc = {0};
	vkhelper_pipeline_config(&vpc, 0, 0, 1);

	ppath_rel(&path, __FILE__, "../../shader/view_vert.spv");
	vpc.stages[0].module = vkhelper_shader_module(device, path);
	ppath_rel(&path, __FILE__, "../../shader/view_frag.spv");
	vpc.stages[1].module = vkhelper_shader_module(device, path);
	free(path);

	vpc.dss.depthTestEnable = VK_FALSE;
	vpc.dss.depthWriteEnable = VK_FALSE;
	vpc.rast.cullMode = VK_CULL_MODE_NONE;
	vpc.desc[0] = iv->desc.layout;
	vkhelper_pipeline_build(&iv->ppll_view, &iv->ppl_view,
		&vpc, iv->rp, device, 0);
	vkhelper_pipeline_config_deinit(&vpc, device);
}

void imgview_init_render(Imgview* iv, Dmgrect *dmg) {
	// buffer
	vkhelper_buffer_init_gpu(&iv->ubufg, sizeof(ImgviewUniform),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		iv->vks.device, iv->vks.memprop);
	// TODO: mipmap is required, do a copy between vwdlayout and imgview
	vkhelper_image_new(
		&iv->img, iv->vks.device, iv->vks.memprop,
		dmg->size[0], dmg->size[1], true,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT);
	glm_mat4_identity(iv->uniform.model);
	iv->uniform.model[0][0] = (float)dmg->size[0];
	iv->uniform.model[1][1] = (float)dmg->size[1];
	iv->uniform.model[3][0] = (float)dmg->offset[0];
	iv->uniform.model[3][1] = (float)dmg->offset[1];
	VkCommandBuffer cbuf = vkstatic_oneshot_begin(&iv->vks);
	vkhelper_barrier(cbuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		&iv->img);
	vkstatic_oneshot_end(cbuf, &iv->vks);

	// desc set
	VkhelperDescConfig descconf;
	vkhelper_desc_config(&descconf, 2);
	vkhelper_desc_config_image(&descconf, 1);
	vkhelper_desc_build(&iv->desc, &descconf, iv->vks.device);
	vkhelper_desc_config_deinit(&descconf);
	VkDescriptorBufferInfo bufferinfo = {
		.buffer = iv->ubufg.buffer,
		.offset = 0,
		.range = sizeof(ImgviewUniform),
	};
	VkWriteDescriptorSet writes[2];
	writes[0] = (VkWriteDescriptorSet) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = iv->desc.set,
		.dstBinding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &bufferinfo,
	};
	VkSamplerCreateInfo sampler_ci = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.maxLod = (float)iv->img.mip,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
	};
	vkCreateSampler(iv->vks.device, &sampler_ci, NULL, &iv->sampler);
	VkDescriptorImageInfo imageinfo = {
		.imageView = iv->img.imageview,
		.sampler = iv->sampler,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	writes[1] = (VkWriteDescriptorSet) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = iv->desc.set,
		.dstBinding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.pImageInfo = &imageinfo,
	};
	vkUpdateDescriptorSets(iv->vks.device, 2, writes, 0, NULL);

	// graphics
	VkhelperRenderpassConfig renderpass_conf;
	vkhelper_renderpass_config(&renderpass_conf,
		iv->vks.device,
		iv->vks.surface_format.format, iv->vks.depth_format);
	vkhelper_renderpass_build(
		&iv->rp,
		&renderpass_conf,
		iv->vks.device);
	imgview_init_pipeline_grid(iv, iv->vks.device);
	imgview_init_pipeline_view(iv, iv->vks.device);
}
