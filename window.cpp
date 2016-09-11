#include <iostream>
#include <set>
#include <complex>
#include "window.h"

#define NOMINMAX
#include <Windows.h>

#define BO

#include <boost/scope_exit.hpp>
#include "utils.h"

#include <glm/glm.hpp>

static vk::VertexInputBindingDescription
vertex_binding_description()
{
	vk::VertexInputBindingDescription input_binding_description;
	input_binding_description.setInputRate( vk::VertexInputRate::eVertex )
	                         .setBinding( 0 )
	                         .setStride( sizeof(vertex) );
	return input_binding_description;
}

static std::array<vk::VertexInputAttributeDescription, 2>
vertex_attribute_descriptions()
{
	std::array<vk::VertexInputAttributeDescription, 2> res;
	res[ 0 ].setBinding( 0 )
	        .setFormat( vk::Format::eR32G32Sfloat )
	        .setLocation( 0 )
	        .setOffset( offsetof(vertex, pos) );
	res[ 1 ].setBinding( 0 )
	        .setFormat( vk::Format::eR32G32B32Sfloat )
	        .setLocation( 1 )
	        .setOffset( offsetof(vertex, color) );
	return res;
}

static void
glfw_error_callback( int error, const char *error_msg )
{
	puts( error_msg );
}

static vk::Bool32
vulkan_debug_callback( vk::DebugReportFlagBitsEXT flags, vk::DebugReportObjectTypeEXT objType, uint64_t obj,
                       size_t location, int32_t code, const char *layerPrefix, const char *msg, void *userData )
{
	std::ostringstream ss;
	ss << "VK: ";
	switch (flags) {
	case vk::DebugReportFlagBitsEXT::eInformation:
		ss << "Information: ";
		break;
	case vk::DebugReportFlagBitsEXT::eWarning:
		ss << "Warning: ";
		break;
	case vk::DebugReportFlagBitsEXT::ePerformanceWarning:
		ss << "PerformanceWarning: ";
		break;
	case vk::DebugReportFlagBitsEXT::eError:
		ss << "Error: ";
		break;
	case vk::DebugReportFlagBitsEXT::eDebug:
		ss << "Debug: ";
		break;
	}
	ss << "[" << layerPrefix << "] " << msg << std::endl;
	OutputDebugString( ss.str().c_str() );
	if (flags == vk::DebugReportFlagBitsEXT::eError) {
		DebugBreak();
		return VK_TRUE;
	} else {
		return VK_FALSE;
	}
}

window::window( uint32_t width, uint32_t height, std::string name )
	: _width( width )
	, _height( height )
	, _name( std::move( name ) )
{
	_instance._necessary_layers.emplace_back( "VK_LAYER_LUNARG_standard_validation" );
	create_window();
	check_layers();

	init_vulkan();
	install_debug_callback();
	create_surface();
	choose_physical_device();
	create_logical_device();
	create_swapchain();
	create_image_views();
	create_renderpass();
	create_graphics_pipeline();
	create_framebuffers();
	create_commandpool();
	create_vertex_buffer();
	create_index_buffer();
	create_command_buffers();
	create_semaphores();
}

window::~window()
{
	destroy_buffers();
	destroy_semaphores();
	destroy_commandpool();
	destroy_framebuffers();
	destroy_graphics_pipeline();
	destroy_renderpass();
	destroy_image_views();
	destroy_swapchain();
	destroy_logical_device();
	destroy_surface();
	uninstall_debug_callback();
	deinit_vulkan();
	destroy_window();
}

void
window::create_window()
{
	glfwSetErrorCallback( glfw_error_callback );
	auto glfw_init_ret = glfwInit();
	if (glfw_init_ret == GLFW_FALSE) {
		throw std::runtime_error( "failed to initialize GLFW" );
	}

	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

	_glfw_window = glfwCreateWindow( _width, _height, _name.c_str(), nullptr, nullptr );
	if (!_glfw_window) {
		throw std::runtime_error( "failed to initialize GLFW" );
	}

	auto resize_callback = [](GLFWwindow *w, int width, int height) {
			if (width == 0 || height == 0) {
				return;
			}

			window *app = reinterpret_cast<window *>( glfwGetWindowUserPointer( w ) );
			app->recreate_swap_chain();
		};

	glfwSetWindowUserPointer( _glfw_window, this );
	glfwSetWindowSizeCallback( _glfw_window, resize_callback );
}

void
window::destroy_window()
{
}

void
window::init_vulkan()
{
	uint32_t glfw_ext_count;
	auto glfw_extension_names = glfwGetRequiredInstanceExtensions( &glfw_ext_count );
	std::copy_n( glfw_extension_names, glfw_ext_count, std::back_inserter( _instance._necessary_instance_extensions ) );
	_instance._necessary_instance_extensions.emplace_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
	std::sort( _instance._necessary_instance_extensions.begin(), _instance._necessary_instance_extensions.end() );
	_instance._necessary_instance_extensions
	         .erase( std::unique( _instance._necessary_instance_extensions.begin(),
	                              _instance._necessary_instance_extensions.end() ),
	                 _instance._necessary_instance_extensions.end() );

	std::vector<const char *> nec_exts( _instance._necessary_instance_extensions.size() );
	std::transform( _instance._necessary_instance_extensions.cbegin(), _instance._necessary_instance_extensions.cend(),
	                nec_exts.begin(), [](auto &str) {
		                return str.c_str();
	                } );

	std::vector<const char *> nec_layers( _instance._necessary_layers.size() );
	std::transform( _instance._necessary_layers.cbegin(), _instance._necessary_layers.cend(), nec_layers.begin(),
	                [](auto &str) {
		                return str.c_str();
	                } );

	vk::ApplicationInfo app_info;
	app_info.setApiVersion( VK_API_VERSION_1_0 )
	        .setApplicationVersion( VK_MAKE_VERSION( 1, 0, 0 ) )
	        .setEngineVersion( VK_MAKE_VERSION( 1, 0, 0 ) )
	        .setPApplicationName( _name.c_str() )
	        .setPEngineName( "No engine" );

	vk::InstanceCreateInfo instance_create_info;
	instance_create_info.setPApplicationInfo( &app_info )
	                    .setEnabledExtensionCount( ( uint32_t ) _instance._necessary_instance_extensions.size() )
	                    .setPpEnabledExtensionNames( nec_exts.data() )
	                    .setEnabledLayerCount( ( uint32_t ) _instance._necessary_layers.size() )
	                    .setPpEnabledLayerNames( nec_layers.data() );

	_instance._vulkan_instance = vk::createInstance( instance_create_info );

	_instance._extension_props = vk::enumerateInstanceExtensionProperties( nullptr );

	if (_instance._extension_props.size() == 0) {
		std::cout << "No extensions available.\n";
	} else {
		std::cout << "Available extensions:\n";
		for (auto &extension : _instance._extension_props) {
			std::cout << "\t" << extension.extensionName << "\n";
		}
	}
}

void
window::run()
{
	while (!glfwWindowShouldClose( _glfw_window )) {
		glfwPollEvents();
		draw_frame();
	}
	_gpu._logical_device.waitIdle();
}

void
window::check_layers()
{
	auto available_layers = vk::enumerateInstanceLayerProperties();
	if (available_layers.size() == 0) {
		std::cout << "No available layers.\n";
	} else {
		std::cout << "Available layers:\n";
		for (auto &layer : available_layers) {
			std::cout << "\t" << layer.layerName << "\t" << layer.description << "\n";
		}
	}
	for (auto &layer : _instance._necessary_layers) {
		auto position = std::find_if( available_layers.cbegin(), available_layers.cend(), [&](auto l) {
			                              return layer == l.layerName;
		                              } );
		if (position == available_layers.cend()) {
			std::cout << "Error: " << layer << " layer not found.\n";
			throw std::runtime_error( "Layer not found" );
		}
	}
	std::cout << std::endl;
}

void
window::install_debug_callback()
{
	vk::DebugReportCallbackCreateInfoEXT callback_create_info;
	callback_create_info.setPfnCallback( ( PFN_vkDebugReportCallbackEXT ) vulkan_debug_callback );
	callback_create_info.setFlags(
		vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eInformation
		| vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning );

	PFN_vkCreateDebugReportCallbackEXT ptr_vkCreateDebugReportCallbackEXT;
	ptr_vkCreateDebugReportCallbackEXT =
		( PFN_vkCreateDebugReportCallbackEXT ) _instance._vulkan_instance.getProcAddr( "vkCreateDebugReportCallbackEXT" );

	auto callback_create_info_tmp = static_cast<VkDebugReportCallbackCreateInfoEXT>( callback_create_info );

	VkDebugReportCallbackEXT debug_report_callback_tmp;

	auto ret = ptr_vkCreateDebugReportCallbackEXT( _instance._vulkan_instance, &callback_create_info_tmp, nullptr,
	                                               &debug_report_callback_tmp );
	if (ret != VK_SUCCESS) {
		throw std::system_error( ret, std::system_category() );
	}
	_instance._debug_report_callback = debug_report_callback_tmp;
}

void
window::uninstall_debug_callback()
{
	PFN_vkDestroyDebugReportCallbackEXT ptr_vkDestroyDebugReportCallbackEXT;
	ptr_vkDestroyDebugReportCallbackEXT =
		( PFN_vkDestroyDebugReportCallbackEXT ) _instance._vulkan_instance.getProcAddr( "vkDestroyDebugReportCallbackEXT" );

	ptr_vkDestroyDebugReportCallbackEXT( _instance._vulkan_instance, _instance._debug_report_callback, nullptr );
}

void
window::deinit_vulkan()
{
	_instance._vulkan_instance.destroy();
}

void
window::choose_physical_device()
{
	_gpu._necessary_device_extensions.push_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
	std::sort( _gpu._necessary_device_extensions.begin(), _gpu._necessary_device_extensions.end() );

	auto phys_devices = _instance._vulkan_instance.enumeratePhysicalDevices();
	if (phys_devices.size() == 0) {
		throw std::runtime_error( "no GPU that is compatible with Vulkan" );
	}
	bool found = false;
	for (auto gpu: phys_devices) {
		_gpu._queue_family_properties = gpu.getQueueFamilyProperties();
		_gpu._present_family_index = ( uint32_t ) _gpu._queue_family_properties.size();
		_gpu._graphics_family_index = ( uint32_t ) _gpu._queue_family_properties.size();

		for (uint32_t i = 0; i < _gpu._queue_family_properties.size(); ++i) {
			if (gpu.getSurfaceSupportKHR( i, _surface )) {
				_gpu._present_family_index = i;
				break;
			}
		}
		for (uint32_t i = 0; i < _gpu._queue_family_properties.size(); ++i) {
			auto qfp = _gpu._queue_family_properties[ i ];
			if (qfp.queueFlags & vk::QueueFlagBits::eGraphics) {
				_gpu._graphics_family_index = i;
				break;
			}
		}
		if (_gpu._present_family_index == _gpu._queue_family_properties.size()
			|| _gpu._graphics_family_index == _gpu._queue_family_properties.size()) {
			continue;
		}

		_gpu._physical_device_extension_properties = gpu.enumerateDeviceExtensionProperties();
		std::vector<const char *> supported_extension_names( _gpu._physical_device_extension_properties.size() );
		std::transform( _gpu._physical_device_extension_properties.begin(),
		                _gpu._physical_device_extension_properties.end(), supported_extension_names.begin(),
		                [](const vk::ExtensionProperties &p) {
			                return static_cast<const char *>( p.extensionName );
		                } );
		std::sort( supported_extension_names.begin(), supported_extension_names.end() );

		if (!std::includes( supported_extension_names.cbegin(), supported_extension_names.cend(),
		                    _gpu._necessary_device_extensions.cbegin(), _gpu._necessary_device_extensions.cend() )) {
			continue;
		}

		query_swapchain_support( gpu );
		if (_swapchain.formats.empty() || _swapchain.present_modes.empty()) {
			continue;
		}

		found = true;
		_gpu._physical_device = gpu;
	}

	if (!found) {
		throw std::runtime_error( "no GPU that is compatible with Vulkan Graphics & Present queues" );
	}

	_gpu._physical_device_properties = _gpu._physical_device.getProperties();
	_gpu._physical_device_features = _gpu._physical_device.getFeatures();
	std::cout << "Found GPU: " << _gpu._physical_device_properties.deviceName << std::endl;
}

void
window::create_logical_device()
{
	std::set<uint32_t> queues = { _gpu._graphics_family_index, _gpu._present_family_index };
	std::vector<vk::DeviceQueueCreateInfo> queue_create_info;
	queue_create_info.reserve( queues.size() );
	float prio = 1;

	for (uint32_t queue_i : queues) {
		vk::DeviceQueueCreateInfo queue_create_info_tmp;
		queue_create_info_tmp.setQueueFamilyIndex( queue_i ).setQueueCount( 1 ).setPQueuePriorities( &prio );
		queue_create_info.emplace_back( std::move( queue_create_info_tmp ) );
	}

	std::vector<const char *> ext_names( _gpu._necessary_device_extensions.size() );
	std::transform( _gpu._necessary_device_extensions.cbegin(), _gpu._necessary_device_extensions.cend(),
	                ext_names.begin(), [](const std::string &s) {
		                return s.c_str();
	                } );

	vk::DeviceCreateInfo device_create_info;
	device_create_info.setQueueCreateInfoCount( ( uint32_t ) queue_create_info.size() )
	                  .setPQueueCreateInfos( queue_create_info.data() )
	                  .setEnabledExtensionCount( ( uint32_t ) _gpu._necessary_device_extensions.size() )
	                  .setPpEnabledExtensionNames( ext_names.data() )
	                  .setPEnabledFeatures( &_gpu._physical_device_features );

	_gpu._logical_device = _gpu._physical_device.createDevice( device_create_info );

	_gpu._graphics_queue = _gpu._logical_device.getQueue( _gpu._graphics_family_index, 0 );
	_gpu._present_queue = _gpu._logical_device.getQueue( _gpu._present_family_index, 0 );
}

void
window::destroy_logical_device()
{
	_gpu._logical_device.waitIdle();
	_gpu._logical_device.destroy();
}

void
window::create_surface()
{
	VkSurfaceKHR tmp_surface;
	if (glfwCreateWindowSurface( _instance._vulkan_instance, _glfw_window, nullptr, &tmp_surface ) != VK_SUCCESS) {
		throw std::runtime_error( "failed to create window surface" );
	}
	_surface = tmp_surface;
}

void
window::destroy_surface()
{
	_instance._vulkan_instance.destroySurfaceKHR( _surface );
}

void
window::create_swapchain()
{
	vk::SurfaceFormatKHR preferred_format;
	preferred_format.setColorSpace( vk::ColorSpaceKHR::eSrgbNonlinear );
	preferred_format.setFormat( vk::Format::eA8B8G8R8UnormPack32 );

	if (_swapchain.formats.size() == 1 && _swapchain.formats[ 0 ].format == vk::Format::eUndefined) {
		_swapchain.chosen_format = preferred_format;
	} else if (std::find( _swapchain.formats.cbegin(), _swapchain.formats.cend(), preferred_format )
		!= _swapchain.formats.cend()) {
		_swapchain.chosen_format = preferred_format;
	} else {
		_swapchain.chosen_format = _swapchain.formats[ 0 ];
	}
	std::cout << "Chose surface format: " << to_string( _swapchain.chosen_format.colorSpace ) << " and "
		<< to_string( _swapchain.chosen_format.format ) << std::endl;

	struct {
		vk::PresentModeKHR present_mode;
		uint32_t min_image_count;
	} preferred_present_modes[] = { { vk::PresentModeKHR::eMailbox, 3 },
		{ vk::PresentModeKHR::eFifoRelaxed, 2 },
		{ vk::PresentModeKHR::eImmediate, 2 },
		{ vk::PresentModeKHR::eFifo, 2 } };

	for (auto pm : preferred_present_modes) {
		auto it = std::find( _swapchain.present_modes.cbegin(), _swapchain.present_modes.cend(), pm.present_mode );
		if (it == _swapchain.present_modes.cend()) {
			continue;
		}
		if (_swapchain.capabilities.minImageCount < pm.min_image_count) {
			continue;
		}
		if (_swapchain.capabilities.maxImageCount > 0 && _swapchain.capabilities.maxImageCount < pm.min_image_count) {
			continue;
		}
		_swapchain.chosen_present_mode = pm.present_mode;
		_swapchain.image_count = pm.min_image_count;
		break;
	}

	if (_swapchain.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()
	) {
		_swapchain.chosen_extent = _swapchain.capabilities.currentExtent;
	} else {
		vk::Extent2D ext{ _width, _height };

		ext.width = std::max( _swapchain.capabilities.minImageExtent.width,
		                      std::min( _swapchain.capabilities.maxImageExtent.width, ext.width ) );
		ext.height = std::max( _swapchain.capabilities.minImageExtent.height,
		                       std::min( _swapchain.capabilities.maxImageExtent.height, ext.height ) );

		_swapchain.chosen_extent = ext;
	}
	std::cout << "Chose present mode: " << to_string( _swapchain.chosen_present_mode ) << std::endl;
	std::cout << "Chosen extent: " << _swapchain.chosen_extent.width << "x" << _swapchain.chosen_extent.height
		<< std::endl;
	std::cout << "Image count: " << _swapchain.image_count << std::endl;

	auto old_swapchain = std::move( _swapchain.swapchain );
	vk::SwapchainCreateInfoKHR swapchain_create_info;
	swapchain_create_info.setSurface( _surface )
	                     .setOldSwapchain( VK_NULL_HANDLE )
	                     .setPreTransform( _swapchain.capabilities.currentTransform )
	                     .setClipped( VK_TRUE )
	                     .setCompositeAlpha( vk::CompositeAlphaFlagBitsKHR::eOpaque )
	                     .setImageUsage( vk::ImageUsageFlagBits::eColorAttachment )
	                     .setImageFormat( _swapchain.chosen_format.format )
	                     .setImageColorSpace( _swapchain.chosen_format.colorSpace )
	                     .setPresentMode( _swapchain.chosen_present_mode )
	                     .setMinImageCount( _swapchain.image_count )
	                     .setImageArrayLayers( 1 )
	                     .setImageExtent( _swapchain.chosen_extent )
	                     .setOldSwapchain( old_swapchain );
	uint32_t queue_indices[] = { _gpu._graphics_family_index, _gpu._present_family_index };

	if (queue_indices[ 0 ] != queue_indices[ 1 ]) {
		swapchain_create_info.setImageSharingMode( vk::SharingMode::eConcurrent )
		                     .setQueueFamilyIndexCount( 2 )
		                     .setPQueueFamilyIndices( queue_indices );
	} else {
		swapchain_create_info.setImageSharingMode( vk::SharingMode::eExclusive );
	}

	_swapchain.swapchain = _gpu._logical_device.createSwapchainKHR( swapchain_create_info );
	_swapchain.swapchain_images = _gpu._logical_device.getSwapchainImagesKHR( _swapchain.swapchain );
	assert( _swapchain.swapchain_images.size() == _swapchain.image_count );
	if (old_swapchain) {
		_gpu._logical_device.destroySwapchainKHR( old_swapchain );
	}
}

void
window::destroy_swapchain()
{
	_gpu._logical_device.destroySwapchainKHR( _swapchain.swapchain );
}

void
window::query_swapchain_support( vk::PhysicalDevice gpu )
{
	_swapchain.capabilities = gpu.getSurfaceCapabilitiesKHR( _surface );
	_swapchain.formats = gpu.getSurfaceFormatsKHR( _surface );
	_swapchain.present_modes = gpu.getSurfacePresentModesKHR( _surface );
}

void
window::create_image_views()
{
	_swapchain.image_views.resize( _swapchain.image_count );
	for (uint32_t i = 0; i < _swapchain.image_count; ++i) {
		vk::ImageSubresourceRange range;
		range.setAspectMask( vk::ImageAspectFlagBits::eColor )
		     .setBaseArrayLayer( 0 )
		     .setBaseMipLevel( 0 )
		     .setLayerCount( 1 )
		     .setLevelCount( 1 );
		vk::ImageViewCreateInfo create_info;
		create_info.setImage( _swapchain.swapchain_images[ i ] )
		           .setViewType( vk::ImageViewType::e2D )
		           .setFormat( _swapchain.chosen_format.format )
		           .setSubresourceRange( range );
		_swapchain.image_views[ i ] = _gpu._logical_device.createImageView( create_info );
	}
}

void
window::destroy_image_views()
{
	for (auto &img : _swapchain.image_views) {
		_gpu._logical_device.destroyImageView( img );
	}
}

void
window::create_graphics_pipeline()
{
	auto vertex_code = read_file( "shaders/shader.vert.spv" );
	auto shader_code = read_file( "shaders/shader.frag.spv" );
	vk::ShaderModule vertex_module, fragment_module;
	vk::ShaderModuleCreateInfo vertex_ci, fragment_ci;
	vertex_ci.setCodeSize( vertex_code.size() ).setPCode( ( const uint32_t * ) vertex_code.data() );
	fragment_ci.setCodeSize( shader_code.size() ).setPCode( ( const uint32_t * ) shader_code.data() );
	vertex_module = _gpu._logical_device.createShaderModule( vertex_ci );
	BOOST_SCOPE_EXIT( vertex_module, &_gpu )
		{
			_gpu._logical_device.destroyShaderModule( vertex_module );
		}

		BOOST_SCOPE_EXIT_END
	fragment_module = _gpu._logical_device.createShaderModule( fragment_ci );
	BOOST_SCOPE_EXIT( fragment_module, &_gpu )
		{
			_gpu._logical_device.destroyShaderModule( fragment_module );
		}

		BOOST_SCOPE_EXIT_END

	vk::PipelineShaderStageCreateInfo pstci[2];
	pstci[ 0 ].setStage( vk::ShaderStageFlagBits::eVertex ).setModule( vertex_module ).setPName( "main" );
	pstci[ 1 ].setStage( vk::ShaderStageFlagBits::eFragment ).setModule( fragment_module ).setPName( "main" );

	auto binding_description = vertex_binding_description();
	auto attribute_descriptions = vertex_attribute_descriptions();
	vk::PipelineVertexInputStateCreateInfo vertex_input_info;
	vertex_input_info.setVertexBindingDescriptionCount( 1 )
	                 .setPVertexBindingDescriptions( &binding_description )
	                 .setVertexAttributeDescriptionCount( attribute_descriptions.size() )
	                 .setPVertexAttributeDescriptions( attribute_descriptions.data() );


	vk::PipelineInputAssemblyStateCreateInfo input_assembly;
	input_assembly.setTopology( vk::PrimitiveTopology::eTriangleList ).setPrimitiveRestartEnable( VK_FALSE );

	vk::Viewport viewport;
	viewport.setX( 0 )
	        .setY( 0 )
	        .setWidth( _swapchain.chosen_extent.width )
	        .setHeight( _swapchain.chosen_extent.height )
	        .setMinDepth( 0 )
	        .setMaxDepth( 1 );

	vk::Rect2D scissor;
	scissor.setOffset( { 0, 0 } ).setExtent( _swapchain.chosen_extent );

	vk::PipelineViewportStateCreateInfo viewport_state;
	viewport_state.setViewportCount( 1 ).setPViewports( &viewport ).setScissorCount( 1 ).setPScissors( &scissor );

	vk::PipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.setRasterizerDiscardEnable( VK_FALSE )
	          .setPolygonMode( vk::PolygonMode::eFill )
	          .setLineWidth( 1 )
	          .setCullMode( vk::CullModeFlagBits::eBack )
	          .setFrontFace( vk::FrontFace::eClockwise )
	          .setDepthBiasEnable( VK_FALSE );

	vk::PipelineMultisampleStateCreateInfo multisampling;
	multisampling.setSampleShadingEnable( VK_FALSE ).setRasterizationSamples( vk::SampleCountFlagBits::e1 );

	vk::PipelineColorBlendAttachmentState color_blend_attachment;
	color_blend_attachment.setBlendEnable( VK_FALSE )
	                      .setColorWriteMask( vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eB
		                      | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eR )
	                      .setBlendEnable( VK_FALSE );

	vk::PipelineColorBlendStateCreateInfo color_blending;
	color_blending.setLogicOpEnable( VK_FALSE ).setAttachmentCount( 1 ).setPAttachments( &color_blend_attachment );

	vk::PipelineLayoutCreateInfo pipeline_layout_create_info;
	vk::PipelineLayout pipeline_layout;
	pipeline_layout = _gpu._logical_device.createPipelineLayout( pipeline_layout_create_info );
	BOOST_SCOPE_EXIT( pipeline_layout, &_gpu )
		{
			_gpu._logical_device.destroyPipelineLayout( pipeline_layout );
		}

		BOOST_SCOPE_EXIT_END

	vk::GraphicsPipelineCreateInfo pipeline_create_info;
	pipeline_create_info.setStageCount( 2 )
	                    .setPStages( pstci )
	                    .setPVertexInputState( &vertex_input_info )
	                    .setPInputAssemblyState( &input_assembly )
	                    .setPViewportState( &viewport_state )
	                    .setPRasterizationState( &rasterizer )
	                    .setPMultisampleState( &multisampling )
	                    .setPColorBlendState( &color_blending )
	                    .setLayout( pipeline_layout )
	                    .setRenderPass( _renderpass )
	                    .setSubpass( 0 )
	                    .setBasePipelineHandle( VK_NULL_HANDLE );

	_graphics_pipeline = _gpu._logical_device.createGraphicsPipeline( VK_NULL_HANDLE, pipeline_create_info );
}

void
window::create_renderpass()
{
	vk::AttachmentDescription color_attachment;
	color_attachment.setFormat( _swapchain.chosen_format.format )
	                .setSamples( vk::SampleCountFlagBits::e1 )
	                .setLoadOp( vk::AttachmentLoadOp::eClear )
	                .setStoreOp( vk::AttachmentStoreOp::eStore )
	                .setStencilLoadOp( vk::AttachmentLoadOp::eDontCare )
	                .setStencilStoreOp( vk::AttachmentStoreOp::eDontCare )
	                .setInitialLayout( vk::ImageLayout::eUndefined )
	                .setFinalLayout( vk::ImageLayout::ePresentSrcKHR );

	vk::AttachmentReference color_attachment_ref;
	color_attachment_ref.setAttachment( 0 ).setLayout( vk::ImageLayout::eColorAttachmentOptimal );

	vk::SubpassDescription subpass;
	subpass.setPipelineBindPoint( vk::PipelineBindPoint::eGraphics )
	       .setColorAttachmentCount( 1 )
	       .setPColorAttachments( &color_attachment_ref );

	vk::SubpassDependency dependency;
	dependency.setSrcSubpass( VK_SUBPASS_EXTERNAL )
	          .setDstSubpass( 0 )
	          .setSrcStageMask( vk::PipelineStageFlagBits::eBottomOfPipe )
	          .setSrcAccessMask( vk::AccessFlagBits::eMemoryRead )
	          .setDstStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput )
	          .setDstAccessMask( vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite );

	vk::RenderPassCreateInfo renderpass_create_info;
	renderpass_create_info.setAttachmentCount( 1 )
	                      .setPAttachments( &color_attachment )
	                      .setSubpassCount( 1 )
	                      .setPSubpasses( &subpass )
	                      .setDependencyCount( 1 )
	                      .setPDependencies( &dependency );

	_renderpass = _gpu._logical_device.createRenderPass( renderpass_create_info );
}

void
window::destroy_renderpass()
{
	_gpu._logical_device.destroyRenderPass( _renderpass );
}

void
window::destroy_graphics_pipeline()
{
	_gpu._logical_device.destroyPipeline( _graphics_pipeline );
}

void
window::create_framebuffers()
{
	_swapchain.framebuffers.clear();
	_swapchain.framebuffers.reserve( _swapchain.image_count );
	for (auto &&view : _swapchain.image_views) {
		vk::FramebufferCreateInfo framebuffer_create_info;
		framebuffer_create_info.setRenderPass( _renderpass )
		                       .setAttachmentCount( 1 )
		                       .setPAttachments( &view )
		                       .setWidth( _swapchain.chosen_extent.width )
		                       .setHeight( _swapchain.chosen_extent.height )
		                       .setLayers( 1 );

		_swapchain.framebuffers.emplace_back( _gpu._logical_device.createFramebuffer( framebuffer_create_info ) );
	}
}

void
window::destroy_framebuffers()
{
	for (auto &&framebuffer : _swapchain.framebuffers) {
		_gpu._logical_device.destroyFramebuffer( framebuffer );
		framebuffer = VK_NULL_HANDLE;
	}
}

void
window::create_commandpool()
{
	vk::CommandPoolCreateInfo command_pool_create_info;
	command_pool_create_info.setQueueFamilyIndex( _gpu._graphics_family_index );

	_command_pool = _gpu._logical_device.createCommandPool( command_pool_create_info );
}

void
window::destroy_commandpool()
{
	_gpu._logical_device.destroyCommandPool( _command_pool );
	_command_pool = VK_NULL_HANDLE;
}

void
window::create_command_buffers()
{
	if (_command_buffers.size() > 0) {
		_gpu._logical_device
		    .freeCommandBuffers( _command_pool, ( uint32_t ) _command_buffers.size(), _command_buffers.data() );
	}
	vk::CommandBufferAllocateInfo command_buffer_allocate_info;
	command_buffer_allocate_info.setCommandBufferCount( ( uint32_t ) _swapchain.framebuffers.size() )
	                            .setCommandPool( _command_pool )
	                            .setLevel( vk::CommandBufferLevel::ePrimary );

	_command_buffers = _gpu._logical_device.allocateCommandBuffers( command_buffer_allocate_info );

	for (int i = 0; i < _command_buffers.size(); ++i) {
		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags( vk::CommandBufferUsageFlagBits::eSimultaneousUse );
		_command_buffers[ i ].begin( begin_info );

		vk::ClearColorValue clear_color_value;
		clear_color_value.setFloat32( { 0.0f, 0.0f, 0.0f, 1.0f } );
		vk::ClearValue clear_value( clear_color_value );
		vk::RenderPassBeginInfo render_pass_begin_info;
		render_pass_begin_info.setRenderPass( _renderpass )
		                      .setFramebuffer( _swapchain.framebuffers[ i ] )
		                      .setRenderArea( { { 0, 0 }, _swapchain.chosen_extent } )
		                      .setClearValueCount( 1 )
		                      .setPClearValues( &clear_value );

		_command_buffers[ i ].beginRenderPass( render_pass_begin_info, vk::SubpassContents::eInline );
		_command_buffers[ i ].bindPipeline( vk::PipelineBindPoint::eGraphics, _graphics_pipeline );
		_command_buffers[ i ].bindVertexBuffers( 0, _vertex_buffer, { 0 } );
		_command_buffers[ i ].bindIndexBuffer( _index_buffer, 0, vk::IndexType::eUint16 );
		_command_buffers[i].drawIndexed(_indices.size(), 1, 0, 0, 0);
		_command_buffers[ i ].endRenderPass();
		_command_buffers[ i ].end();
	}
}

void
window::draw_frame()
{
	uint32_t image_index = _gpu._logical_device
	                           .acquireNextImageKHR( _swapchain.swapchain, std::numeric_limits<uint64_t>::max()
	                                                 ,
	                                                 _image_available_sem, VK_NULL_HANDLE
	                           )
	                           .
	                           value;

	vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	vk::SubmitInfo submit_info;
	submit_info.setPWaitSemaphores( &_image_available_sem )
	           .setWaitSemaphoreCount( 1 )
	           .setPWaitDstStageMask( wait_stages )
	           .setCommandBufferCount( 1 )
	           .setPCommandBuffers( &_command_buffers[ image_index ] )
	           .setSignalSemaphoreCount( 1 )
	           .setPSignalSemaphores( &_render_finished_sem );

	_gpu._graphics_queue.submit( submit_info, VK_NULL_HANDLE );

	vk::PresentInfoKHR present_info;
	present_info.setWaitSemaphoreCount( 1 )
	            .setPWaitSemaphores( &_render_finished_sem )
	            .setSwapchainCount( 1 )
	            .setPSwapchains( &_swapchain.swapchain )
	            .setPImageIndices( &image_index );

	_gpu._present_queue.presentKHR( present_info );
}

void
window::create_semaphores()
{
	vk::SemaphoreCreateInfo semaphore_create_info;
	_image_available_sem = _gpu._logical_device.createSemaphore( semaphore_create_info );
	_render_finished_sem = _gpu._logical_device.createSemaphore( semaphore_create_info );
}

void
window::destroy_semaphores()
{
	_gpu._logical_device.destroySemaphore( _image_available_sem );
	_gpu._logical_device.destroySemaphore( _render_finished_sem );
}

void
window::recreate_swap_chain()
{
	_gpu._logical_device.waitIdle();

	destroy_framebuffers();
	destroy_graphics_pipeline();
	destroy_renderpass();
	destroy_image_views();

	create_swapchain();
	create_image_views();
	create_renderpass();
	create_graphics_pipeline();
	create_framebuffers();
	create_command_buffers();
}

void
window::create_index_buffer()
{
	vk::DeviceSize size = sizeof(_indices[ 0 ]) * _indices.size();
	vk::Buffer staging_buffer;
	vk::DeviceMemory staging_memory;
	std::tie( staging_buffer, staging_memory ) = create_buffer( size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible );
	BOOST_SCOPE_EXIT_ALL(&) {

			_gpu._logical_device.freeMemory( staging_memory );
			_gpu._logical_device.destroyBuffer( staging_buffer );
		};

	auto *data = static_cast<decltype(_indices)::pointer>( _gpu._logical_device.mapMemory( staging_memory, 0, size, vk::MemoryMapFlags() ) );
	std::copy( begin( _indices ), end( _indices ), data );
	_gpu._logical_device.unmapMemory( staging_memory );

	std::tie( _index_buffer, _index_buffer_memory ) = create_buffer( size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal );
	copy_buffer( size, staging_buffer, _index_buffer );
}

uint32_t
window::find_memory_type( uint32_t type_filter, vk::MemoryPropertyFlags properties )
{
	const auto memory_properties = _gpu._physical_device.getMemoryProperties();
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
		if (type_filter & (1 << i) && (properties & memory_properties.memoryTypes[ i ].propertyFlags) == properties) {
			return i;
		}
	}
}

std::pair<vk::Buffer, vk::DeviceMemory>
window::create_buffer( vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags mem_props )
{
	std::pair<vk::Buffer, vk::DeviceMemory> res;

	vk::BufferCreateInfo buffer_create_info;
	buffer_create_info.setSize( size )
	                  .setUsage( usage )
	                  .setSharingMode( vk::SharingMode::eExclusive );

	res.first = _gpu._logical_device.createBuffer( buffer_create_info );

	const auto memory_req = _gpu._logical_device.getBufferMemoryRequirements( res.first );
	vk::MemoryAllocateInfo allocate_info;
	allocate_info.setMemoryTypeIndex( find_memory_type( memory_req.memoryTypeBits, mem_props ) )
	             .setAllocationSize( memory_req.size );

	res.second = _gpu._logical_device.allocateMemory( allocate_info );
	_gpu._logical_device.bindBufferMemory( res.first, res.second, 0 );

	return res;
}

void
window::copy_buffer( vk::DeviceSize size, vk::Buffer src_buffer, vk::Buffer dst_buffer )
{
	vk::CommandBufferAllocateInfo command_buffer_allocate_info;
	command_buffer_allocate_info.setCommandBufferCount( 1 )
	                            .setCommandPool( _command_pool )
	                            .setLevel( vk::CommandBufferLevel::ePrimary );
	auto cmd_copy = _gpu._logical_device.allocateCommandBuffers( command_buffer_allocate_info )[ 0 ];
	BOOST_SCOPE_EXIT_ALL(&) {
			_gpu._logical_device.freeCommandBuffers( _command_pool, cmd_copy );
		};
	
	{
		vk::CommandBufferBeginInfo buffer_begin_info;
		buffer_begin_info.setFlags( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );

		cmd_copy.begin( buffer_begin_info );
		vk::BufferCopy copy_info;
		copy_info.setSize( size )
		         .setSrcOffset( 0 )
		         .setDstOffset( 0 );
		cmd_copy.copyBuffer( src_buffer, dst_buffer, copy_info );
		cmd_copy.end();
	}
	vk::SubmitInfo submit_info;
	submit_info.setCommandBufferCount( 1 )
	           .setPCommandBuffers( &cmd_copy );
	_gpu._graphics_queue.submit( submit_info, VK_NULL_HANDLE );
	_gpu._graphics_queue.waitIdle();
}

void
window::create_vertex_buffer()
{
	auto size = sizeof(vertex) * _vertices.size();
	vk::Buffer staging_buffer;
	vk::DeviceMemory staging_memory;
	std::tie( staging_buffer, staging_memory ) = create_buffer( size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible );
	BOOST_SCOPE_EXIT_ALL(&) {
			_gpu._logical_device.freeMemory( staging_memory );
			_gpu._logical_device.destroyBuffer( staging_buffer );
		};

	auto *data = static_cast<vertex *>( _gpu._logical_device.mapMemory( staging_memory, 0, size, vk::MemoryMapFlags() ) );
	std::copy( begin( _vertices ), end( _vertices ), data );
	_gpu._logical_device.unmapMemory( staging_memory );

	std::tie( _vertex_buffer, _vertex_buffer_memory ) = create_buffer( size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal );

	copy_buffer( size, staging_buffer, _vertex_buffer );
}

void
window::destroy_buffers()
{
	_gpu._logical_device.freeMemory( _vertex_buffer_memory );
	_gpu._logical_device.destroyBuffer( _vertex_buffer );

	_gpu._logical_device.freeMemory( _index_buffer_memory );
	_gpu._logical_device.destroyBuffer( _index_buffer );
}
