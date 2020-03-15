#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace vke {

	class DescriptorAllocatorPool;

	struct DescriptorAllocatorHandle {		
		friend class DescriptorAllocatorPool;
		DescriptorAllocatorHandle() = default;		
		DescriptorAllocatorHandle& operator=(const DescriptorAllocatorHandle&) = delete;

		~DescriptorAllocatorHandle();
		DescriptorAllocatorHandle(DescriptorAllocatorHandle&& other);
		DescriptorAllocatorHandle& operator=(DescriptorAllocatorHandle&& other);

		//return this handle to the pool. Will make this handle orphaned
		void Return();

		//allocate new descriptor. handle has to be valid
		//returns true if allocation succeeded, and false if it didnt
		//will mutate the handle if it requires a new vkDescriptorPool
		bool Allocate(const VkDescriptorSetLayout& layout, VkDescriptorSet& builtSet);

		
		DescriptorAllocatorPool* ownerPool{nullptr};
		VkDescriptorPool vkPool;
		int8_t poolIdx;
	};

	class DescriptorAllocatorPool {
	public:
		virtual ~DescriptorAllocatorPool(){};

		
		static DescriptorAllocatorPool* Create(const VkDevice& device, int nFrames = 3);

		//not thread safe
		//switches default allocators to the next frame. When frames loop it will reset the descriptors of that frame
		virtual void Flip() = 0;

		//not thread safe
		//override the pool size for a specific descriptor type. This will be used new pools are allocated
		virtual void SetPoolSizeMultiplier(VkDescriptorType type, float multiplier) = 0;

		//thread safe, uses lock
		//get handle to use when allocating descriptors
		virtual DescriptorAllocatorHandle GetAllocator() = 0;
	};
}
