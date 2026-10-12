// TODO: Part 1b
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#include "FSLogo.h"
#include <vector>
#ifdef _WIN32 // must use MT platform DLL libraries on windows
	#pragma comment(lib, "shaderc_combined.lib") 
#endif

void PrintLabeledDebugString(const char* label, const char* toPrint)
{
	std::cout << label << toPrint << std::endl;
#if defined WIN32 //OutputDebugStringA is a windows-only function 
	OutputDebugStringA(label);
	OutputDebugStringA(toPrint);
#endif
}

class Renderer
{
	// TODO: Part 2b
	// TODO: Part 3a
	
	// proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GVulkanSurface vlk;
	GW::CORE::GEventReceiver shutdown;
	
	// what we need at a minimum to draw a triangle
	VkDevice device = nullptr;
	VkPhysicalDevice physicalDevice = nullptr;
	VkRenderPass renderPass;
	VkBuffer vertexHandle = nullptr;
	VkDeviceMemory vertexData = nullptr;
	// TODO: Part 1g
	VkBuffer indexHandle = nullptr;
	VkDeviceMemory indexData = nullptr;
	// TODO: Part 2c
	VkShaderModule vertexShader = nullptr;
	VkShaderModule fragmentShader = nullptr;
	// pipeline settings for drawing (also required)
	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;
	// TODO: Part 2d
	// TODO: Part 3d
	std::vector<VkBuffer> instanceBuffers;
	std::vector<VkDeviceMemory> instanceBuffersMemory;
	unsigned int windowWidth, windowHeight;

	// TODO: Part 2a
	struct SHADER_SCENE_DATA
	{
		GW::MATH::GVECTORF lightDirection; // Direction the light is pointing
		GW::MATH::GVECTORF lightColor;     // Color/intensity of the light
		GW::MATH::GMATRIXF viewMatrix;     // Camera view matrix
		GW::MATH::GMATRIXF projectionMatrix; // Camera projection matrix
		GW::MATH::GVECTORF eyePosition;    // Camera position for specular calculation
	} sceneData;
	// TODO: Part 2b // TODO: Part 4d
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	VkDescriptorSetLayout descriptorSetLayout = nullptr;
	VkDescriptorPool descriptorPool = nullptr;
	std::vector<VkDescriptorSet> descriptorSets;
	// TODO: Part 3a
	struct INSTANCE_DATA
	{
		GW::MATH::GMATRIXF worldMatrix;
		OBJ_ATTRIBUTES material;
	};
	std::vector<INSTANCE_DATA> perFrame;

public:

	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GVulkanSurface _vlk)
	{
		win = _win;
		vlk = _vlk;
		UpdateWindowDimensions();

		// TODO: Part 2a
		// TODO: Part 2b // TODO: Part 4d
		InitializeSceneData();
		// TODO: part 3a

		InitializeGraphics();
		BindShutdownCallback();
	}

private:
	void UpdateWindowDimensions()
	{
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);
	}

	void InitializeGraphics()
	{
		GetHandlesFromSurface();
		InitializeVertexBuffer();
		InitializeIndexBuffer();
		// TODO: Part 1g
		// TODO: Part 2d // TODO: Part 3d
		InitializeInstanceData();
		CreateDescriptorSetLayout();
		InitializeUniformBuffers();
		InitializeInstanceBuffers();
		CreateDescriptorPool();
		CreateDescriptorSets();
		CompileShaders();
		InitializeGraphicsPipeline();
	}

	void GetHandlesFromSurface()
	{
		vlk.GetDevice((void**)&device);
		vlk.GetPhysicalDevice((void**)&physicalDevice);
		vlk.GetRenderPass((void**)&renderPass);
	}

	void InitializeInstanceBuffers()
	{
		// Get the number of swapchain images
		unsigned int maxFramesInFlight = 0;
		vlk.GetSwapchainImageCount(maxFramesInFlight);

		// Resize the instance buffers vector to hold the correct number of buffers
		instanceBuffers.resize(maxFramesInFlight);
		instanceBuffersMemory.resize(maxFramesInFlight);

		// Calculate the total size needed for instance data
		size_t bufferSize = sizeof(INSTANCE_DATA) * perFrame.size();

		// Create a buffer for each frame
		for (unsigned int i = 0; i < maxFramesInFlight; ++i)
		{
			// Create a storage buffer
			GvkHelper::create_buffer(physicalDevice, device, bufferSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, // Storage buffer this time
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&instanceBuffers[i], &instanceBuffersMemory[i]);

			// Write instance data to the buffer
			GvkHelper::write_to_buffer(device, instanceBuffersMemory[i], perFrame.data(), bufferSize);
		}
	}

	void InitializeInstanceData()
	{
		// Clear any existing instances
		perFrame.clear();

		// Create one instance per mesh in the model
		for (unsigned int i = 0; i < FSLogo_meshcount; ++i)
		{
			INSTANCE_DATA instance;

			// Set up an identity matrix for the world transform
			GW::MATH::GMatrix matrixMath;
			matrixMath.IdentityF(instance.worldMatrix);

			// Set the correct material for this mesh from the model data
			instance.material = FSLogo_materials[i].attrib;

			// Add this instance to our vector
			perFrame.push_back(instance);
		}
	}

	void CreateDescriptorSetLayout()
	{
		// Create bindings for our buffers
		VkDescriptorSetLayoutBinding layoutBindings[2] = {};

		// Binding 0: Uniform buffer (scene data)
		layoutBindings[0].binding = 0;
		layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBindings[0].descriptorCount = 1;
		layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBindings[0].pImmutableSamplers = nullptr;

		// Binding 1: Storage buffer (instance data)
		layoutBindings[1].binding = 1;
		layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		layoutBindings[1].descriptorCount = 1;
		layoutBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBindings[1].pImmutableSamplers = nullptr;

		// Create the descriptor set layout with our bindings
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 2; // Now two bindings
		layoutInfo.pBindings = layoutBindings;

		// Create the descriptor set layout
		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create descriptor set layout!");
		}
	}

	void InitializeUniformBuffers()
	{
		// Get the number of swapchain images
		unsigned int maxFramesInFlight = 0;
		vlk.GetSwapchainImageCount(maxFramesInFlight);

		// Resize the uniform buffers vector to hold the correct number of buffers
		uniformBuffers.resize(maxFramesInFlight);
		uniformBuffersMemory.resize(maxFramesInFlight);

		// Create a buffer for each frame
		for (int i = 0; i < maxFramesInFlight; ++i)
		{
			// Create a uniform buffer for each frame
			GvkHelper::create_buffer(physicalDevice, device,
				sizeof(SHADER_SCENE_DATA),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&uniformBuffers[i], &uniformBuffersMemory[i]);

			// Initialize the uniform buffer with the world matrix data
			SHADER_SCENE_DATA shaderSceneData = {};
			shaderSceneData.viewMatrix = sceneData.viewMatrix;
			shaderSceneData.projectionMatrix = sceneData.projectionMatrix;
			shaderSceneData.lightDirection = sceneData.lightDirection;
			shaderSceneData.lightColor = sceneData.lightColor;

			GvkHelper::write_to_buffer(device, uniformBuffersMemory[i], &shaderSceneData, sizeof(SHADER_SCENE_DATA)); // Transfer data to the uniform buffer
		}
	}

	void CreateDescriptorSets() {
		unsigned int maxFramesInFlight;
		vlk.GetSwapchainImageCount(maxFramesInFlight);

		descriptorSets.resize(maxFramesInFlight);

		std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, descriptorSetLayout);

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = maxFramesInFlight;
		allocInfo.pSetLayouts = layouts.data();

		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate descriptor sets!");
		}

		// Initialize all descriptor sets
		for (unsigned int i = 0; i < maxFramesInFlight; i++) {
			UpdateDescriptorSet(i);
		}
	}

	void UpdateDescriptorSet(unsigned int index)
	{
		// Update for uniform buffer (scene data)
		VkDescriptorBufferInfo uniformBufferInfo = {};
		uniformBufferInfo.buffer = uniformBuffers[index];
		uniformBufferInfo.offset = 0;
		uniformBufferInfo.range = sizeof(SHADER_SCENE_DATA);

		// Update for storage buffer (instance data)
		VkDescriptorBufferInfo storageBufferInfo = {};
		storageBufferInfo.buffer = instanceBuffers[index];
		storageBufferInfo.offset = 0;
		storageBufferInfo.range = sizeof(INSTANCE_DATA) * perFrame.size();

		// Set up the descriptor write operations
		VkWriteDescriptorSet descriptorWrites[2] = {};

		// Write for uniform buffer
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[index];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

		// Write for storage buffer
		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[index];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &storageBufferInfo;

		// Update the descriptor sets
		vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
	}


	void CreateDescriptorPool()
	{
		unsigned int maxFramesInFlight;
		vlk.GetSwapchainImageCount(maxFramesInFlight);

		// Describe the types of descriptor sets we'll create
		VkDescriptorPoolSize poolSizes[2] = {};

		// Uniform buffer descriptors
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = maxFramesInFlight;

		// Storage buffer descriptors
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[1].descriptorCount = maxFramesInFlight;

		// Setup the descriptor pool create info
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 2; // Now two types of descriptors
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = maxFramesInFlight;
		poolInfo.flags = 0;

		// Create the descriptor pool
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create descriptor pool!");
		}
	}

	void InitializeSceneData()
	{
		// Setup camera view matrix
		// Camera at (0.75, 0.25, -1.5) looking at (0.15, 0.75, 0)
		GW::MATH::GVECTORF eye = { 0.75f, 0.25f, -1.5f, 1.0f };
		GW::MATH::GVECTORF at = { 0.15f, 0.75f, 0.0f, 1.0f };
		GW::MATH::GVECTORF up = { 0.0f, 1.0f, 0.0f, 0.0f };

		// Store eye position for specular calculations
		sceneData.eyePosition = eye;

		GW::MATH::GMatrix matrixMath;
		matrixMath.LookAtLHF(eye, at, up, sceneData.viewMatrix);

		// Setup projection matrix with 65-degree FOV
		float aspectRatio = (float)windowWidth / windowHeight;
		matrixMath.ProjectionVulkanLHF(
			G_DEGREE_TO_RADIAN(65.0f), // 65 degrees vertical FOV
			aspectRatio,
			0.1f,   // Near plane
			100.0f, // Far plane
			sceneData.projectionMatrix);

		// Setup light direction (-1, -1, 2) normalized
		sceneData.lightDirection = { -1.0f, -1.0f, 2.0f, 0.0f };
		GW::MATH::GVector vectorMath;
		vectorMath.NormalizeF(sceneData.lightDirection, sceneData.lightDirection);

		// Set light color to slightly bluish white
		sceneData.lightColor = { 0.9f, 0.9f, 1.0f, 1.0f };
	}

	void InitializeVertexBuffer()
	{
		// TODO: Part 1c
		// Use the vertex data from FSLogo.h instead of the hardcoded triangle
		const unsigned int vertexCount = FSLogo_vertexcount;

		CreateVertexBuffer(FSLogo_vertices, sizeof(OBJ_VERT) * vertexCount);

	}

	void CreateVertexBuffer(const void* data, unsigned int sizeInBytes)
	{
		GvkHelper::create_buffer(physicalDevice, device, sizeInBytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertexHandle, &vertexData);
		// Transfer triangle data to the vertex buffer. (staging would be prefered here)
		GvkHelper::write_to_buffer(device, vertexData, data, sizeInBytes);
	}

	void InitializeIndexBuffer()
	{
		// Use the index data from FSLogo.h
		const unsigned int indexCount = FSLogo_indexcount;

		// Pass the data directly to CreateIndexBuffer
		CreateIndexBuffer(FSLogo_indices, sizeof(unsigned int) * indexCount);
	}

	void CreateIndexBuffer(const void* data, unsigned int sizeInBytes)
	{
		GvkHelper::create_buffer(physicalDevice, device, sizeInBytes,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &indexHandle, &indexData);

		// Transfer index data to the index buffer
		GvkHelper::write_to_buffer(device, indexData, data, sizeInBytes);
	}

	void CompileShaders()
	{
		// Intialize runtime shader compiler HLSL -> SPIRV
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compile_options_t options = CreateCompileOptions();

		CompileVertexShader(compiler, options);
		CompilePixelShader(compiler, options);

		// Free runtime shader compiler resources
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);
	}

	shaderc_compile_options_t CreateCompileOptions()
	{
		shaderc_compile_options_t retval = shaderc_compile_options_initialize();
		shaderc_compile_options_set_source_language(retval, shaderc_source_language_hlsl);
		shaderc_compile_options_set_invert_y(retval, false); // TODO: Part 2g
#ifndef NDEBUG
		shaderc_compile_options_set_generate_debug_info(retval);
#endif
		return retval;	
	}

	void CompileVertexShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
	{
		std::string vertexShaderSource = ReadFileIntoString("../VertexShader.hlsl");

		shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
			compiler, vertexShaderSource.c_str(), vertexShaderSource.length(),
			shaderc_vertex_shader, "main.vert", "main", options);
		
		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			PrintLabeledDebugString("Vertex Shader Errors:\n", shaderc_result_get_error_message(result));
			abort();
			return;
		}
		
		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vertexShader);
		
		shaderc_result_release(result); // done
	}
	
	void CompilePixelShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
	{
		std::string fragmentShaderSource = ReadFileIntoString("../FragmentShader.hlsl");
		
		shaderc_compilation_result_t result;
		
		result = shaderc_compile_into_spv( // compile
			compiler, fragmentShaderSource.c_str(), fragmentShaderSource.length(),
			shaderc_fragment_shader, "main.frag", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			PrintLabeledDebugString("Fragment Shader Errors:\n", shaderc_result_get_error_message(result));
			abort();
			return;
		}
		
		GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &fragmentShader);
		
		shaderc_result_release(result); // done
	}

	void InitializeGraphicsPipeline()
	{
		// Create Pipeline & Layout (Thanks Tiny!)
		VkPipelineShaderStageCreateInfo stage_create_info[2] = {};
		// Create Stage Info for Vertex Shader
		stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_create_info[0].module = vertexShader;
		stage_create_info[0].pName = "main";

		// Create Stage Info for Fragment Shader
		stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_create_info[1].module = fragmentShader;
		stage_create_info[1].pName = "main";

		VkPipelineInputAssemblyStateCreateInfo assembly_create_info = CreateVkPipelineInputAssemblyStateCreateInfo();
		VkVertexInputBindingDescription vertex_binding_description = CreateVkVertexInputBindingDescription();

		VkVertexInputAttributeDescription vertex_attribute_description[3];

		// Position attribute (x, y, z)
		vertex_attribute_description[0].binding = 0;
		vertex_attribute_description[0].location = 0;
		vertex_attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT; // xyz
		vertex_attribute_description[0].offset = offsetof(OBJ_VERT, pos);

		// Texture coordinates attribute (u, v, w)
		vertex_attribute_description[1].binding = 0;
		vertex_attribute_description[1].location = 1;
		vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT; // uvw
		vertex_attribute_description[1].offset = offsetof(OBJ_VERT, uvw);

		// Normal attribute (x, y, z)
		vertex_attribute_description[2].binding = 0;
		vertex_attribute_description[2].location = 2;
		vertex_attribute_description[2].format = VK_FORMAT_R32G32B32_SFLOAT; // xyz
		vertex_attribute_description[2].offset = offsetof(OBJ_VERT, nrm);

		VkPipelineVertexInputStateCreateInfo input_vertex_info = CreateVkPipelineVertexInputStateCreateInfo(
			&vertex_binding_description, 1, vertex_attribute_description, 3);
		
		VkViewport viewport = CreateViewportFromWindowDimensions();
		VkRect2D scissor = CreateScissorFromWindowDimensions(); 
		VkPipelineViewportStateCreateInfo viewport_create_info = CreateVkPipelineViewportStateCreateInfo(&viewport, 1, &scissor, 1);
		VkPipelineRasterizationStateCreateInfo rasterization_create_info = CreateVkPipelineRasterizationStateCreateInfo();
		VkPipelineMultisampleStateCreateInfo multisample_create_info = CreateVkPipelineMultisampleStateCreateInfo();
		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = CreateVkPipelineDepthStencilStateCreateInfo();
		VkPipelineColorBlendAttachmentState color_blend_attachment_state = CreateVkPipelineColorBlendAttachmentState();
		VkPipelineColorBlendStateCreateInfo color_blend_create_info = CreateVkPipelineColorBlendStateCreateInfo(&color_blend_attachment_state, 1);

		// Dynamic State 
		VkDynamicState dynamic_states[2] = 
		{
			// By setting these we do not need to re-create the pipeline on Resize
			VK_DYNAMIC_STATE_VIEWPORT, 
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_create_info = CreateVkPipelineDynamicStateCreateInfo(dynamic_states, 2);

		CreatePipelineLayout();

		// Pipeline State... (FINALLY) 
		VkGraphicsPipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = stage_create_info;
		pipeline_create_info.pInputAssemblyState = &assembly_create_info;
		pipeline_create_info.pVertexInputState = &input_vertex_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.pDynamicState = &dynamic_create_info;
		pipeline_create_info.layout = pipelineLayout;
		pipeline_create_info.renderPass = renderPass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

		vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline);
	}

	VkPipelineInputAssemblyStateCreateInfo CreateVkPipelineInputAssemblyStateCreateInfo()
	{
		VkPipelineInputAssemblyStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		retval.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		retval.primitiveRestartEnable = false;
		return retval;
	}

	VkVertexInputBindingDescription CreateVkVertexInputBindingDescription()
	{
		// TODO: Part 1e
		VkVertexInputBindingDescription retval = {};
		retval.binding = 0;
		retval.stride = sizeof(OBJ_VERT);
		retval.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return retval;
	}

	VkPipelineVertexInputStateCreateInfo CreateVkPipelineVertexInputStateCreateInfo(
		VkVertexInputBindingDescription* bindingDescriptions, uint32_t bindingCount,
		VkVertexInputAttributeDescription* attributeDescriptions, uint32_t attributeCount)
	{
		VkPipelineVertexInputStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		retval.vertexBindingDescriptionCount = bindingCount;
		retval.pVertexBindingDescriptions = bindingDescriptions;
		retval.vertexAttributeDescriptionCount = attributeCount;
		retval.pVertexAttributeDescriptions = attributeDescriptions;
		return retval;
	}

	VkViewport CreateViewportFromWindowDimensions()
	{
		VkViewport retval = {};
		retval.x = 0;
		retval.y = 0;
		retval.width = static_cast<float>(windowWidth);
		retval.height = static_cast<float>(windowHeight);
		retval.minDepth = 0;
		retval.maxDepth = 1;
		return retval;
	}

	VkRect2D CreateScissorFromWindowDimensions()
	{
		VkRect2D retval = {};
		retval.offset.x = 0;
		retval.offset.y = 0;
		retval.extent.width = windowWidth;
		retval.extent.height = windowHeight;
		return retval;
	}

	VkPipelineViewportStateCreateInfo CreateVkPipelineViewportStateCreateInfo(VkViewport* viewports, uint32_t viewportCount, VkRect2D* scissors, uint32_t scissorCount)
	{
		VkPipelineViewportStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		retval.viewportCount = viewportCount;
		retval.pViewports = viewports;
		retval.scissorCount = scissorCount;
		retval.pScissors = scissors;
		return retval;
	}

	VkPipelineRasterizationStateCreateInfo CreateVkPipelineRasterizationStateCreateInfo()
	{
		VkPipelineRasterizationStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		retval.rasterizerDiscardEnable = VK_FALSE;
		retval.polygonMode = VK_POLYGON_MODE_FILL;
		retval.lineWidth = 1.0f;
		retval.cullMode = VK_CULL_MODE_BACK_BIT;
		retval.frontFace = VK_FRONT_FACE_CLOCKWISE;
		retval.depthClampEnable = VK_FALSE;
		retval.depthBiasEnable = VK_FALSE;
		retval.depthBiasClamp = 0.0f;
		retval.depthBiasConstantFactor = 0.0f;
		retval.depthBiasSlopeFactor = 0.0f;
		return retval;
	}

	VkPipelineMultisampleStateCreateInfo CreateVkPipelineMultisampleStateCreateInfo()
	{
		VkPipelineMultisampleStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		retval.sampleShadingEnable = VK_FALSE;
		retval.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		retval.minSampleShading = 1.0f;
		retval.pSampleMask = VK_NULL_HANDLE;
		retval.alphaToCoverageEnable = VK_FALSE;
		retval.alphaToOneEnable = VK_FALSE;
		return retval;
	}

	VkPipelineDepthStencilStateCreateInfo CreateVkPipelineDepthStencilStateCreateInfo()
	{
		VkPipelineDepthStencilStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		retval.depthTestEnable = VK_TRUE;
		retval.depthWriteEnable = VK_TRUE;
		retval.depthCompareOp = VK_COMPARE_OP_LESS;
		retval.depthBoundsTestEnable = VK_FALSE;
		retval.minDepthBounds = 0.0f;
		retval.maxDepthBounds = 1.0f;
		retval.stencilTestEnable = VK_FALSE;
		return retval;
	}

	VkPipelineColorBlendAttachmentState CreateVkPipelineColorBlendAttachmentState()
	{
		VkPipelineColorBlendAttachmentState retval = {};
		retval.colorWriteMask = 0xF;
		retval.blendEnable = VK_FALSE;
		retval.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
		retval.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
		retval.colorBlendOp = VK_BLEND_OP_ADD;
		retval.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		retval.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		retval.alphaBlendOp = VK_BLEND_OP_ADD;
		return retval;
	}

	VkPipelineColorBlendStateCreateInfo CreateVkPipelineColorBlendStateCreateInfo(VkPipelineColorBlendAttachmentState* attachmentStates, uint32_t attachmentCount)
	{
		VkPipelineColorBlendStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		retval.logicOpEnable = VK_FALSE;
		retval.logicOp = VK_LOGIC_OP_COPY;
		retval.attachmentCount = attachmentCount;
		retval.pAttachments = attachmentStates;
		retval.blendConstants[0] = 0.0f;
		retval.blendConstants[1] = 0.0f;
		retval.blendConstants[2] = 0.0f;
		retval.blendConstants[3] = 0.0f;
		return retval;
	}

	VkPipelineDynamicStateCreateInfo CreateVkPipelineDynamicStateCreateInfo(VkDynamicState* dynamicStates, uint32_t dynamicStateCount)
	{
		VkPipelineDynamicStateCreateInfo retval = {};
		retval.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		retval.dynamicStateCount = dynamicStateCount;
		retval.pDynamicStates = dynamicStates;
		return retval;
	}

	void CreatePipelineLayout()
	{
		// Descriptor pipeline layout
		// TODO: Part 2d // TODO: Part 3d
		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 1;
		pipeline_layout_create_info.pSetLayouts = &descriptorSetLayout;
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.pPushConstantRanges = nullptr;

		vkCreatePipelineLayout(device, &pipeline_layout_create_info,nullptr, &pipelineLayout);
	}

	void BindShutdownCallback()
	{
		// GVulkanSurface will inform us when to release any allocated resources
		shutdown.Create(vlk, [&]() {
			if (+shutdown.Find(GW::GRAPHICS::GVulkanSurface::Events::RELEASE_RESOURCES, true)) {
				CleanUp(); // unlike D3D we must be careful about destroy timing
			}
			});
	}


public:
		// TODO: Part 3i
		// TODO: Part 2a
		// TODO: Part 3i
		// TODO: Part 1h
		// TODO: Part 2e
		// TODO: Part 3f
	void Render()
	{
		// Calculate delta time between frames
		static float lastFrameTime = 0.0f;
		float currentTime = static_cast<float>(GetTickCount64()) / 1000.0f;
		float deltaTime = currentTime - lastFrameTime;
		lastFrameTime = currentTime;

		// Track total time for rotation
		static float totalTime = 0.0f;
		totalTime += deltaTime;

		// Create a rotation matrix for the orange logo (first mesh)
		GW::MATH::GMatrix matrixMath;
		GW::MATH::GMATRIXF rotationMatrix;

		// Create a rotation around the Y-axis based on time
		matrixMath.RotationYawPitchRollF(
			totalTime * -1.0f,  // Yaw (rotate around Y-axis)
			0.0f,              // Pitch 
			0.0f,              // Roll
			rotationMatrix);

		// Apply the rotation to the first instance (orange logo)
		if (!perFrame.empty()) {
			// Check if we have at least one instance
			perFrame[1].worldMatrix = rotationMatrix;

			// Write the updated instance data to the GPU buffer
			unsigned int currentBuffer;
			vlk.GetSwapchainCurrentImage(currentBuffer);

			// Update the instance buffer with new rotated matrix
			size_t bufferSize = sizeof(INSTANCE_DATA) * perFrame.size();
			GvkHelper::write_to_buffer(device, instanceBuffersMemory[currentBuffer],
				perFrame.data(), bufferSize);
		}

		// Continue with normal rendering
		VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
		SetUpPipeline(commandBuffer);

		SHADER_SCENE_DATA shaderSceneData = {};
		shaderSceneData.viewMatrix = sceneData.viewMatrix;
		shaderSceneData.projectionMatrix = sceneData.projectionMatrix;
		shaderSceneData.lightDirection = sceneData.lightDirection;
		shaderSceneData.lightColor = sceneData.lightColor;
		shaderSceneData.eyePosition = sceneData.eyePosition;

		unsigned int currentBuffer;
		vlk.GetSwapchainCurrentImage(currentBuffer);
		GvkHelper::write_to_buffer(device, uniformBuffersMemory[currentBuffer],
			&shaderSceneData, sizeof(SHADER_SCENE_DATA));

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);

		// Bind index buffer
		vkCmdBindIndexBuffer(commandBuffer, indexHandle, 0, VK_INDEX_TYPE_UINT32);

		// Get the number of instances per mesh
		unsigned int instanceCount = perFrame.size() / FSLogo_meshcount;

		// Render each mesh with the correct instance count
		for (unsigned int i = 0; i < FSLogo_meshcount; i++) {
			vkCmdDrawIndexed(commandBuffer,
				FSLogo_meshes[i].indexCount,  // Index count for this mesh
				instanceCount,                // Instance count
				FSLogo_meshes[i].indexOffset, // Starting index for this mesh
				0,                            // Vertex offset
				i * instanceCount);           // First instance
		}
	}

private:

	VkCommandBuffer GetCurrentCommandBuffer()
	{
		VkCommandBuffer retval;
		unsigned int currentBuffer;
		vlk.GetSwapchainCurrentImage(currentBuffer);
		vlk.GetCommandBuffer(currentBuffer, (void**)&retval);
		return retval;
	}

	void SetUpPipeline(VkCommandBuffer& commandBuffer)
	{
		UpdateWindowDimensions(); // what is the current client area dimensions?
		SetViewport(commandBuffer);
		SetScissor(commandBuffer);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		BindVertexBuffers(commandBuffer);
	}

	void SetViewport(const VkCommandBuffer& commandBuffer)
	{
		VkViewport viewport = CreateViewportFromWindowDimensions();
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	}

	void SetScissor(const VkCommandBuffer& commandBuffer)
	{
		VkRect2D scissor = CreateScissorFromWindowDimensions();
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void BindVertexBuffers(VkCommandBuffer& commandBuffer)
	{
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexHandle, offsets);
	}


	void CleanUp()
	{
		// wait till everything has completed
		vkDeviceWaitIdle(device);
		// Release allocated buffers, shaders & pipeline
		// TODO: Part 1g
		// TODO: Part 2d
		// TODO: Part 3d

		// Clean up instance buffers
		for (size_t i = 0; i < instanceBuffers.size(); ++i)
		{
			vkDestroyBuffer(device, instanceBuffers[i], nullptr);
			vkFreeMemory(device, instanceBuffersMemory[i], nullptr);
		}

		// Original cleanup code
		for (size_t i = 0; i < uniformBuffers.size(); ++i)
		{
			vkDestroyBuffer(device, uniformBuffers[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
		}

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyBuffer(device, vertexHandle, nullptr);
		vkFreeMemory(device, vertexData, nullptr);
		vkDestroyBuffer(device, indexHandle, nullptr);
		vkFreeMemory(device, indexData, nullptr);
		vkDestroyShaderModule(device, vertexShader, nullptr);
		vkDestroyShaderModule(device, fragmentShader, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
	}
};
