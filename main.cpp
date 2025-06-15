#include "vulkan_renderer.h"
#include "utils/logger.h"
#include <iostream>
#include <stdexcept>

int main() {

	if (!Logger::getInstance().init("VulkanRenderer", "logs/Renderer.log", LogLevel::INFO)) {
		std::cerr << "Failed to initialize logger" << std::endl;
		return -1;
	}

	LOGI("Starting Vulkan Renderer");

	try {
		VulkanRenderer renderer;

		if (!renderer.initialize(1280, 720, "Vulkan PBR Renderer")) {
			LOGE("Failed to initialize Renderer");
			return -1;
		}
		
		if (!renderer.loadModel(MODEL_DIR "Chair/Chair.obj")) {
			LOGE("Failed to load model");
			return -1;
		}
		
		if (!renderer.loadTexture(MODEL_DIR "Chair/Texture/Chair/Chair_Base_color.png")) {
			LOGW("Failed to load custom texture");
		}
		
		if (!renderer.createDefaultSkyBox()) {
			LOGE("Failed to create default skybox");
			return -1;
		}

		LOGI("Renderer initialized successfully, starting main loop");
		renderer.run();

	}
	catch (const std::exception& e) {
		LOGE("Renderer error: {}", e.what());
		Logger::getInstance().shutdown();
		return -1;
	}

	LOGI("Renderer shutting down");
	Logger::getInstance().shutdown();
	return 0;
}