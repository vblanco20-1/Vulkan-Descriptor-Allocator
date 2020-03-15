#include <descriptor_allocator.h>
#include <vector>
#include <memory>
#include <mutex>



namespace vke {

	bool IsMemoryError(VkResult errorResult) {
		switch (errorResult) {
		case VK_ERROR_FRAGMENTED_POOL:
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			return true;
		}
		return false;
	}
	
	struct DescriptorAllocator {		
		VkDescriptorPool pool;
	};
	struct PoolStorage {

		std::vector<DescriptorAllocator> _usableAllocators;
		std::vector<DescriptorAllocator> _fullAllocators;
	};

	struct PoolSize {
		VkDescriptorType type;
		float multiplier;
	};
	struct PoolSizes {
		std::vector<PoolSize> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1.f },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1.f }
		};
	};

	
	class DescriptorAllocatorPoolImpl :public DescriptorAllocatorPool {
	public:
		 virtual ~DescriptorAllocatorPoolImpl();

		 void Flip() override final;
		 void SetPoolSizeMultiplier(VkDescriptorType type, float multiplier) override final;
		 DescriptorAllocatorHandle GetAllocator() override final;

		void ReturnAllocator(DescriptorAllocatorHandle& handle, bool bIsFull);
		VkDescriptorPool createPool(int count, VkDescriptorPoolCreateFlags flags);
		
		VkDevice _device;
		PoolSizes _poolSizes;
		int _frameIndex;
		int _maxFrames;

		std::mutex _poolMutex;

		//zero is for static pool, next is for frame indexing
		std::vector<std::unique_ptr<PoolStorage>> _descriptorPools;		

		//fully cleared allocators
		std::vector<DescriptorAllocator> _clearAllocators;
	};

	

	vke::DescriptorAllocatorPool* vke::DescriptorAllocatorPool::Create(const VkDevice& device, int nFrames)
	{
		DescriptorAllocatorPoolImpl* impl = new DescriptorAllocatorPoolImpl();
		impl->_device = device;
		impl->_frameIndex = 0;
		impl->_maxFrames = nFrames;
		for (int i = 0; i < nFrames; i++) {
			impl->_descriptorPools.push_back(std::make_unique<PoolStorage>());
		}
		return impl;
	}

	DescriptorAllocatorHandle::~DescriptorAllocatorHandle()
	{
		DescriptorAllocatorPoolImpl* implPool = static_cast<DescriptorAllocatorPoolImpl*>(ownerPool);
		if (implPool) {
			
			implPool->ReturnAllocator(*this, false);
		}
	}

	DescriptorAllocatorHandle::DescriptorAllocatorHandle(DescriptorAllocatorHandle&& other)
	{
		Return();

		vkPool = other.vkPool;
		poolIdx = other.poolIdx;
		ownerPool = other.ownerPool;

		other.ownerPool = nullptr;
		other.poolIdx = -1;
		other.vkPool = VkDescriptorPool{};
	}

	vke::DescriptorAllocatorHandle& DescriptorAllocatorHandle::operator=(DescriptorAllocatorHandle&& other)
	{
		Return();

		vkPool = other.vkPool;
		poolIdx = other.poolIdx;
		ownerPool = other.ownerPool;

		other.ownerPool = nullptr;
		other.poolIdx = -1;
		other.vkPool = VkDescriptorPool{};

		return *this;
	}

	void DescriptorAllocatorHandle::Return()
	{
		DescriptorAllocatorPoolImpl* implPool = static_cast<DescriptorAllocatorPoolImpl*>(ownerPool);

		if (implPool) {
			implPool->ReturnAllocator(*this, false);
		}		

		vkPool = VkDescriptorPool{ };
		poolIdx = -1;
		ownerPool = nullptr;
	}

	bool DescriptorAllocatorHandle::Allocate(const VkDescriptorSetLayout& layout, VkDescriptorSet& builtSet)
	{
		DescriptorAllocatorPoolImpl*implPool = static_cast<DescriptorAllocatorPoolImpl*>(ownerPool);
	

		VkDescriptorSetAllocateInfo allocInfo;
		allocInfo.pNext = nullptr;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = vkPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;

		VkResult result = vkAllocateDescriptorSets(implPool->_device, &allocInfo, &builtSet);
		if (result != VK_SUCCESS)
		{
			//we reallocate pools on memory error
			if (IsMemoryError(result))
			{
				//out of space need reallocate			
			
				implPool->ReturnAllocator(*this, true);

				DescriptorAllocatorHandle newHandle = implPool->GetAllocator();
				
				vkPool = newHandle.vkPool;
				poolIdx = newHandle.poolIdx;

				newHandle.vkPool = VkDescriptorPool{};
				newHandle.poolIdx = -1;
				newHandle.ownerPool = nullptr;
				//could be good idea to avoid infinite loop here
				return Allocate(layout, builtSet);
			}
			else {
				//stuff is truly broken
				return false;
			}
		}

		return true;
	}

	VkDescriptorPool DescriptorAllocatorPoolImpl::createPool(int count, VkDescriptorPoolCreateFlags flags)
	{
		std::vector<VkDescriptorPoolSize> sizes;
		sizes.reserve(_poolSizes.sizes.size());
		for (auto sz : _poolSizes.sizes) {
			sizes.push_back({ sz.type, uint32_t(sz.multiplier * count) });
		}
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = flags;
		pool_info.maxSets = count;
		pool_info.poolSizeCount = (uint32_t)sizes.size();
		pool_info.pPoolSizes = sizes.data();

		VkDescriptorPool descriptorPool;
		vkCreateDescriptorPool(_device, &pool_info, nullptr, &descriptorPool);

		return descriptorPool;
	}

	DescriptorAllocatorPoolImpl::~DescriptorAllocatorPoolImpl()
	{
		for (DescriptorAllocator allocator : _clearAllocators) {
			vkDestroyDescriptorPool(_device, allocator.pool, nullptr);
		}
		for (auto&& storage : _descriptorPools) {
			for (DescriptorAllocator allocator : storage->_fullAllocators) {
				vkDestroyDescriptorPool(_device, allocator.pool, nullptr);
			}
			for (DescriptorAllocator allocator : storage->_usableAllocators) {
				vkDestroyDescriptorPool(_device, allocator.pool, nullptr);
			}
		}
	}

	void DescriptorAllocatorPoolImpl::Flip()
	{
		 _frameIndex = (_frameIndex+1) % _maxFrames;
		
		 for (auto al :  _descriptorPools[_frameIndex]->_fullAllocators ) {

			 vkResetDescriptorPool(_device, al.pool, VkDescriptorPoolResetFlags{ 0 });

			 _clearAllocators.push_back(al);
		 }

		 for (auto al : _descriptorPools[_frameIndex]->_usableAllocators) {

			 vkResetDescriptorPool(_device, al.pool, VkDescriptorPoolResetFlags{ 0 });

			 _clearAllocators.push_back(al);
		 }

		 _descriptorPools[_frameIndex]->_fullAllocators.clear();
		 _descriptorPools[_frameIndex]->_usableAllocators.clear();
	}

	void DescriptorAllocatorPoolImpl::SetPoolSizeMultiplier(VkDescriptorType type, float multiplier)
	{
		for (auto& s : _poolSizes.sizes) {
			if (s.type == type) {
				s.multiplier = multiplier;
				return;
			}
		}

		//not found, so add it
		PoolSize newSize;
		newSize.type = type;
		newSize.multiplier = multiplier;
		_poolSizes.sizes.push_back(newSize);
	}

	void DescriptorAllocatorPoolImpl::ReturnAllocator(DescriptorAllocatorHandle& handle, bool bIsFull)
	{
		std::lock_guard<std::mutex> lk(_poolMutex);

		
		if (bIsFull) {
			_descriptorPools[handle.poolIdx]->_fullAllocators.push_back(DescriptorAllocator{ handle.vkPool });
		}
		else {
			_descriptorPools[handle.poolIdx]->_usableAllocators.push_back(DescriptorAllocator{ handle.vkPool });
		}
	}

	vke::DescriptorAllocatorHandle DescriptorAllocatorPoolImpl::GetAllocator()
	{
		std::lock_guard<std::mutex> lk(_poolMutex);

		
		bool foundAllocator = false;
		
		int poolIndex = _frameIndex ;


		DescriptorAllocator allocator;
		//try reuse an allocated pool
		if (_clearAllocators.size() != 0) {
			allocator = _clearAllocators.back();
			_clearAllocators.pop_back();
			foundAllocator = true;				
		}
		else {
			if (_descriptorPools[poolIndex]->_usableAllocators.size() > 0) {
				allocator = _descriptorPools[poolIndex]->_usableAllocators.back();
				_descriptorPools[poolIndex]->_usableAllocators.pop_back();
				foundAllocator = 1;
			}
		}
		//need a new pool
		if (!foundAllocator)
		{
			//static pool has to be free-able
			VkDescriptorPoolCreateFlags flags = 0;
			if (poolIndex == 0) {
				flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			}

			VkDescriptorPool newPool = createPool(2000, flags);

			allocator.pool = newPool;		

			foundAllocator = true;
		}

		DescriptorAllocatorHandle newHandle;
		newHandle.ownerPool = this;
		newHandle.poolIdx = poolIndex;
		newHandle.vkPool = allocator.pool;

		return newHandle;
	}
}