#include "material.h"
#include "vulkan_context.h"
#include "utils/logger.h"

Material::Material(std::shared_ptr<VulkanContext> context) : context_(context) {
}

Material::~Material() {
	cleanup();
}

bool Material::initialize() {
	if (!createDescriptorSetLayout()) {
		LOGE("Failed to create descriptor set layout");
		return false;
	}

	if (!createUniformBuffers()) {
		LOGE("Failed to create uniform buffers");
		return false;
	}

	if (!createDescriptorPool()) {
		LOGE("Failed to create descriptor pool");
		return false;
	}
	
	LOGI("Material initialized successfully");
	return true;
}

void Material::cleanup() {
	auto device = context_->getDevice();

	if (uniform_buffer_) {
		device.destroyBuffer(uniform_buffer_);
		device.freeMemory(uniform_buffer_memory_);
	}

	if (material_buffer_) {
		device.destroyBuffer(material_buffer_);
		device.freeMemory(material_buffer_memory_);
	}

	if (light_buffer_) {
		device.destroyBuffer(light_buffer_);
		device.freeMemory(light_buffer_memory_);
	}

	if (descriptor_pool_) {
		device.destroyDescriptorPool(descriptor_pool_);
	}

	if (descriptor_set_layout_) {
		device.destroyDescriptorSetLayout(descriptor_set_layout_);
	}
}

void Material::setPBRProperties(const glm::vec3& albedo, float metallic, float roughness, float ao) {
	pbr_material_.albedo = albedo;
	pbr_material_.metallic = metallic;
	pbr_material_.roughness = roughness;
	pbr_material_.ao = ao;
	
	auto device = context_->getDevice();
	void* data = device.mapMemory(material_buffer_memory_, 0, sizeof(PBRMaterial));
	memcpy(data, &pbr_material_, sizeof(PBRMaterial));
	device.unmapMemory(material_buffer_memory_);
}

void Material::setLightProperties(const glm::vec3& position, const glm::vec3& color) {
	light_data_.position = position;
	light_data_.color = color;
	
	auto device = context_->getDevice();
	void* data = device.mapMemory(light_buffer_memory_, 0, sizeof(LightData));
	memcpy(data, &light_data_, sizeof(LightData));
	device.unmapMemory(light_buffer_memory_);
}

bool Material::setTexture(std::shared_ptr<Texture> texture) {
	if (!texture) {
		LOGE("Cannot set null texture");
		return false;
	}
	
	texture_ = texture;
	
	if (!descriptor_pool_) {
		LOGE("Cannot create descriptor sets: descriptor pool is null");
		return false;
	}

	if (!descriptor_set_layout_) {
		LOGE("Cannot create descriptor sets: descriptor set layout is null");
		return false;
	}
	
	return createDescriptorSets();
}

void Material::updateUniforms(const UniformBufferObject& ubo) {
	auto device = context_->getDevice();
	void* data = device.mapMemory(uniform_buffer_memory_, 0, sizeof(UniformBufferObject));
	memcpy(data, &ubo, sizeof(UniformBufferObject));
	device.unmapMemory(uniform_buffer_memory_);
}

void Material::bind(vk::CommandBuffer command_buffer, vk::PipelineLayout pipeline_layout) {
	command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
		pipeline_layout, 0, 1, &descriptor_set_, 0, nullptr);
}

bool Material::createDescriptorSetLayout() {
	auto device = context_->getDevice();

	std::array<vk::DescriptorSetLayoutBinding, 4> bindings{};
	
	bindings[0].binding = 0;
	bindings[0].descriptorCount = 1;
	bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindings[0].pImmutableSamplers = nullptr;
	bindings[0].stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	
	bindings[1].binding = 1;
	bindings[1].descriptorCount = 1;
	bindings[1].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindings[1].pImmutableSamplers = nullptr;
	bindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;
	
	bindings[2].binding = 2;
	bindings[2].descriptorCount = 1;
	bindings[2].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindings[2].pImmutableSamplers = nullptr;
	bindings[2].stageFlags = vk::ShaderStageFlagBits::eFragment;

	bindings[3].binding = 3;
	bindings[3].descriptorCount = 1;
	bindings[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
	bindings[3].pImmutableSamplers = nullptr;
	bindings[3].stageFlags = vk::ShaderStageFlagBits::eFragment;

	vk::DescriptorSetLayoutCreateInfo layout_info{};
	layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
	layout_info.pBindings = bindings.data();

	try {
		descriptor_set_layout_ = device.createDescriptorSetLayout(layout_info);
		return true;
	}
	catch (const std::exception& e) {
		LOGE("Failed to create descriptor set layout: {}", e.what());
		return false;
	}
}

bool Material::createDescriptorPool() {
	auto device = context_->getDevice();

	std::array<vk::DescriptorPoolSize, 2> pool_sizes{};
	pool_sizes[0].type = vk::DescriptorType::eUniformBuffer;
	pool_sizes[0].descriptorCount = 10;
	pool_sizes[1].type = vk::DescriptorType::eCombinedImageSampler;
	pool_sizes[1].descriptorCount = 10;

	vk::DescriptorPoolCreateInfo pool_info{};
	pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();
	pool_info.maxSets = 5;

	try {
		descriptor_pool_ = device.createDescriptorPool(pool_info);
		return true;
	}
	catch (const std::exception& e) {
		LOGE("Failed to create descriptor pool: {}", e.what());
		return false;
	}
}

bool Material::createDescriptorSets() {
	auto device = context_->getDevice();

	vk::DescriptorSetAllocateInfo alloc_info{};
	alloc_info.descriptorPool = descriptor_pool_;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &descriptor_set_layout_;

	try {
		auto descriptor_sets = device.allocateDescriptorSets(alloc_info);
		descriptor_set_ = descriptor_sets[0];
	}
	catch (const std::exception& e) {
		LOGE("Failed to allocate descriptor sets: {}", e.what());
		return false;
	}

	std::vector<vk::WriteDescriptorSet> descriptor_writes;

	vk::DescriptorBufferInfo buffer_info{};
	buffer_info.buffer = uniform_buffer_;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(UniformBufferObject);

	descriptor_writes.emplace_back();
	auto& ubo_write = descriptor_writes.back();
	ubo_write.dstSet = descriptor_set_;
	ubo_write.dstBinding = 0;
	ubo_write.dstArrayElement = 0;
	ubo_write.descriptorType = vk::DescriptorType::eUniformBuffer;
	ubo_write.descriptorCount = 1;
	ubo_write.pBufferInfo = &buffer_info;

	vk::DescriptorBufferInfo material_buffer_info{};
	material_buffer_info.buffer = material_buffer_;
	material_buffer_info.offset = 0;
	material_buffer_info.range = sizeof(PBRMaterial);

	descriptor_writes.emplace_back();
	auto& material_write = descriptor_writes.back();
	material_write.dstSet = descriptor_set_;
	material_write.dstBinding = 1;
	material_write.dstArrayElement = 0;
	material_write.descriptorType = vk::DescriptorType::eUniformBuffer;
	material_write.descriptorCount = 1;
	material_write.pBufferInfo = &material_buffer_info;

	vk::DescriptorBufferInfo light_buffer_info{};
	light_buffer_info.buffer = light_buffer_;
	light_buffer_info.offset = 0;
	light_buffer_info.range = sizeof(LightData);

	descriptor_writes.emplace_back();
	auto& light_write = descriptor_writes.back();
	light_write.dstSet = descriptor_set_;
	light_write.dstBinding = 2;
	light_write.dstArrayElement = 0;
	light_write.descriptorType = vk::DescriptorType::eUniformBuffer;
	light_write.descriptorCount = 1;
	light_write.pBufferInfo = &light_buffer_info;

	vk::DescriptorImageInfo image_info{};
	if (texture_) {
		image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		image_info.imageView = texture_->getImageView();
		image_info.sampler = texture_->getSampler();
	}

	descriptor_writes.emplace_back();
	auto& texture_write = descriptor_writes.back();
	texture_write.dstSet = descriptor_set_;
	texture_write.dstBinding = 3;
	texture_write.dstArrayElement = 0;
	texture_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	texture_write.descriptorCount = 1;
	texture_write.pImageInfo = &image_info;

	device.updateDescriptorSets(static_cast<uint32_t>(descriptor_writes.size()),
		descriptor_writes.data(), 0, nullptr);
	return true;
}

bool Material::createUniformBuffers() {
	auto device = context_->getDevice();
	
	vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
	if (!createBuffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		uniform_buffer_, uniform_buffer_memory_)) {
		return false;
	}
	
	buffer_size = sizeof(PBRMaterial);
	if (!createBuffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		material_buffer_, material_buffer_memory_)) {
		return false;
	}
	
	buffer_size = sizeof(LightData);
	if (!createBuffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		light_buffer_, light_buffer_memory_)) {
		return false;
	}
	
	setPBRProperties(glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, 0.5f, 1.0f);
	setLightProperties(glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(300.0f));

	return true;
}

bool Material::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
	vk::MemoryPropertyFlags properties,
	vk::Buffer& buffer, vk::DeviceMemory& buffer_memory) {
	auto device = context_->getDevice();

	vk::BufferCreateInfo buffer_info{};
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = vk::SharingMode::eExclusive;

	try {
		buffer = device.createBuffer(buffer_info);
	}
	catch (const std::exception& e) {
		LOGE("Failed to create buffer: {}", e.what());
		return false;
	}

	vk::MemoryRequirements mem_requirements = device.getBufferMemoryRequirements(buffer);

	vk::MemoryAllocateInfo alloc_info{};
	alloc_info.allocationSize = mem_requirements.size;
	alloc_info.memoryTypeIndex = findMemoryType(mem_requirements.memoryTypeBits, properties);

	try {
		buffer_memory = device.allocateMemory(alloc_info);
		device.bindBufferMemory(buffer, buffer_memory, 0);
	}
	catch (const std::exception& e) {
		LOGE("Failed to allocate buffer memory: {}", e.what());
		device.destroyBuffer(buffer);
		return false;
	}

	return true;
}

uint32_t Material::findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
	vk::PhysicalDeviceMemoryProperties mem_properties = context_->getPhysicalDevice().getMemoryProperties();

	for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) &&
			(mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}
