#include "../include/imgview.h"

static void imgview(init_pipeline_dots)(Imgview() *iv, VkDevice device) {
	char *path = NULL;
	Vkhelper2(PipelineConfig) vpc = {0};
	vkhelper2(pipeline_config)(&vpc, 1, 1, 0);
	vpc.ias.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	vpc.vib[0] = (VkVertexInputBindingDescription) {
		.binding = 0,
		.stride = sizeof(Imgview(Dot)),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	vpc.via[0] = (VkVertexInputAttributeDescription) {
		.binding = 0,
		.location = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = 0,
	};
	vkhelper2(pipeline_simple_shader)(&vpc, device,
		__FILE__, "../../vwdraw_shaders/build/dots");
	free(path);

	vpc.dss.depthTestEnable = VK_FALSE;
	vpc.dss.depthWriteEnable = VK_FALSE;
	vpc.rast.cullMode = VK_CULL_MODE_NONE;
	vkhelper2(pipeline_build)(&iv->ppll_dots, &iv->ppl_dots,
		&vpc, iv->rp, device, 0);
	vkhelper2(pipeline_config_deinit)(&vpc, device);
}

static void imgview(init_pipeline_grid)(Imgview() *iv, VkDevice device) {
	char *path = NULL;
	Vkhelper2(PipelineConfig) vpc = {0};
	vkhelper2(pipeline_config)(&vpc, 0, 0, 0);
	vkhelper2(pipeline_simple_shader)(&vpc, device,
		__FILE__, "../../vwdraw_shaders/build/grid");
	free(path);

	vpc.dss.depthTestEnable = VK_FALSE;
	vpc.dss.depthWriteEnable = VK_FALSE;
	vpc.rast.cullMode = VK_CULL_MODE_NONE;
	vkhelper2(pipeline_build)(&iv->ppll_grid, &iv->ppl_grid,
		&vpc, iv->rp, device, 0);
	vkhelper2(pipeline_config_deinit)(&vpc, device);
}

static void imgview(init_pipeline_view)(Imgview() *iv, VkDevice device) {
	Vkhelper2(PipelineConfig) vpc = {0};
	vkhelper2(pipeline_config)(&vpc, 0, 0, 1);

	vkhelper2(pipeline_simple_shader)(&vpc, device,
		__FILE__, "../../vwdraw_shaders/build/view");
	vpc.dss.depthTestEnable = VK_FALSE;
	vpc.dss.depthWriteEnable = VK_FALSE;
	vpc.rast.cullMode = VK_CULL_MODE_NONE;
	vpc.desc[0] = iv->desc.layout;
	vkhelper2(pipeline_build)(&iv->ppll_view, &iv->ppl_view,
		&vpc, iv->rp, device, 0);
	vkhelper2(pipeline_config_deinit)(&vpc, device);
}

void imgview(init_render)(Imgview()* iv, Dmgrect() *dmg) {
	// buffer
	vkhelper2(buffer_init_gpu)(&iv->dotsg,
		sizeof(Imgview(Dot)) * imgview(maxdot),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		iv->vks.device, iv->vks.memprop);
	vkhelper2(buffer_init_cpu)(&iv->dotsc,
		sizeof(Imgview(Dot)) * imgview(maxdot),
		iv->vks.device, iv->vks.memprop);
	assert(0 == vkMapMemory(iv->vks.device, iv->dotsc.memory, 0,
		sizeof(Imgview(Dot)) * imgview(maxdot), 0, (void**)&iv->dots));
	vkhelper2(buffer_init_gpu)(&iv->ubufg, sizeof(Imgview(Uniform)),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		iv->vks.device, iv->vks.memprop);
	// TODO: mipmap is required, do a copy between vwdlayout and imgview
	vkhelper2(image_new)(
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
	VkCommandBuffer cbuf = vkstatic(oneshot_begin)(&iv->vks);
	vkhelper2(barrier)(cbuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		&iv->img);
	vkstatic(oneshot_end)(cbuf, &iv->vks);

	// desc set
	Vkhelper2(DescConfig) descconf;
	vkhelper2(desc_config)(&descconf, 2);
	vkhelper2(desc_config_image)(&descconf, 1);
	vkhelper2(desc_build)(&iv->desc, &descconf, iv->vks.device);
	vkhelper2(desc_config_deinit)(&descconf);
	VkDescriptorBufferInfo bufferinfo = {
		.buffer = iv->ubufg.buffer,
		.offset = 0,
		.range = sizeof(Imgview(Uniform)),
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
	Vkhelper2(RenderpassConfig) renderpass_conf;
	vkhelper2(renderpass_config)(&renderpass_conf,
		iv->vks.surface_format.format, iv->vks.depth_format);
	vkhelper2(renderpass_build)(&iv->rp, &renderpass_conf, iv->vks.device);
	imgview(init_pipeline_grid)(iv, iv->vks.device);
	imgview(init_pipeline_view)(iv, iv->vks.device);
	imgview(init_pipeline_dots)(iv, iv->vks.device);
}
