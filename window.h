#pragma once

#include "vulkan.h"
#include <glm/glm.hpp>
#include <vector>

struct vertex {
	glm::vec2 pos;
	glm::vec3 color;
};

class window {
public:
	window( uint32_t width, uint32_t height, std::string name );

	~window();

	void run();

private:
	void create_window();

	void destroy_window();

	void init_vulkan();

	void deinit_vulkan();

	void install_debug_callback();

	void uninstall_debug_callback();

	void choose_physical_device();

	void create_logical_device();

	void destroy_logical_device();

	void check_layers();

	void create_vertex_buffer();

	void destroy_buffers();

	void create_surface();

	void destroy_surface();

	void create_swapchain();

	void query_swapchain_support( vk::PhysicalDevice gpu );

	void destroy_swapchain();

	void create_image_views();

	void destroy_image_views();

	void create_renderpass();

	void destroy_renderpass();

	void create_graphics_pipeline();

	void destroy_graphics_pipeline();

	void create_framebuffers();

	void destroy_framebuffers();

	void create_commandpool();

	void destroy_commandpool();

	void create_command_buffers();

	void draw_frame();

	void create_semaphores();

	void destroy_semaphores();

	void recreate_swap_chain();

	void create_index_buffer();

	uint32_t find_memory_type( uint32_t type_filter, vk::MemoryPropertyFlags properties );

	std::pair<vk::Buffer, vk::DeviceMemory> create_buffer( vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags mem_props );

	void copy_buffer( vk::DeviceSize size, vk::Buffer src_buffer, vk::Buffer dst_buffer );

	uint32_t _width;
	uint32_t _height;
	std::string _name;

	GLFWwindow *_glfw_window = nullptr;

	struct {
		vk::Instance _vulkan_instance;
		vk::DebugReportCallbackEXT _debug_report_callback;
		std::vector<vk::ExtensionProperties> _extension_props;
		std::vector<std::string> _necessary_instance_extensions;
		std::vector<std::string> _necessary_layers;
	} _instance;

	struct {
		std::vector<std::string> _necessary_device_extensions;
		vk::PhysicalDevice _physical_device;
		vk::PhysicalDeviceProperties _physical_device_properties;
		vk::PhysicalDeviceFeatures _physical_device_features;
		std::vector<vk::ExtensionProperties> _physical_device_extension_properties;
		std::vector<vk::QueueFamilyProperties> _queue_family_properties;
		uint32_t _graphics_family_index;
		uint32_t _present_family_index;
		vk::Device _logical_device;
		vk::Queue _graphics_queue;
		vk::Queue _present_queue;
	} _gpu;

	struct {
		vk::SurfaceCapabilitiesKHR capabilities;
		vk::SwapchainKHR swapchain;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR> present_modes;
		vk::SurfaceFormatKHR chosen_format;
		vk::PresentModeKHR chosen_present_mode;
		uint32_t image_count;
		vk::Extent2D chosen_extent;
		std::vector<vk::Image> swapchain_images;
		std::vector<vk::ImageView> image_views;
		std::vector<vk::Framebuffer> framebuffers;
	} _swapchain;

	vk::SurfaceKHR _surface;
	vk::RenderPass _renderpass;
	vk::Pipeline _graphics_pipeline;
	vk::CommandPool _command_pool;
	std::vector<vk::CommandBuffer> _command_buffers;
	vk::Semaphore _image_available_sem;
	vk::Semaphore _render_finished_sem;

	const std::vector<vertex> _vertices = {
		{ { -0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f } },
		{ { 0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f } },
		{ { 0.5f, 0.5f },{ 0.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f },{ 1.0f, 1.0f, 1.0f } }
	};

	const std::vector<uint16_t> _indices = {
		0, 1, 2, 2, 3, 0
	};

	vk::Buffer _vertex_buffer;
	vk::DeviceMemory _vertex_buffer_memory;

	vk::Buffer _index_buffer;
	vk::DeviceMemory _index_buffer_memory;
};
