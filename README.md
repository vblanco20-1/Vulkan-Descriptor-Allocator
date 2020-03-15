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
//get an allocator handle. This is thread safe, 
//but can be expensive as it will allocate new VkDescriptorPools if there are no reusable ones. 
//Its recomended to create them only on a per-task basis.
auto allocator_handle = allocator_pool->GetAllocator();

VkDescriptorSet newSet;
if( allocator_handle.Allocate(descriptorLayout,newSet))
{
//if it returns true, the allocation succeeded and now you can use the newly allocated set.
}

//once per frame
//this will update the internal state to the next frame. 
//By doing this, the allocator will cycle its frames (numFrames). 
//When a frame is reused, it will clear those descriptor pools for reuse. 
//Dont keep allocator handles alive when calling Flip
allocator_pool->Flip();

```

# Installation
Just add the 2 code files to your project. No special build is needed.

# Internals
The allocator is implemented as a ringbuffer of frames. Each frame is a linear allocator of descriptor pools. When the ringbuffer loops over, it will reset all of the allocator pools of the frame its overriding and add them into a pending list. When a handle is grabbed, the allocator will reuse one of its already allocated descriptor pools. If there are no pools, it will create a new one. 
When descriptors are allocated from the handle, the library will check if the allocation succeeds. If the allocation fails as out-of-space or fragmented, it will grab a new descriptor pool from the allocator, or allocate a new one.
When a handle is destructed (RAII) the descriptor pool owned for that handle is returned to the allocator, and can be grabbed again until its full.

This library is designed for very high performance multithreaded allocations. Locking is minimal (only when returning pools or creating new handles), so it will not perform any locks until the pool the handle has gets filled. The recomended way to use it is to grab the handles from multithreaded jobs, and keep those handles alive for a while. Grabbing a new handle is relatively expensive, so grab them outside of your main render loops.

The handles are exclusively frame-based, so do not keep them alive from one frame to the next.

Recomended usage in a game engine is to create multiple allocators for different usages. One very typical usage is to have an allocator with only 1 frame for level-based descriptors, allocated at load time (you can then use Flip to completely reset the pools when switching levels), and another pool for dynamic frame descriptors, with 2 or 3 frames (depending on how you buffer your rendering).

