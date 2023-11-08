#include "TrVulkanRendererRayTracing.h"

TrVulkanRendererRayTracingBase::TrVulkanRendererRayTracingBase()
{
}

TrVulkanRendererRayTracingBase::TrVulkanRendererRayTracingBase(uint32_t width, uint32_t height, const char* title) :
TrVulkanRendererBase(width, height, title)
{
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
    LoadModel(nvh::findFile("media/scenes/cube_multi.obj", TrVulkanGlobalRT::defaultSearchPaths, true));

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
        nvmath::vec4f clearColor = nvmath::vec4f(1, 1, 1, 1.00f);
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

        vkCmdBeginRenderPass(cmdBuf, &offscreenRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        Rasterize(cmdBuf);

        vkCmdEndRenderPass(cmdBuf);
        
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

void TrVulkanRendererRayTracingBase::LoadModel(const std::string& filename, nvmath::mat4f transform)
{
    ObjLoader objLoader;
    objLoader.loadModel(filename);

    // material tone mapping, from srgb to linear
    for(MaterialObj mat : objLoader.m_materials)
    {
        mat.diffuse = nvmath::pow(mat.diffuse, 2.2f);
        mat.specular = nvmath::pow(mat.specular, 2.2f);
        mat.ambient= nvmath::pow(mat.ambient, 2.2f);
    }

    // create buffers for model vertices, indices, material colors, material indices
    nvvk::CommandPool cmdPool(m_device, m_graphicsQueueIndex);
    VkCommandBuffer cmdBuffer = cmdPool.createCommandBuffer();
    
    TrObjModelRtBase model;
    model.mNumIndices = static_cast<uint32_t>(objLoader.m_indices.size());
    model.mNumVertices = static_cast<uint32_t>(objLoader.m_vertices.size());

    // use vkGetBufferDeviceAddress can retrieve buffer, and can use that address to access buffer's memory from a shader
    VkBufferUsageFlags flag   = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    // VK_BUFFER_USAGE_VERTEX_BUFFER_BIT: buffer is suitable for passing to vkCmdBindVertexBuffers
    model.mVertexBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | flag);
    // VK_BUFFER_USAGE_INDEX_BUFFER_BIT: buffer is suitable for passing to vkCmdBindIndexBuffer
    model.mIndexBuffer = mResourceAllocDma.createBuffer(cmdBuffer, objLoader.m_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | flag);
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
            nvvk::Texture nvvkTexture = mResourceAllocDma.createTexture(image, imgViewCreateInfo);
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
    mDescSetLayoutBindings.addBinding(SceneBindings::eGlobals, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    mDescSetLayoutBindings.addBinding(SceneBindings::eObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    mDescSetLayoutBindings.addBinding(SceneBindings::eTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texNumber, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

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
    VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantRaster)};
    
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
    mBufferGlobals = mResourceAllocDma.createBuffer(sizeof(GlobalUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
    writes.emplace_back(mDescSetLayoutBindings.makeWrite(mDescSet, SceneBindings::eGlobals, &descBufferInfoUnif));

    VkDescriptorBufferInfo descBufferInfoSceneDesc{mBufferObjDesc.buffer, 0, VK_WHOLE_SIZE};
    writes.emplace_back(mDescSetLayoutBindings.makeWrite(mDescSet, SceneBindings::eObjDescs, &descBufferInfoSceneDesc));

    std::vector<VkDescriptorImageInfo> descImageInfos;
    for(auto& texture : mTextures)
    {
        descImageInfos.emplace_back(texture.descriptor);
    }
    writes.emplace_back(mDescSetLayoutBindings.makeWriteArray(mDescSet, SceneBindings::eTextures, descImageInfos.data()));

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

void TrVulkanRendererRayTracingBase::RenderUI(nvmath::vec4f clearColor)
{
    ImGuiH::Panel::Begin();
    ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&clearColor));
    
    ImGuiH::CameraWidget();
    if(ImGui::CollapsingHeader("Light"))
    {
        ImGui::RadioButton("Point", &mPushConstantRaster.lightType, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Infinite", &mPushConstantRaster.lightType, 1);

        ImGui::SliderFloat3("Position", &mPushConstantRaster.lightPosition.x, -20.f, 20.f);
        ImGui::SliderFloat("Intensity", &mPushConstantRaster.lightIntensity, 0.f, 150.f);
    }
    
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGuiH::Panel::End();
}

void TrVulkanRendererRayTracingBase::UpdateUniformBuffer(const VkCommandBuffer& cmdBuf)
{
    // prepare new ubo on host
    const float aspectRatio = m_size.width / static_cast<float>(m_size.height);
    GlobalUniforms hostUbo = {};
    const auto& view = CameraManip.getMatrix();
    const auto& proj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.1f, 1000.0f);

    hostUbo.viewProj = proj * view;
    hostUbo.viewInverse = nvmath::invert(view);
    hostUbo.projInverse = nvmath::invert(proj);

    VkBuffer deviceUbo = mBufferGlobals.buffer;
    auto uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkBufferMemoryBarrier beforeBarrier {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    beforeBarrier.buffer = deviceUbo;
    beforeBarrier.offset = 0;
    beforeBarrier.size = sizeof(hostUbo);
    beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 1, &beforeBarrier, 0, nullptr);

    vkCmdUpdateBuffer(cmdBuf, mBufferGlobals.buffer, 0, sizeof(GlobalUniforms), &hostUbo);

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
        mPushConstantRaster.objIndex = inst.mObjIndex;
        mPushConstantRaster.modelMatrix = inst.mTransform;

        vkCmdPushConstants(cmdBuf, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantRaster), &mPushConstantRaster);
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

    mResourceAllocDma.deinit();
}



