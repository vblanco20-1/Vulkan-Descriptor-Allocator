# Vulkan-Descriptor-Allocator
Multithread allocator for vulkan descriptor sets.

# Usage
```cpp
//initialization ------------------
//create an allocator pool. Its recomended you store it on a unique_ptr
allocator_pool = vke::DescriptorAllocatorPool::Create(device, numFrames);

//when allocating new VkDescriptorPools, it will now reserve 3 uniform buffer descriptors per descriptor set.
allocator_pool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3.f /* multiplier*/) = 0;


//when rendering ------------------
//get an allocator handle. This is thread safe, but can be expensive as it will allocate new VkDescriptorPools if there are no reusable ones. Its recomended to create them only on a per-task basis.
auto allocator_handle = allocator_pool->GetAllocator();

VkDescriptorSet newSet;
if( allocator_handle.Allocate(descriptorLayout,newSet))
{
//if it returns true, the allocation succeeded and now you can use the newly allocated set.
}

//once per frame
//this will update the internal state to the next frame. By doing this, the allocator will cycle its frames (numFrames). When a frame is reused, it will clear those descriptor pools for reuse. Dont keep allocator handles alive when calling Flip
allocator_pool->Flip();

```
