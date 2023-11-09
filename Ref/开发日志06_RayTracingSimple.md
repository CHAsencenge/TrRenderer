[NVIDIA Vulkan Ray Tracing Tutorial (nvpro-samples.github.io)](https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/)

##### before已经完成了：

加载obj

rasterize them



##### simple要做：

修改before，使其支持ray tracing

能够作为其他tutorial的starting point



##### 具体要修改哪些功能：

context create info要能够使用ray tracing，增加extensions，选择合适的物理设备

使用加速结构AS（能够减少ray-triangle 相交测试，BLAS存实际的顶点数据，TLAS包含object instances，会引用到一个或者多个BLAS），ObjModel和ObjInstance其实就对应BLAS和TLAS，build加速结构使用nvvk::RaytracingBuilderKHR

填充几种结构（VkAccelerationStructureGeometryTrianglesDataKHR，VkAccelerationStructureGeometryKHR， VkAccelerationStructureBuildRangeInfoKHR），传递给AS builder

每个objModel通过转换会得到一个BlasInput，每个BlasInput会用于buildBlas

每次build加速结构都复用scratch buffer，因此需要确定最大的scratch memory大小

不要在单个command buffer中创建所有的BLAS

 