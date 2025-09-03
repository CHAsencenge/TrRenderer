#include "TrVulkanRendererRayTracing.h"

#include <corecrt_io.h>



TrVulkanRendererRayTracingBase::TrVulkanRendererRayTracingBase()
{
}

TrVulkanRendererRayTracingBase::TrVulkanRendererRayTracingBase(uint32_t width, uint32_t height, const char* title) :
TrVulkanRendererBase(width, height, title)
{
    mPushConstantRay.mNumRayGenSamples = 10;
}

void TrVulkanRendererRayTracingBase::Run()
{
    OnInitWindow();
    OnInitVulkan();
    OnRender();
    OnCleanup();
}

void TrVulkanRendererRayTracingBase::OnInitWindow()
{
    TrVulkanRendererBase::OnInitWindow();

    SetupCamera();
}

void TrVulkanRendererRayTracingBase::OnInitVulkan()
{
    // create application
    CreateInstance();

    // can GCT queue (graphics, compute, transter) present on the surface
    IsSurfaceSupportPresent();

    // resource allocator, debug util, depth format
    setup(mNvContext.m_instance, mNvContext.m_device, mNvContext.m_physicalDevice, mNvContext.m_queueGCT.familyIndex);
    
    // swap chain: for rendering onto the surface
    createSwapchain(mSurface, mWidth, mHeight);

    // depth buffer
    createDepthBuffer();

    // default simple render pass
    createRenderPass();

    // frame buffers, in which the image will be rendered
    createFrameBuffers();

    // imgui using sub pass 0
    initGUI(0);

    // load model
    // LoadModel(nvh::findFile("media/scenes/cube_multi.obj", TrVulkanGlobalRT::defaultSearchPaths, true));
    LoadModel(nvh::findFile("media/scenes/Medieval_building.obj", TrVulkanGlobalRT::defaultSearchPaths, true));
    LoadModel(nvh::findFile("media/scenes/plane.obj", TrVulkanGlobalRT::defaultSearchPaths, true));
    LoadModel(nvh::findFile("media/scenes/wuson.obj", TrVulkanGlobalRT::defaultSearchPaths, true), glm::translate(glm::mat4(1),glm::vec3(0.0f, 0.0f, 12.0f)));
    LoadModel(nvh::findFile("media/scenes/sphere.obj", TrVulkanGlobalRT::defaultSearchPaths, true), glm::scale(glm::mat4(1.f),glm::vec3(1.5f)) * glm::translate(glm::mat4(1),glm::vec3(0.0f, 0.0f, 10.0f)));

    // offscreen render
    CreateOffscreenRender();

    // descriptor set layout
    CreateDescriptorSetLayout();

    // pipeline
    CreateGraphicsPipeline();

    // uniform buffer
    CreateUniformBuffer();

    // obj buffer
    CreateObjDescriptionBuffer();

    // update descriptor set
    UpdateDescriptorSet();

    /// simple begin: RT init
    InitRayTracing();
    CreateBLAS();
    CreateTLAS();
    CreateRtDescriptorSet();
    CreateRtPipeline();
    CreateSBT();
    
    /// simple end 

    // post descriptor
    CreatePostDescriptor();
    
    // post pipeline
    CreatePostPipeline();

    // update post descriptor set
    UpdatePostDescriptorSet();

    // imgui init for vulkan
    setupGlfwCallbacks(mWindow); // response to cursor drag events
    ImGui_ImplGlfw_InitForVulkan(mWindow, true);
}

void TrVulkanRendererRayTracingBase::OnRender()
{
    while(!glfwWindowShouldClose(mWindow))
    {
        glfwPollEvents();

        if(isMinimized())
        {
            continue;
        }
    
        // imgui new frame
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // imgui style
        glm::vec4 clearColor = glm::vec4(1, 1, 1, 1.00f);
        if(showGui())
        {
            RenderUI(clearColor);
        }

        // prepare rendering scene
        prepareFrame();

        auto curFrame = getCurFrame();
        const VkCommandBuffer& cmdBuf = getCommandBuffers()[curFrame];

        VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuf, &cmdBufferBeginInfo);
        
        // update uniform buffer
        UpdateUniformBuffer(cmdBuf);

        // clear screen
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color        = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
        clearValues[1].depthStencil = {1.0f, 0};

        // offscreen render pass
        VkRenderPassBeginInfo offscreenRenderPassBeginInfo {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        offscreenRenderPassBeginInfo.framebuffer = mOffscreenFrameBuffer;
        offscreenRenderPassBeginInfo.renderArea = {{0, 0}, getSize()};
        offscreenRenderPassBeginInfo.renderPass = mOffscreenRenderPass;
        offscreenRenderPassBeginInfo.clearValueCount = 2;
        offscreenRenderPassBeginInfo.pClearValues = clearValues.data();

        // rendering scene
        if(mbUseRayTracer)
        {
            RayTrace(cmdBuf, clearColor);
        }
        else
        {
            vkCmdBeginRenderPass(cmdBuf, &offscreenRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            Rasterize(cmdBuf);
            vkCmdEndRenderPass(cmdBuf);
        }
        
        // post render pass
        VkRenderPassBeginInfo postRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        postRenderPassBeginInfo.framebuffer = getFramebuffers()[curFrame];
        postRenderPassBeginInfo.renderArea = {{0, 0}, getSize()};
        postRenderPassBeginInfo.renderPass = getRenderPass();
        postRenderPassBeginInfo.clearValueCount = 2;
        postRenderPassBeginInfo.pClearValues = clearValues.data();
        
        vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        DrawPost(cmdBuf);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

        vkCmdEndRenderPass(cmdBuf);

        vkEndCommandBuffer(cmdBuf);
        submitFrame();
    }
}

void TrVulkanRendererRayTracingBase::OnCleanup()
{
    vkDeviceWaitIdle(getDevice());
    DestroyResources();
    destroy();
    mNvContext.deinit();
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

void TrVulkanRendererRayTracingBase::CreateInstance()
{
    // extensions and layers
    std::vector<const char*> extensions = GetRequiredExtensions();
    
    // input: extensions, layers, version, 
    nvvk::ContextCreateInfo contextCreateInfo;
    contextCreateInfo.setVersion(1, 2);
    
    for(const char* extension : extensions)
    {
        contextCreateInfo.addInstanceExtension(extension, true);
    }

    for(const char* extension : TrVulkanGlobalRT::deviceExtensions)
    {
        contextCreateInfo.addDeviceExtension(extension);
    }

    /// simple begin: activate the ray tracing extension
    // feature structures
    // physical device will place the structs on the pNext chain of CreateInfo before calling CreateDevice
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR}; 
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    contextCreateInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);
    contextCreateInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);
    contextCreateInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // required by ray tracing pipeline
    /// simple end
    
    for(const char* layer : TrVulkanGlobalRT::layers)
    {
        contextCreateInfo.addInstanceLayer(layer, true);
    }

    // create vulkan instance
    mNvContext.initInstance(contextCreateInfo);

    // create device
    std::vector<uint32_t> deviceIndices = mNvContext.getCompatibleDevices(contextCreateInfo);
    assert(!deviceIndices.empty());
    mNvContext.initDevice(deviceIndices[0], contextCreateInfo);

    
}

std::vector<const char*> TrVulkanRendererRayTracingBase::GetRequiredExtensions()
{
    std::vector<const char*> extensions = TrVulkanRendererBase::GetRequiredExtensions();
    return extensions;
}

void TrVulkanRendererRayTracingBase::setup(const VkInstance& instance, const VkDevice& device,
    const VkPhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex)
{
    // both AppBaseVk and nvvk::Context have these references
    AppBaseVk::setup(instance, device, physicalDevice, graphicsQueueIndex);
    mResourceAllocDma.init(instance, device, physicalDevice);
    mDebugger.setup(device);
    mOffscreenDepthFormat = nvvk::findDepthFormat(physicalDevice);
}

void TrVulkanRendererRayTracingBase::IsSurfaceSupportPresent()
{
    mSurface = getVkSurface(mNvContext.m_instance, mWindow);
    // determine whether a queue family of a physical device supports presentation to a given surface
    bool bSupportPresent = mNvContext.setGCTQueueWithPresent(mSurface);
    assert(bSupportPresent);
}

void TrVulkanRendererRayTracingBase::SetupCamera()
{
    CameraManip.setWindowSize(mWidth, mHeight);
    CameraManip.setLookat(mEye, mCenter, mUp);
    
}

void TrVulkanRendererRayTracingBase::LoadModel(const std::string& filename, glm::mat4 transform)
{
    ObjLoader objLoader;
    objLoader.loadModel(filename);

    // material tone mapping, from srgb to linear
    for(MaterialObj mat : objLoader.m_materials)
    {
        mat.diffuse = glm::pow(mat.diffuse, glm::vec3(2.2f));
        mat.specular = glm::pow(mat.specular, glm::vec3(2.2f));
        mat.ambient= glm::pow(mat.ambient, glm::vec3(2.2f));
    }

    // create buffers for model vertices, indices, material colors, material indices
    nvvk::CommandPool cmdPool(m_device, m_graphicsQueueIndex);
    VkCommandBuffer cmdBuffer = cmdPool.createCommandBuffer();
    
    TrObjModelRtBase model;
    model.mNumIndices = static_cast<uint32_t>(objLoader.m_indices.size());
    model.mNumVertices = static_cast<uint32_t>(objLoader.m_vertices.size());

    // use vkGetBufferDeviceAddress can retrieve buffer, and can use that address to access buffer's memory from a shader
    VkBufferUsageFlags flag   = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    /// simple begin
    // // used also for building acceleration structures
    VkBufferUsageFlags rayTracingFlags = flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    /// simple end
    // VK_BUFFER_USAGE_VERTEX_BUFFER_BIT: buffer is suitable for passing to vkCmdBindVertexBuffers
    model.mVertexBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
    // VK_BUFFER_USAGE_INDEX_BUFFER_BIT: buffer is suitable for passing to vkCmdBindIndexBuffer
    model.mIndexBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
    // VK_BUFFER_USAGE_STORAGE_BUFFER_BIT: buffer can be used in VkDescriptorBufferInfo
    // suitable for occupying VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC VkDescriptorSet slot
    model.mMatColorBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
    model.mMatIndexBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_matIndx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);

    // create all textures (nvvk::Texture) for the model, if no textures, create a dummy texture
    auto texOffset = static_cast<uint32_t>(mTextures.size());
    CreateTextureImages(cmdBuffer, objLoader.m_textures);

    // submit and wait
    cmdPool.submitAndWait(cmdBuffer);

    // staging memory release
    mResourceAllocDma.finalizeAndReleaseStaging();


    // debug obj name
    std::string objNb = std::to_string(mObjModels.size());
    mDebugger.setObjectName(model.mVertexBuffer.buffer, (std::string("vertex_" + objNb)));
    mDebugger.setObjectName(model.mIndexBuffer.buffer, (std::string("index_" + objNb)));
    mDebugger.setObjectName(model.mMatColorBuffer.buffer, (std::string("mat_" + objNb)));
    mDebugger.setObjectName(model.mMatIndexBuffer.buffer, (std::string("matIdx_" + objNb)));

    // set obj instance transform
    TrObjInstanceRtBase instance;
    instance.mTransform = transform;
    instance.mObjIndex = static_cast<uint32_t>(mObjModels.size());
    mObjInstances.push_back(instance);

    TrObjDescRtBase objDesc;
    objDesc.mTexOffset = texOffset;
    objDesc.mVertexAddress = nvvk::getBufferDeviceAddress(m_device, model.mVertexBuffer.buffer);
    objDesc.mIndexAddress = nvvk::getBufferDeviceAddress(m_device, model.mIndexBuffer.buffer);
    objDesc.mMaterialAddress = nvvk::getBufferDeviceAddress(m_device, model.mMatColorBuffer.buffer);
    objDesc.mMaterialIndexAddress = nvvk::getBufferDeviceAddress(m_device, model.mMatIndexBuffer.buffer);

    mObjModels.emplace_back(model);
    mObjDescs.emplace_back(objDesc);
    
}

void TrVulkanRendererRayTracingBase::CreateTextureImages(const VkCommandBuffer cmdBuffer, const std::vector<std::string> textures)
{
    // need: sampler create info, image create info
    VkSamplerCreateInfo samplerCreateInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.maxLod = FLT_MAX;

    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    
    // if no textures are present, create a dummy texture to accomodate the pipeline layout
    if(textures.empty() && mTextures.empty())
    {
        nvvk::Texture texture;
        
        std::array<uint8_t, 4> color{255u, 255u, 255u, 255u}; // u means unsigned
        VkDeviceSize size = sizeof(color);
        auto imgExtent = VkExtent2D{1, 1};
        VkImageCreateInfo imgCreateInfo = nvvk::makeImage2DCreateInfo(imgExtent, format);

        nvvk::Image image = mResourceAllocDma.createImage(cmdBuffer, size, color.data(), imgCreateInfo);
        VkImageViewCreateInfo imgViewCreateInfo = nvvk::makeImageViewCreateInfo(image.image, imgCreateInfo);
        texture = mResourceAllocDma.createTexture(image, imgViewCreateInfo, samplerCreateInfo);

        // when createImage, image layout is VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        nvvk::cmdBarrierImageLayout(cmdBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        mTextures.push_back(texture);
    }
    else
    {
        for(const auto& texture : textures)
        {
            // find texture file
            std::string path = "media/textures/" + texture;
            int texWidth, texHeight, texChannels;
            std::string texFile = nvh::findFile(path, TrVulkanGlobalRT::defaultSearchPaths, true);
            std::cout << "TrVulkanRendererRayTracingBase::CreateTextureImages: " << texture << " " << texFile << std::endl;

            // load file (unsigned char)
            std::array<stbi_uc, 4> color{255u, 0u, 255u, 255u};
            stbi_uc* pixels = stbi_load(texFile.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            if(!pixels)
            {
                texWidth = texHeight = 1;
                texChannels = 4;
                pixels = color.data();
            }
            
            // compute buffer size, img extent, img create info
            VkDeviceSize bufferSize = static_cast<uint64_t>(texWidth) * texHeight * sizeof(uint8_t) * 4;
            auto imgExtent = VkExtent2D{(uint32_t)texWidth, (uint32_t)texHeight};
            auto imgCreateInfo = nvvk::makeImage2DCreateInfo(imgExtent, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

            // create texture
            nvvk::Image image = mResourceAllocDma.createImage(cmdBuffer, bufferSize, pixels, imgCreateInfo);
            nvvk::cmdGenerateMipmaps(cmdBuffer, image.image, format, imgExtent, imgCreateInfo.mipLevels);
            VkImageViewCreateInfo imgViewCreateInfo = nvvk::makeImageViewCreateInfo(image.image, imgCreateInfo);
            nvvk::Texture nvvkTexture = mResourceAllocDma.createTexture(image, imgViewCreateInfo, samplerCreateInfo);
            mTextures.push_back(nvvkTexture);
            stbi_image_free(pixels);
        }
    }
}

void TrVulkanRendererRayTracingBase::CreateOffscreenRender()
{
    mResourceAllocDma.destroy(mOffscreenColorTex);
    mResourceAllocDma.destroy(mOffscreenDepthTex);
    // create color image
    VkImageCreateInfo colorCreateInfo = nvvk::makeImage2DCreateInfo(m_size, mOffscreenColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    nvvk::Image colorImage = mResourceAllocDma.createImage(colorCreateInfo);
    VkImageViewCreateInfo colorViewCreateInfo = nvvk::makeImageViewCreateInfo(colorImage.image, colorCreateInfo);
    VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    mOffscreenColorTex = mResourceAllocDma.createTexture(colorImage, colorViewCreateInfo, samplerCreateInfo);
    mOffscreenColorTex.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    
    // create depth buffer
    VkImageCreateInfo depthCreateInfo = nvvk::makeImage2DCreateInfo(m_size, mOffscreenDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    nvvk::Image depthImage = mResourceAllocDma.createImage(depthCreateInfo);
    VkImageViewCreateInfo depthViewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthViewCreateInfo.format = mOffscreenDepthFormat;
    depthViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewCreateInfo.image = depthImage.image;
    depthViewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    mOffscreenDepthTex = mResourceAllocDma.createTexture(depthImage, depthViewCreateInfo);
    
    // set image layout for color and depth
    nvvk::CommandPool cmdPool(m_device, m_graphicsQueueIndex);
    auto cmdBuf = cmdPool.createCommandBuffer();
    nvvk::cmdBarrierImageLayout(cmdBuf, mOffscreenColorTex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, mOffscreenDepthTex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    cmdPool.submitAndWait(cmdBuf);
    
    // create render pass for offscreen
    if(!mOffscreenRenderPass)
    {
        mOffscreenRenderPass = nvvk::createRenderPass(m_device, {mOffscreenColorFormat}, mOffscreenDepthFormat, 1, true, true, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
    }
    
    // create frame buffer for offscreen
    vkDestroyFramebuffer(m_device, mOffscreenFrameBuffer, nullptr);
    
    std::vector<VkImageView> imageViews = {mOffscreenColorTex.descriptor.imageView, mOffscreenDepthTex.descriptor.imageView};
    VkFramebufferCreateInfo frameBufferCreateInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(imageViews.size());
    frameBufferCreateInfo.pAttachments = imageViews.data();
    frameBufferCreateInfo.renderPass = mOffscreenRenderPass;
    frameBufferCreateInfo.width = m_size.width;
    frameBufferCreateInfo.height = m_size.height;
    frameBufferCreateInfo.layers = 1;

    vkCreateFramebuffer(m_device, &frameBufferCreateInfo, nullptr, &mOffscreenFrameBuffer);
}

void TrVulkanRendererRayTracingBase::CreateDescriptorSetLayout()
{
    // add bindings
    auto texNumber = static_cast<uint32_t>(mTextures.size());
    // VK_SHADER_STAGE_RAYGEN_BIT_KHR: ray generation stage
    mDescSetLayoutBindings.addBinding(ETrSceneBindings::Globals, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    mDescSetLayoutBindings.addBinding(ETrSceneBindings::ObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    mDescSetLayoutBindings.addBinding(ETrSceneBindings::Textures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texNumber, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    
    
    // create layout for bindings
    mDescSetLayout = mDescSetLayoutBindings.createLayout(m_device);

    // create pool
    mDescPool = mDescSetLayoutBindings.createPool(m_device, 1);

    // allocate descriptor set
    mDescSet = nvvk::allocateDescriptorSet(m_device, mDescPool, mDescSetLayout);
}

void TrVulkanRendererRayTracingBase::CreateGraphicsPipeline()
{
    // push constant ranges (model matrix, light info, obj index)
    VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TrPushConstantRaster)};
    
    // create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &mDescSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout);

    // create pipeline (shader, binding, attribute)
    nvvk::GraphicsPipelineGeneratorCombined graphicsPipelineGenerator(m_device, mPipelineLayout, mOffscreenRenderPass);
    graphicsPipelineGenerator.depthStencilState.depthTestEnable = true;
    graphicsPipelineGenerator.addShader(nvh::loadFile("spv/VkRayTracing/vert_shader.vert.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true), VK_SHADER_STAGE_VERTEX_BIT);
    graphicsPipelineGenerator.addShader(nvh::loadFile("spv/VkRayTracing/frag_shader.frag.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true), VK_SHADER_STAGE_FRAGMENT_BIT);
    graphicsPipelineGenerator.addBindingDescription({0, sizeof(VertexObj)});
    graphicsPipelineGenerator.addAttributeDescriptions({
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, pos))},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, nrm))},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, color))},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, texCoord))},
    });

    mGraphicsPipeline = graphicsPipelineGenerator.createPipeline();
    mDebugger.setObjectName(mGraphicsPipeline, "GraphicsPipeline");
}

void TrVulkanRendererRayTracingBase::CreateUniformBuffer()
{
    // global uniforms include viewProj viewInverse projInverse
    mBufferGlobals = mResourceAllocDma.createBuffer(sizeof(TrGlobalUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mDebugger.setObjectName(mBufferGlobals.buffer, "BufferGlobals");
}

void TrVulkanRendererRayTracingBase::CreateObjDescriptionBuffer()
{
    nvvk::CommandPool cmdPool(m_device, m_graphicsQueueIndex);
    auto cmdBuf = cmdPool.createCommandBuffer();
    mBufferObjDesc = mResourceAllocDma.createBuffer(cmdBuf, mObjDescs, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    cmdPool.submitAndWait(cmdBuf);
    mResourceAllocDma.finalizeAndReleaseStaging();
    mDebugger.setObjectName(mBufferObjDesc.buffer, "BufferObjDesc");
}

void TrVulkanRendererRayTracingBase::UpdateDescriptorSet()
{
    // use descriptor buffer info and image info to make write
    std::vector<VkWriteDescriptorSet> writes;
    VkDescriptorBufferInfo descBufferInfoUnif{mBufferGlobals.buffer, 0, VK_WHOLE_SIZE};
    writes.emplace_back(mDescSetLayoutBindings.makeWrite(mDescSet, ETrSceneBindings::Globals, &descBufferInfoUnif));

    VkDescriptorBufferInfo descBufferInfoSceneDesc{mBufferObjDesc.buffer, 0, VK_WHOLE_SIZE};
    writes.emplace_back(mDescSetLayoutBindings.makeWrite(mDescSet, ETrSceneBindings::ObjDescs, &descBufferInfoSceneDesc));

    std::vector<VkDescriptorImageInfo> descImageInfos;
    for(auto& texture : mTextures)
    {
        descImageInfos.emplace_back(texture.descriptor);
    }
    writes.emplace_back(mDescSetLayoutBindings.makeWriteArray(mDescSet, ETrSceneBindings::Textures, descImageInfos.data()));

    // use writes to update descriptor sets
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void TrVulkanRendererRayTracingBase::CreatePostDescriptor()
{
    // allocate descriptor set for post
    mPostDescSetLayoutBindings.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    mPostDescSetLayout = mPostDescSetLayoutBindings.createLayout(m_device);
    mPostDescPool = mPostDescSetLayoutBindings.createPool(m_device);
    mPostDescSet = nvvk::allocateDescriptorSet(m_device, mPostDescPool, mPostDescSetLayout);
}

void TrVulkanRendererRayTracingBase::CreatePostPipeline()
{
    // create pipeline layout
    VkPushConstantRange pushConstantRanges = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)};
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &mPostDescSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRanges;
    vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &mPostPipelineLayout);
    
    // create pipeline
    nvvk::GraphicsPipelineGeneratorCombined pipelineGenerator(m_device, mPostPipelineLayout, m_renderPass);
    pipelineGenerator.addShader(nvh::loadFile("spv/VkRayTracing/passthrough.vert.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true), VK_SHADER_STAGE_VERTEX_BIT); // pass through full screen triangle
    pipelineGenerator.addShader(nvh::loadFile("spv/VkRayTracing/post.frag.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true), VK_SHADER_STAGE_FRAGMENT_BIT); // fragment tone mapper shader
    pipelineGenerator.rasterizationState.cullMode = VK_CULL_MODE_NONE;
    mPostGraphicsPipeline = pipelineGenerator.createPipeline();
    mDebugger.setObjectName(mPostGraphicsPipeline, "PostGraphicsPipeline");
}

void TrVulkanRendererRayTracingBase::UpdatePostDescriptorSet()
{
    VkWriteDescriptorSet writeDescriptorSets = mPostDescSetLayoutBindings.makeWrite(mPostDescSet, 0, &mOffscreenColorTex.descriptor);
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSets, 0, nullptr);
}

void TrVulkanRendererRayTracingBase::RenderUI(glm::vec4 clearColor)
{
    bool bChanged = false;
    ImGuiH::Panel::Begin();
    bChanged |= ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&clearColor));

    bChanged |= ImGui::Checkbox("Enable Ray Tracer", &mbUseRayTracer);

    bChanged |= ImGui::SliderInt("Max Frames", &mMaxFrames, 1, 100);

    bChanged |= ImGui::SliderInt("Ray Generate Sample Number", &mPushConstantRay.mNumRayGenSamples, 1, 50);
    
    bChanged |= ImGuiH::CameraWidget();
    if(ImGui::CollapsingHeader("Light"))
    {
        bChanged |= ImGui::RadioButton("Point", &mPushConstantRaster.mLightType, 0);
        ImGui::SameLine();
        bChanged |= ImGui::RadioButton("Infinite", &mPushConstantRaster.mLightType, 1);

        bChanged |= ImGui::SliderFloat3("Position", &mPushConstantRaster.mLightPosition.x, -20.f, 20.f);
        bChanged |= ImGui::SliderFloat("Intensity", &mPushConstantRaster.mLightIntensity, 0.f, 150.f);
    }
    
    // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGuiH::Panel::End();

    if(bChanged)
    {
        ResetFrameVal();
    }
}

void TrVulkanRendererRayTracingBase::UpdateUniformBuffer(const VkCommandBuffer& cmdBuf)
{
    // prepare new ubo on host
    const float aspectRatio = m_size.width / static_cast<float>(m_size.height);
    TrGlobalUniforms hostUbo = {};
    const auto& view = CameraManip.getMatrix();
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj[1][1] *= -1;  // Inverting Y for Vulkan (not needed with perspectiveVK).
    
    hostUbo.mViewProj = proj * view;
    hostUbo.mViewInverse = glm::inverse(view);
    hostUbo.mProjInverse = glm::inverse(proj);

    VkBuffer deviceUbo = mBufferGlobals.buffer;
    auto uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

    VkBufferMemoryBarrier beforeBarrier {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    beforeBarrier.buffer = deviceUbo;
    beforeBarrier.offset = 0;
    beforeBarrier.size = sizeof(hostUbo);
    beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 1, &beforeBarrier, 0, nullptr);

    vkCmdUpdateBuffer(cmdBuf, mBufferGlobals.buffer, 0, sizeof(TrGlobalUniforms), &hostUbo);

    VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    afterBarrier.buffer = deviceUbo;
    afterBarrier.offset = 0;
    afterBarrier.size = sizeof(hostUbo);
    afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 1, &afterBarrier, 0, nullptr);

}

void TrVulkanRendererRayTracingBase::Rasterize(const VkCommandBuffer& cmdBuf)
{
    VkDeviceSize offset{0};

    setViewport(cmdBuf);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescSet, 0, nullptr);

    for(const TrObjInstanceRtBase inst : mObjInstances)
    {
        auto& model = mObjModels[inst.mObjIndex];
        mPushConstantRaster.mObjIndex = inst.mObjIndex;
        mPushConstantRaster.mModelMatrix = inst.mTransform;

        vkCmdPushConstants(cmdBuf, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TrPushConstantRaster), &mPushConstantRaster);
        vkCmdBindVertexBuffers(cmdBuf, 0, 1, &model.mVertexBuffer.buffer, &offset);
        vkCmdBindIndexBuffer(cmdBuf, model.mIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmdBuf, model.mNumIndices, 1, 0, 0, 0);
    }
}

void TrVulkanRendererRayTracingBase::DrawPost(const VkCommandBuffer& cmdBuf)
{
    setViewport(cmdBuf);

    auto aspectRatio = static_cast<float>(m_size.width) / static_cast<float>(m_size.height);

    vkCmdPushConstants(cmdBuf, mPostPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &aspectRatio);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mPostGraphicsPipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mPostPipelineLayout, 0, 1, &mPostDescSet, 0, nullptr);
    vkCmdDraw(cmdBuf, 3, 1, 0, 0);
}

void TrVulkanRendererRayTracingBase::DestroyResources()
{
    vkDestroyPipeline(m_device, mGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, mPipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, mDescPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, mDescSetLayout, nullptr);

    mResourceAllocDma.destroy(mBufferGlobals);
    mResourceAllocDma.destroy(mBufferObjDesc);

    for(auto& m : mObjModels)
    {
        mResourceAllocDma.destroy(m.mVertexBuffer);
        mResourceAllocDma.destroy(m.mIndexBuffer);
        mResourceAllocDma.destroy(m.mMatColorBuffer);
        mResourceAllocDma.destroy(m.mMatIndexBuffer);
    }

    for(auto& t : mTextures)
    {
        mResourceAllocDma.destroy(t);
    }

    //#Post
    mResourceAllocDma.destroy(mOffscreenColorTex);
    mResourceAllocDma.destroy(mOffscreenDepthTex);
    vkDestroyPipeline(m_device, mPostGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, mPostPipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, mPostDescPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, mPostDescSetLayout, nullptr);
    vkDestroyRenderPass(m_device, mOffscreenRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, mOffscreenFrameBuffer, nullptr);

    mRtBuilder.destroy();
    vkDestroyDescriptorPool(m_device, mRtDescPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, mRtDescSetLayout, nullptr);

    vkDestroyPipeline(m_device, mRtPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, mRtPipelineLayout, nullptr);
    mResourceAllocDma.destroy(mRtSbtBuffer);

    mResourceAllocDma.deinit();
}

void TrVulkanRendererRayTracingBase::onResize(int, int)
{
    ResetFrameVal();
    CreateOffscreenRender();
    UpdatePostDescriptorSet();
    UpdateRtDescriptorSet();
}

// query the rt capabilities of the GPU
void TrVulkanRendererRayTracingBase::InitRayTracing()
{
    VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    prop2.pNext = &mRtProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2); // properties into mRtProperties?

    mRtBuilder.setup(m_device, &mResourceAllocDma, m_graphicsQueueIndex);
}

// put obj model data into structures consumed by the AS builder
nvvk::RaytracingBuilderKHR::BlasInput TrVulkanRendererRayTracingBase::ObjectToVkGeometryKHR(const TrObjModelRtBase& model)
{
    // BLAS builder requires raw device addresses
    VkDeviceAddress vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.mVertexBuffer.buffer);
    VkDeviceAddress indexAddress = nvvk::getBufferDeviceAddress(m_device, model.mIndexBuffer.buffer);
    uint32_t maxPrimitiveCount = model.mNumIndices / 3;
    
    // device pointer to the buffers holding vertex/index data
    // describe buffer as array of VertexObj
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = sizeof(TrVulkanVertexRT);
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;
    triangles.maxVertex = model.mNumVertices - 1;

    // geometry type enum + flags (for AS builder)
    // Identify the above data as containing opaque triangles
    VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeom.flags =  VK_GEOMETRY_OPAQUE_BIT_KHR; // avoided invoking the any hit shader
    asGeom.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR; // to have the any hit shader process only one hit per triangle
    asGeom.geometry.triangles = triangles;
    
    // the indices within the vertex arrays to source input geometry for the BLAS
    // The entire array will be used to build the BLAS
    VkAccelerationStructureBuildRangeInfoKHR asRange;
    asRange.firstVertex = 0;
    asRange.primitiveCount = maxPrimitiveCount;
    asRange.primitiveOffset = 0;
    asRange.transformOffset = 0;

    nvvk::RaytracingBuilderKHR::BlasInput input;
    input.asGeometry.emplace_back(asGeom);
    input.asBuildOffsetInfo.emplace_back(asRange);

    return input;
}

void TrVulkanRendererRayTracingBase::CreateBLAS()
{
    std::vector<nvvk::RaytracingBuilderKHR::BlasInput> blasInputs;
    for(const auto& obj : mObjModels)
    {
        auto input = ObjectToVkGeometryKHR(obj);
        blasInputs.emplace_back(input);
    }
    mRtBuilder.buildBlas(blasInputs, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR); // prioritize trace performance over build time
}

void TrVulkanRendererRayTracingBase::CreateTLAS()
{
    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    asInstances.reserve(mObjInstances.size());
    for(const TrObjInstanceRtBase& inst : mObjInstances)
    {
        VkAccelerationStructureInstanceKHR asInst{};
        asInst.transform = nvvk::toTransformMatrixKHR(inst.mTransform);
        asInst.instanceCustomIndex = inst.mObjIndex;  // for shader gl_InstanceCustomIndexEXT usage
        asInst.accelerationStructureReference = mRtBuilder.getBlasDeviceAddress(inst.mObjIndex);
        asInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; // disables face culling for this instance
        asInst.mask = 0xFF;
        asInst.instanceShaderBindingTableRecordOffset = 0; // will use the same hit group for all objects
        asInstances.emplace_back(asInst);
    }
    mRtBuilder.buildTlas(asInstances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void TrVulkanRendererRayTracingBase::CreateRtDescriptorSet()
{
    mRtDescSetLayoutBindings.addBinding(ETrRtxBindings::Tlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    mRtDescSetLayoutBindings.addBinding(ETrRtxBindings::OutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

    mRtDescPool = mRtDescSetLayoutBindings.createPool(m_device);
    mRtDescSetLayout = mRtDescSetLayoutBindings.createLayout(m_device);

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = mRtDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mRtDescSetLayout;
    vkAllocateDescriptorSets(m_device, &allocInfo, &mRtDescSet);

    VkAccelerationStructureKHR tlas = mRtBuilder.getAccelerationStructure();
    VkWriteDescriptorSetAccelerationStructureKHR descAS{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    descAS.accelerationStructureCount = 1;
    descAS.pAccelerationStructures = &tlas;

    VkDescriptorImageInfo imageInfo{{}, mOffscreenColorTex.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

    std::vector<VkWriteDescriptorSet> writes;
    writes.emplace_back(mRtDescSetLayoutBindings.makeWrite(mRtDescSet, ETrRtxBindings::Tlas, &descAS));
    writes.emplace_back(mRtDescSetLayoutBindings.makeWrite(mRtDescSet, ETrRtxBindings::OutImage, &imageInfo));
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    
}

// the ray tracing descriptor set needs to be updated if its contents change
// when resizing the window, as the output image is recreated and needs to be re-linked to the descriptor set
void TrVulkanRendererRayTracingBase::UpdateRtDescriptorSet()
{
    VkDescriptorImageInfo imageInfo{{}, mOffscreenColorTex.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet write = mRtDescSetLayoutBindings.makeWrite(mRtDescSet, ETrRtxBindings::OutImage, &imageInfo);
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void TrVulkanRendererRayTracingBase::CreateRtPipeline()
{
    // all shader stages (create info)
    std::array<VkPipelineShaderStageCreateInfo, ShaderGroupCount> stages{};
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.pName = "main"; // all the same entry
    // raygen
    stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/VkRayTracing/raytrace.rgen.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true));
    stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[TrStageIndices::Raygen] = stage;
    // miss
    stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/VkRayTracing/raytrace.rmiss.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true));
    stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[TrStageIndices::Miss] = stage;

    // shadow miss is invoked when a shadow ray misses the geometry
    stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/VkRayTracing/raytraceShadow.rmiss.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true));
    stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[TrStageIndices::MissForShadow] = stage;

    // hit group - closest hit
    stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/VkRayTracing/raytrace.rchit.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true));
    stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[TrStageIndices::ClosestHit] = stage;

    // any hit
    stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/VkRayTracing/raytrace.rahit.spv", true, TrVulkanGlobalRT::defaultSearchPaths, true));
    stage.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    stages[TrStageIndices::AnyHit] = stage;
    
    // shader groups
    VkRayTracingShaderGroupCreateInfoKHR group{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
    group.anyHitShader = VK_SHADER_UNUSED_KHR;  // use a built-in pass-through shader
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;  // ray trace hardware therefore takes the place of the intersection shader

    // raygen
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = TrStageIndices::Raygen;
    mRtShaderGroupCreateInfos.push_back(group);

    // miss
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = TrStageIndices::Miss;
    mRtShaderGroupCreateInfos.push_back(group);

    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = TrStageIndices::MissForShadow;
    mRtShaderGroupCreateInfos.push_back(group);

    // intersection shader, any-hit shader and closest hit shader are bound into a hit group
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;  // because the geometry is made of triangles
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = TrStageIndices::ClosestHit;
    group.anyHitShader = TrStageIndices::AnyHit;
    mRtShaderGroupCreateInfos.push_back(group);

    // allow the ray tracing shaders to access the global uniform values
    VkPushConstantRange pushConstant{VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0, sizeof(TrPushConstantRay)};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; // describe how the pipeline will access external data
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    std::vector<VkDescriptorSetLayout> rtDescSetLayouts = {mRtDescSetLayout, mDescSetLayout};
    pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayouts.data();
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayouts.size());
    vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &mRtPipelineLayout);

    // ray tracing pipeline can contain an arbitrary number of stages depending on the number of active shaders in the scene
    VkRayTracingPipelineCreateInfoKHR rtPipelineCreateInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    rtPipelineCreateInfo.stageCount = static_cast<uint32_t>(stages.size());
    rtPipelineCreateInfo.pStages = stages.data();
    rtPipelineCreateInfo.groupCount = static_cast<uint32_t>(mRtShaderGroupCreateInfos.size());
    rtPipelineCreateInfo.pGroups = mRtShaderGroupCreateInfos.data();
    rtPipelineCreateInfo.maxPipelineRayRecursionDepth = 2; // has to allow shooting rays from the closest hit program (check is point in shadow)
    rtPipelineCreateInfo.layout = mRtPipelineLayout;
    // can create multiple pipeline once
    vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rtPipelineCreateInfo, nullptr, &mRtPipeline);

    // once the pipeline has been created we discard the shader modules
    for(auto& s : stages)
    {
        vkDestroyShaderModule(m_device, s.module, nullptr);
    }
    
}

// in a typical rasterization setup, a current shader and its associated resources are bound prior to drawing the corresponding objects
// another shader and resource set can be bound for some other objects
// but ray tracing can hit any surface of the scene at any time, all shaders must be available simultaneously
// allows us to select which ray generation shader to use as the entry point, which miss shader to execute if no intersections are found, and which hit shader groups can be executed for each instance
// instances --- shader groups association: for each instance we provided a hitGroupId in the TLAS
void TrVulkanRendererRayTracingBase::CreateSBT()
{
    uint32_t missCount{2};
    uint32_t hitCount{1};
    auto handleCount = 1 + missCount + hitCount;  // always and only 1 raygen
    // ensure that all starting groups start with an address aligned to shaderGroupBaseAlignment
    // and that each entry in the group is aligned to shaderGroupHandleAlignment bytes
    uint32_t handleSize = mRtProperties.shaderGroupHandleSize; // each entry in the SBT consists of shaderGroupHandleSize bytes of data

    // shaderGroupHandleAlignment is the required alignment in bytes for each SBT entry. The value must be a power of two
    uint32_t handleSizeAligned = nvh::align_up(handleSize, mRtProperties.shaderGroupHandleAlignment);
    
    // shaderGroupBaseAlignment is the required alignment in bytes for the base of the SBT
    mRaygenRegion.stride = nvh::align_up(handleSizeAligned, mRtProperties.shaderGroupBaseAlignment);
    mRaygenRegion.size = mRaygenRegion.stride;
    mMissRegion.stride = handleSizeAligned;
    mMissRegion.size = nvh::align_up(handleSizeAligned * missCount, mRtProperties.shaderGroupBaseAlignment);
    mClosestHitRegion.stride = handleSizeAligned;
    mClosestHitRegion.size = nvh::align_up(handleSizeAligned * hitCount, mRtProperties.shaderGroupBaseAlignment);

    // fetch the handles to the shader groups of the pipeline
    uint32_t dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);
    auto result = vkGetRayTracingShaderGroupHandlesKHR(m_device, mRtPipeline, 0, handleCount, dataSize, handles.data());
    assert(result == VK_SUCCESS);

    // allocate the buffer that will hold the handle data
    VkDeviceSize sbtSize = mRaygenRegion.size + mMissRegion.size + mClosestHitRegion.size + mCallRegion.size;
    // VK_BUFFER_USAGE_TRANSFER_SRC_BIT specifies that the buffer can be used as the source of a transfer command
    // VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT specifies that the buffer can be used to retrieve a
    // buffer device address via vkGetBufferDeviceAddress and use that address to access the buffer’s memory from a shader
    // VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR specifies that the buffer is suitable for use as a SBT
    auto bufUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit specifies that memory allocated with this type can be mapped for host access using vkMapMemory
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit specifies that the host cache management commands vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges 
    // are not needed to flush host writes to the device or make device writes visible to the host, respectively
    auto memProp = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    mRtSbtBuffer = mResourceAllocDma.createBuffer(sbtSize, bufUsage, memProp);

    // store the device address of each shader group (region)
    VkBufferDeviceAddressInfo bufDeviceAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, mRtSbtBuffer.buffer};
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(m_device, &bufDeviceAddressInfo);
    mRaygenRegion.deviceAddress = sbtAddress;
    mMissRegion.deviceAddress = sbtAddress + mRaygenRegion.size;
    mClosestHitRegion.deviceAddress = sbtAddress + mRaygenRegion.size + mMissRegion.size;

    // lambda returns the pointer to the previously retrieved handle to copy the data from the handle into the SBT buffer
    auto GetHandle = [&](int i){ return handles.data() + i * handleSize; };

    // buffer is visible to the host (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), map its memory in preparation for the data copy
    auto* pSbtBuffer = reinterpret_cast<uint8_t*>(mResourceAllocDma.map(mRtSbtBuffer));
    uint8_t* pData{nullptr};
    uint32_t handleIdx{0};

    // raygen
    pData = pSbtBuffer;
    memcpy(pData, GetHandle(handleIdx++), handleSize);

    // miss
    pData = pSbtBuffer + mRaygenRegion.size;
    for(uint32_t c = 0; c < missCount; c++)
    {
        memcpy(pData, GetHandle(handleIdx++), handleSize);
        pData += mMissRegion.stride;
    }

    // hit
    pData = pSbtBuffer + mRaygenRegion.size + mMissRegion.size;
    for(uint32_t c = 0; c < hitCount; c++)
    {
        memcpy(pData, GetHandle(handleIdx++), handleSize);
        pData += mClosestHitRegion.stride;
    }

    mResourceAllocDma.unmap(mRtSbtBuffer);
    mResourceAllocDma.finalizeAndReleaseStaging();
}

// record commands to call the ray trace shaders
// ray trace the scene
void TrVulkanRendererRayTracingBase::RayTrace(const VkCommandBuffer& cmdBuf, const glm::vec4& clearColor)
{
    UpdateFrameVal();
    if(mPushConstantRay.mFrame >= mMaxFrames)
    {
        return;
    }
    
    // init push constant values
    mPushConstantRay.mClearColor = clearColor;
    mPushConstantRay.mLightPosition = mPushConstantRaster.mLightPosition;
    mPushConstantRay.mLightIntensity = mPushConstantRaster.mLightIntensity;
    mPushConstantRay.mLightType = mPushConstantRaster.mLightType;

    std::vector<VkDescriptorSet> descSets{mRtDescSet, mDescSet};
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mRtPipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mRtPipelineLayout, 0, (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
    vkCmdPushConstants(cmdBuf, mRtPipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(TrPushConstantRay), &mPushConstantRay);
    vkCmdTraceRaysKHR(cmdBuf, &mRaygenRegion, &mMissRegion, &mClosestHitRegion, &mCallRegion, m_size.width, m_size.height, 1);
}

void TrVulkanRendererRayTracingBase::ResetFrameVal()
{
    mPushConstantRay.mFrame = -1;
}

void TrVulkanRendererRayTracingBase::UpdateFrameVal()
{
    static glm::mat4 oldCamMatrix;
    static float oldFov{CameraManip.getFov()};

    const auto& curCamMatrix = CameraManip.getMatrix();
    const auto curFov = CameraManip.getFov();

    if(oldCamMatrix != curCamMatrix || oldFov != curFov)
    {
        ResetFrameVal();
        oldCamMatrix = curCamMatrix;
        oldFov = curFov;
    }
    mPushConstantRay.mFrame++;
}



