/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROSPER_DYNAMIC_RESIZABLE_BUFFER_HPP__
#define __PROSPER_DYNAMIC_RESIZABLE_BUFFER_HPP__

#include "prosper_definitions.hpp"
#include "buffers/prosper_resizable_buffer.hpp"
#include <memory>
#include <queue>
#include <functional>
#include <list>

namespace Anvil
{
	class BaseDevice;
	class Buffer;
};

namespace prosper
{
	class Context;
	class Buffer;
	class DynamicResizableBuffer;
	namespace util
	{
		struct BufferCreateInfo;
		DLLPROSPER std::shared_ptr<DynamicResizableBuffer> create_dynamic_resizable_buffer(
			Context &context,BufferCreateInfo createInfo,
			uint64_t maxTotalSize,float clampSizeToAvailableGPUMemoryPercentage=1.f,const void *data=nullptr
		);
	};

	class DLLPROSPER DynamicResizableBuffer
		: public ResizableBuffer
	{
	public:
		std::shared_ptr<Buffer> AllocateBuffer(vk::DeviceSize size,const void *data=nullptr);

		friend std::shared_ptr<DynamicResizableBuffer> util::create_dynamic_resizable_buffer(
			Context &context,util::BufferCreateInfo createInfo,
			uint64_t maxTotalSize,float clampSizeToAvailableGPUMemoryPercentage,const void *data
		);

		void DebugPrint(std::stringstream &strFilledData,std::stringstream &strFreeData,std::stringstream *bufferData=nullptr) const;
		const std::vector<Buffer*> &GetAllocatedSubBuffers() const;
		uint64_t GetFreeSize() const;
		float GetFragmentationPercent() const;
	private:
		struct Range
		{
			vk::DeviceSize startOffset;
			vk::DeviceSize size;
		};
		DynamicResizableBuffer(
			Context &context,Buffer &buffer,const util::BufferCreateInfo &createInfo,uint64_t maxTotalSize
		);
		std::shared_ptr<Buffer> AllocateBuffer(vk::DeviceSize size,uint32_t alignment,const void *data);
		void InsertFreeMemoryRange(std::list<Range>::iterator itWhere,vk::DeviceSize startOffset,vk::DeviceSize size);
		void MarkMemoryRangeAsFree(vk::DeviceSize startOffset,vk::DeviceSize size);
		std::list<Range>::iterator FindFreeRange(vk::DeviceSize size,uint32_t alignment);
		std::vector<Buffer*> m_allocatedSubBuffers;
		std::list<Range> m_freeRanges;
		uint32_t m_alignment = 0u;
	};
};

#endif