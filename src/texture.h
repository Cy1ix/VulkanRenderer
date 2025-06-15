#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>
#include <string>
#include <vector>

class VulkanContext;

struct ImageData {
    unsigned char* pixels;
    int width;
    int height;
    int channels;

    ImageData() : pixels(nullptr), width(0), height(0), channels(0) {}

    ImageData(const ImageData&) = delete;
    ImageData& operator=(const ImageData&) = delete;

    ImageData(ImageData&& other) noexcept
        : pixels(other.pixels), width(other.width), height(other.height), channels(other.channels) {
        other.pixels = nullptr;
        other.width = other.height = other.channels = 0;
    }
    
    ImageData& operator=(ImageData&& other) noexcept {
        if (this != &other) {
            free();
            pixels = other.pixels;
            width = other.width;
            height = other.height;
            channels = other.channels;
            other.pixels = nullptr;
            other.width = other.height = other.channels = 0;
        }
        return *this;
    }

    ~ImageData() { free(); }

    void free();
    bool isValid() const { return pixels != nullptr; }
};

class Texture {
public:
    Texture(std::shared_ptr<VulkanContext> context);
    ~Texture();

    bool loadFromFile(const std::string& filename);
    void cleanup();

    vk::Image getImage() const { return image_; }
    vk::ImageView getImageView() const { return image_view_; }
    vk::Sampler getSampler() const { return sampler_; }
    
    static ImageData loadImageData(const std::string& filename, int desired_channels = 4);
    static void freeImageData(ImageData& data);
    
    static bool saveImageData(const std::string& filename, const std::vector<unsigned char>& data,
        int width, int height, int channels);

private:
    bool createImage(uint32_t width, uint32_t height, vk::Format format,
        vk::ImageTiling tiling, vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties);
    bool createImageView(vk::Format format);
    bool createSampler();
    void transitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout);
    void copyBufferToImage(vk::Buffer buffer, uint32_t width, uint32_t height);
    bool createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::Buffer& buffer, vk::DeviceMemory& buffer_memory);
    uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);

    std::shared_ptr<VulkanContext> context_;
    vk::Image image_;
    vk::DeviceMemory image_memory_;
    vk::ImageView image_view_;
    vk::Sampler sampler_;
};
