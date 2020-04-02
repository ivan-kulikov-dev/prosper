/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper.h"
#include "prosper_context.hpp"
#include "prosper_util.hpp"
#include "prosper_util_image_buffer.hpp"
#include "image/prosper_render_target.hpp"
#include "buffers/prosper_buffer.hpp"
#include "prosper_descriptor_set_group.hpp"
#include "image/prosper_sampler.hpp"
#include "prosper_framebuffer.hpp"
#include "prosper_render_pass.hpp"
#include "prosper_command_buffer.hpp"
#include "image/prosper_image_view.hpp"
#include <sharedutils/util.h>
#include <config.h>
#include <wrappers/image.h>
#include <wrappers/image_view.h>
#include <wrappers/sampler.h>
#include <wrappers/command_buffer.h>
#include <wrappers/memory_block.h>
#include <wrappers/buffer.h>
#include <wrappers/device.h>
#include <wrappers/physical_device.h>
#include <misc/image_create_info.h>
#include <misc/image_view_create_info.h>
#include <misc/buffer_create_info.h>
#include <misc/framebuffer_create_info.h>
#include <util_image_buffer.hpp>
#include <util_image.hpp>
#include <util_texture_info.hpp>
#include <gli/gli.hpp>

#ifdef DEBUG_VERBOSE
#include <iostream>
#endif

using namespace prosper;

#pragma optimize("",off)
bool prosper::util::RenderPassCreateInfo::AttachmentInfo::operator==(const AttachmentInfo &other) const
{
	return format == other.format &&
		sampleCount == other.sampleCount &&
		loadOp == other.loadOp &&
		storeOp == other.storeOp &&
		initialLayout == other.initialLayout &&
		finalLayout == other.finalLayout;
}
bool prosper::util::RenderPassCreateInfo::AttachmentInfo::operator!=(const AttachmentInfo &other) const {return !operator==(other);}

/////////////////

bool prosper::util::RenderPassCreateInfo::SubPass::operator==(const SubPass &other) const
{
	return colorAttachments == other.colorAttachments &&
		useDepthStencilAttachment == other.useDepthStencilAttachment &&
		dependencies == other.dependencies;
}
bool prosper::util::RenderPassCreateInfo::SubPass::operator!=(const SubPass &other) const {return !operator==(other);}

/////////////////

bool prosper::util::RenderPassCreateInfo::SubPass::Dependency::operator==(const Dependency &other) const
{
	return sourceSubPassId == other.sourceSubPassId &&
		destinationSubPassId == other.destinationSubPassId &&
		sourceStageMask == other.sourceStageMask &&
		destinationStageMask == other.destinationStageMask &&
		sourceAccessMask == other.sourceAccessMask &&
		destinationAccessMask == other.destinationAccessMask;
}
bool prosper::util::RenderPassCreateInfo::SubPass::Dependency::operator!=(const Dependency &other) const {return !operator==(other);}

/////////////////

prosper::util::RenderPassCreateInfo::AttachmentInfo::AttachmentInfo(
	Anvil::Format format,Anvil::ImageLayout initialLayout,
	Anvil::AttachmentLoadOp loadOp,Anvil::AttachmentStoreOp storeOp,
	Anvil::SampleCountFlagBits sampleCount,Anvil::ImageLayout finalLayout
)
	: format{format},initialLayout{initialLayout},loadOp{loadOp},storeOp{storeOp},
	sampleCount{sampleCount},finalLayout{finalLayout}
{}
bool prosper::util::RenderPassCreateInfo::operator==(const RenderPassCreateInfo &other) const
{
	return attachments == other.attachments && subPasses == other.subPasses;
}
bool prosper::util::RenderPassCreateInfo::operator!=(const RenderPassCreateInfo &other) const {return !operator==(other);}

prosper::util::RenderPassCreateInfo::SubPass::SubPass(const std::vector<std::size_t> &colorAttachments,bool useDepthStencilAttachment,std::vector<Dependency> dependencies)
	: colorAttachments{colorAttachments},useDepthStencilAttachment{useDepthStencilAttachment},dependencies{dependencies}
{}
prosper::util::RenderPassCreateInfo::SubPass::Dependency::Dependency(
	std::size_t sourceSubPassId,std::size_t destinationSubPassId,
	Anvil::PipelineStageFlags sourceStageMask,Anvil::PipelineStageFlags destinationStageMask,
	Anvil::AccessFlags sourceAccessMask,Anvil::AccessFlags destinationAccessMask
)
	: sourceSubPassId{sourceSubPassId},destinationSubPassId{destinationSubPassId},
	sourceStageMask{sourceStageMask},destinationStageMask{destinationStageMask},
	sourceAccessMask{sourceAccessMask},destinationAccessMask{destinationAccessMask}
{}
prosper::util::RenderPassCreateInfo::RenderPassCreateInfo(const std::vector<AttachmentInfo> &attachments,const std::vector<SubPass> &subPasses)
	: attachments{attachments},subPasses{subPasses}
{}

static Anvil::QueueFamilyFlags queue_family_flags_to_anvil_queue_family(prosper::util::QueueFamilyFlags flags)
{
	Anvil::QueueFamilyFlags queueFamilies {};
	if((flags &prosper::util::QueueFamilyFlags::Graphics) != prosper::util::QueueFamilyFlags::None)
		queueFamilies = queueFamilies | Anvil::QueueFamilyFlagBits::GRAPHICS_BIT;
	if((flags &prosper::util::QueueFamilyFlags::Compute) != prosper::util::QueueFamilyFlags::None)
		queueFamilies = queueFamilies | Anvil::QueueFamilyFlagBits::COMPUTE_BIT;
	if((flags &prosper::util::QueueFamilyFlags::DMA) != prosper::util::QueueFamilyFlags::None)
		queueFamilies = queueFamilies | Anvil::QueueFamilyFlagBits::DMA_BIT;
	return queueFamilies;
}

static Anvil::MemoryFeatureFlags memory_feature_flags_to_anvil_flags(prosper::util::MemoryFeatureFlags flags)
{
	auto memoryFeatureFlags = Anvil::MemoryFeatureFlags{};
	if((flags &prosper::util::MemoryFeatureFlags::DeviceLocal) != prosper::util::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::DEVICE_LOCAL_BIT;
	if((flags &prosper::util::MemoryFeatureFlags::HostCached) != prosper::util::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::HOST_CACHED_BIT;
	if((flags &prosper::util::MemoryFeatureFlags::HostCoherent) != prosper::util::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::HOST_COHERENT_BIT;
	if((flags &prosper::util::MemoryFeatureFlags::LazilyAllocated) != prosper::util::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::LAZILY_ALLOCATED_BIT;
	if((flags &prosper::util::MemoryFeatureFlags::HostAccessable) != prosper::util::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::MAPPABLE_BIT;
	return memoryFeatureFlags;
}

std::pair<const Anvil::MemoryType*,prosper::util::MemoryFeatureFlags> prosper::util::find_compatible_memory_type(Anvil::BaseDevice &dev,MemoryFeatureFlags featureFlags)
{
	auto r = std::pair<const Anvil::MemoryType*,prosper::util::MemoryFeatureFlags>{nullptr,featureFlags};
	if(umath::to_integral(featureFlags) == 0u)
		return r;
	prosper::util::MemoryFeatureFlags requiredFeatures {};

    // Convert usage to requiredFlags and preferredFlags.
	switch(featureFlags)
	{
	case prosper::util::MemoryFeatureFlags::CPUToGPU:
	case prosper::util::MemoryFeatureFlags::GPUToCPU:
		requiredFeatures |= prosper::util::MemoryFeatureFlags::HostAccessable;
	}

	auto anvFlags = memory_feature_flags_to_anvil_flags(featureFlags);
	auto anvRequiredFlags = memory_feature_flags_to_anvil_flags(requiredFeatures);
	auto &memProps = dev.get_physical_device_memory_properties();
	for(auto &type : memProps.types)
	{
		if((type.features &anvFlags) == anvFlags)
		{
			// Found preferred flags
			r.first = &type;
			return r;
		}
		if((type.features &anvRequiredFlags) != Anvil::MemoryFeatureFlagBits::NONE)
			r.first = &type; // Found required flags
	}
	r.second = requiredFeatures;
	return r;
}

uint64_t prosper::util::clamp_gpu_memory_size(Anvil::BaseDevice &dev,uint64_t size,float percentageOfGPUMemory,MemoryFeatureFlags featureFlags)
{
	auto r = find_compatible_memory_type(dev,featureFlags);
	if(r.first == nullptr || r.first->heap_ptr == nullptr)
		throw std::runtime_error("Incompatible memory feature flags");
	auto maxMem = std::floorl(r.first->heap_ptr->size *static_cast<long double>(percentageOfGPUMemory));
	return umath::min(size,static_cast<uint64_t>(maxMem));
}

static void find_compatible_memory_feature_flags(Anvil::BaseDevice &dev,prosper::util::MemoryFeatureFlags &featureFlags)
{
	auto r = prosper::util::find_compatible_memory_type(dev,featureFlags);
	if(r.first == nullptr)
		throw std::runtime_error("Incompatible memory feature flags");
	featureFlags = r.second;
}

std::shared_ptr<Buffer> prosper::util::create_sub_buffer(Buffer &parentBuffer,vk::DeviceSize offset,vk::DeviceSize size,const std::function<void(Buffer&)> &onDestroyedCallback)
{
	auto buf = Anvil::Buffer::create(Anvil::BufferCreateInfo::create_no_alloc_child(&parentBuffer.GetAnvilBuffer(),offset,size));
	auto subBuffer = Buffer::Create(parentBuffer.GetContext(),std::move(buf),onDestroyedCallback);
	subBuffer->SetParent(parentBuffer.shared_from_this());
	return subBuffer;
}

std::shared_ptr<Buffer> prosper::util::create_buffer(Anvil::BaseDevice &dev,const BufferCreateInfo &createInfo,const void *data)
{
	if(createInfo.size == 0ull)
		return nullptr;
	auto sharingMode = Anvil::SharingMode::EXCLUSIVE;
	if((createInfo.flags &BufferCreateInfo::Flags::ConcurrentSharing) != BufferCreateInfo::Flags::None)
		sharingMode = Anvil::SharingMode::CONCURRENT;

	auto &context = Context::GetContext(dev);
	if((createInfo.flags &BufferCreateInfo::Flags::Sparse) != BufferCreateInfo::Flags::None)
	{
		Anvil::BufferCreateFlags createFlags {Anvil::BufferCreateFlagBits::NONE};
		if((createInfo.flags &BufferCreateInfo::Flags::SparseAliasedResidency) != BufferCreateInfo::Flags::None)
			createFlags |= Anvil::BufferCreateFlagBits::SPARSE_ALIASED_BIT | Anvil::BufferCreateFlagBits::SPARSE_RESIDENCY_BIT;

		return Buffer::Create(context,Anvil::Buffer::create(
			Anvil::BufferCreateInfo::create_no_alloc(
				&dev,static_cast<VkDeviceSize>(createInfo.size),queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask),
				sharingMode,createFlags,createInfo.usageFlags
			)
		));
	}
	Anvil::BufferCreateFlags createFlags {Anvil::BufferCreateFlagBits::NONE};
	if((createInfo.flags &BufferCreateInfo::Flags::DontAllocateMemory) == BufferCreateInfo::Flags::None)
	{
		auto memoryFeatures = createInfo.memoryFeatures;
		find_compatible_memory_feature_flags(dev,memoryFeatures);
		auto bufferCreateInfo = Anvil::BufferCreateInfo::create_alloc(
			&dev,static_cast<VkDeviceSize>(createInfo.size),queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask),
			sharingMode,createFlags,createInfo.usageFlags,memory_feature_flags_to_anvil_flags(memoryFeatures)
		);
		bufferCreateInfo->set_client_data(data);
		auto r = Buffer::Create(context,Anvil::Buffer::create(
			std::move(bufferCreateInfo)
		));
		return r;
	}
	return Buffer::Create(context,Anvil::Buffer::create(
		Anvil::BufferCreateInfo::create_no_alloc(
			&dev,static_cast<VkDeviceSize>(createInfo.size),queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask),
			sharingMode,createFlags,createInfo.usageFlags
		)
	));
}

std::shared_ptr<prosper::RenderPass> prosper::util::create_render_pass(Anvil::BaseDevice &dev,std::unique_ptr<Anvil::RenderPassCreateInfo> renderPassInfo)
{
	auto &context = prosper::Context::GetContext(dev);
	return prosper::RenderPass::Create(context,Anvil::RenderPass::create(std::move(renderPassInfo),context.GetSwapchain().get()));
}

std::shared_ptr<prosper::RenderPass> prosper::util::create_render_pass(Anvil::BaseDevice &dev,const RenderPassCreateInfo &renderPassInfo)
{
	if(renderPassInfo.attachments.empty())
		throw std::logic_error("Attempted to create render pass with 0 attachments, this is not allowed!");
	auto rpInfo = std::make_unique<Anvil::RenderPassCreateInfo>(&dev);
	std::vector<Anvil::RenderPassAttachmentID> attachmentIds;
	attachmentIds.reserve(renderPassInfo.attachments.size());
	auto depthStencilAttId = std::numeric_limits<Anvil::RenderPassAttachmentID>::max();
	for(auto &attInfo : renderPassInfo.attachments)
	{
		Anvil::RenderPassAttachmentID attId;
		if(is_depth_format(attInfo.format) == false)
		{
			rpInfo->add_color_attachment(
				attInfo.format,attInfo.sampleCount,
				attInfo.loadOp,attInfo.storeOp,
				attInfo.initialLayout,attInfo.finalLayout,
				true,&attId
			);
		}
		else
		{
			if(depthStencilAttId != std::numeric_limits<Anvil::RenderPassAttachmentID>::max())
				throw std::logic_error("Attempted to add more than one depth stencil attachment to render pass, this is not allowed!");
			rpInfo->add_depth_stencil_attachment(
				attInfo.format,attInfo.sampleCount,
				attInfo.loadOp,attInfo.storeOp,
				Anvil::AttachmentLoadOp::DONT_CARE,Anvil::AttachmentStoreOp::DONT_CARE,
				attInfo.initialLayout,attInfo.finalLayout,
				true,&attId
			);
			depthStencilAttId = attId;
		}
		attachmentIds.push_back(attId);
	}
	if(renderPassInfo.subPasses.empty() == false)
	{
		for(auto &subPassInfo : renderPassInfo.subPasses)
		{
			Anvil::SubPassID subPassId;
			rpInfo->add_subpass(&subPassId);
			auto location = 0u;
			for(auto attId : subPassInfo.colorAttachments)
				rpInfo->add_subpass_color_attachment(subPassId,Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,attId,location++,nullptr);
			if(subPassInfo.useDepthStencilAttachment)
			{
				if(depthStencilAttId == std::numeric_limits<Anvil::RenderPassAttachmentID>::max())
					throw std::logic_error("Attempted to add depth stencil attachment sub-pass, but no depth stencil attachment has been specified!");
				rpInfo->add_subpass_depth_stencil_attachment(subPassId,Anvil::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,depthStencilAttId);
			}
			for(auto &dependency : subPassInfo.dependencies)
			{
				rpInfo->add_subpass_to_subpass_dependency(
					dependency.destinationSubPassId,dependency.sourceSubPassId,
					dependency.destinationStageMask,dependency.sourceStageMask,
					dependency.destinationAccessMask,dependency.sourceAccessMask,
					Anvil::DependencyFlagBits::NONE
				);
			}
		}
	}
	else
	{
		Anvil::SubPassID subPassId;
		rpInfo->add_subpass(&subPassId);

		if(depthStencilAttId != std::numeric_limits<Anvil::RenderPassAttachmentID>::max())
			rpInfo->add_subpass_depth_stencil_attachment(subPassId,Anvil::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,depthStencilAttId);
		
		auto attId = 0u;
		auto location = 0u;
		for(auto &attInfo : renderPassInfo.attachments)
		{
			if(is_depth_format(attInfo.format) == false)
				rpInfo->add_subpass_color_attachment(subPassId,Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,attId,location++,nullptr);
			++attId;
		}
	}
	return create_render_pass(dev,std::move(rpInfo));
}

static void init_default_dsg_bindings(Anvil::BaseDevice &dev,Anvil::DescriptorSetGroup &dsg)
{
	// Initialize image sampler bindings with dummy texture
	auto &context = prosper::Context::GetContext(dev);
	auto &dummyTex = context.GetDummyTexture();
	auto &dummyBuf = context.GetDummyBuffer();
	auto numSets = dsg.get_n_descriptor_sets();
	for(auto i=decltype(numSets){0};i<numSets;++i)
	{
		auto *descSet = dsg.get_descriptor_set(i);
		auto *descSetLayout = dsg.get_descriptor_set_layout(i);
		auto &info = *descSetLayout->get_create_info();
		auto numBindings = info.get_n_bindings();
		auto bindingIndex = 0u;
		for(auto j=decltype(numBindings){0};j<numBindings;++j)
		{
			Anvil::DescriptorType descType;
			uint32_t arraySize;
			info.get_binding_properties_by_index_number(j,nullptr,&descType,&arraySize,nullptr,nullptr);
			switch(descType)
			{
				case Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER:
				{
					std::vector<Anvil::DescriptorSet::CombinedImageSamplerBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::CombinedImageSamplerBindingElement{
						Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
						&dummyTex->GetImageView()->GetAnvilImageView(),&dummyTex->GetSampler()->GetAnvilSampler()
					});
					descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
					break;
				}
				case Anvil::DescriptorType::UNIFORM_BUFFER:
				{
					std::vector<Anvil::DescriptorSet::UniformBufferBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::UniformBufferBindingElement{
						&dummyBuf->GetAnvilBuffer(),0ull,dummyBuf->GetSize()
					});
					descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
					break;
				}
				case Anvil::DescriptorType::UNIFORM_BUFFER_DYNAMIC:
				{
					std::vector<Anvil::DescriptorSet::DynamicUniformBufferBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::DynamicUniformBufferBindingElement{
						&dummyBuf->GetAnvilBuffer(),0ull,dummyBuf->GetSize()
					});
					descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
					break;
				}
				case Anvil::DescriptorType::STORAGE_BUFFER:
				{
					std::vector<Anvil::DescriptorSet::StorageBufferBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::StorageBufferBindingElement{
						&dummyBuf->GetAnvilBuffer(),0ull,dummyBuf->GetSize()
					});
					descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
					break;
				}
				case Anvil::DescriptorType::STORAGE_BUFFER_DYNAMIC:
				{
					std::vector<Anvil::DescriptorSet::DynamicStorageBufferBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::DynamicStorageBufferBindingElement{
						&dummyBuf->GetAnvilBuffer(),0ull,dummyBuf->GetSize()
					});
					descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
					break;
				}
			}
			//bindingIndex += arraySize;
			++bindingIndex;
		}
	}
}
std::shared_ptr<prosper::DescriptorSetGroup> prosper::util::create_descriptor_set_group(Anvil::BaseDevice &dev,std::unique_ptr<Anvil::DescriptorSetCreateInfo> descSetInfo)
{
	std::vector<std::unique_ptr<Anvil::DescriptorSetCreateInfo>> descSetInfos = {};
	descSetInfos.push_back(std::move(descSetInfo));
	auto dsg = Anvil::DescriptorSetGroup::create(&dev,descSetInfos,Anvil::DescriptorPoolCreateFlagBits::FREE_DESCRIPTOR_SET_BIT);
	init_default_dsg_bindings(dev,*dsg);
	return prosper::DescriptorSetGroup::Create(prosper::Context::GetContext(dev),std::move(dsg));
}

std::shared_ptr<prosper::Framebuffer> prosper::util::create_framebuffer(Anvil::BaseDevice &dev,uint32_t width,uint32_t height,uint32_t layers,const std::vector<prosper::ImageView*> &attachments)
{
	auto createInfo = Anvil::FramebufferCreateInfo::create(
		&dev,width,height,layers
	);
	for(auto *att : attachments)
		createInfo->add_attachment(&att->GetAnvilImageView(),nullptr);
	return prosper::Framebuffer::Create(prosper::Context::GetContext(dev),attachments,Anvil::Framebuffer::create(
		std::move(createInfo)
	));
}

static std::shared_ptr<prosper::Image> create_image(Anvil::BaseDevice &dev,const prosper::util::ImageCreateInfo &createInfo,const std::vector<Anvil::MipmapRawData> *data)
{
	auto layers = createInfo.layers;
	auto imageCreateFlags = Anvil::ImageCreateFlags{};
	if((createInfo.flags &prosper::util::ImageCreateInfo::Flags::Cubemap) != prosper::util::ImageCreateInfo::Flags::None)
	{
		imageCreateFlags |= Anvil::ImageCreateFlagBits::CUBE_COMPATIBLE_BIT;
		layers = 6u;
	}

	auto postCreateLayout = createInfo.postCreateLayout;
	if(postCreateLayout == Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL && prosper::util::is_depth_format(createInfo.format))
		postCreateLayout = Anvil::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	auto memoryFeatures = createInfo.memoryFeatures;
	find_compatible_memory_feature_flags(dev,memoryFeatures);
	auto memoryFeatureFlags = memory_feature_flags_to_anvil_flags(memoryFeatures);
	auto queueFamilies = queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask);
	auto sharingMode = Anvil::SharingMode::EXCLUSIVE;
	if((createInfo.flags &prosper::util::ImageCreateInfo::Flags::ConcurrentSharing) != prosper::util::ImageCreateInfo::Flags::None)
		sharingMode = Anvil::SharingMode::CONCURRENT;

	auto useDiscreteMemory = umath::is_flag_set(createInfo.flags,prosper::util::ImageCreateInfo::Flags::AllocateDiscreteMemory);
	if(
		useDiscreteMemory == false &&
		((createInfo.memoryFeatures &prosper::util::MemoryFeatureFlags::HostAccessable) != prosper::util::MemoryFeatureFlags::None ||
		(createInfo.memoryFeatures &prosper::util::MemoryFeatureFlags::DeviceLocal) == prosper::util::MemoryFeatureFlags::None)
	)
		useDiscreteMemory = true; // Pre-allocated memory currently only supported for device local memory

	auto sparse = (createInfo.flags &prosper::util::ImageCreateInfo::Flags::Sparse) != prosper::util::ImageCreateInfo::Flags::None;

	auto bUseFullMipmapChain = (createInfo.flags &prosper::util::ImageCreateInfo::Flags::FullMipmapChain) != prosper::util::ImageCreateInfo::Flags::None;
	if(useDiscreteMemory == false || sparse)
	{
		if((createInfo.flags &prosper::util::ImageCreateInfo::Flags::SparseAliasedResidency) != prosper::util::ImageCreateInfo::Flags::None)
			imageCreateFlags |= Anvil::ImageCreateFlagBits::SPARSE_ALIASED_BIT | Anvil::ImageCreateFlagBits::SPARSE_RESIDENCY_BIT;
		;
		auto &context = prosper::Context::GetContext(dev);
		auto img = prosper::Image::Create(context,Anvil::Image::create(
			Anvil::ImageCreateInfo::create_no_alloc(
				&dev,createInfo.type,createInfo.format,
				createInfo.tiling,createInfo.usage,
				createInfo.width,createInfo.height,1u,layers,
				createInfo.samples,queueFamilies,
				sharingMode,bUseFullMipmapChain,imageCreateFlags,
				postCreateLayout,data
			)
		));
		if(sparse == false)
			context.AllocateDeviceImageBuffer(*img);
		return img;
	}

	return prosper::Image::Create(prosper::Context::GetContext(dev),Anvil::Image::create(
		Anvil::ImageCreateInfo::create_alloc(
			&dev,createInfo.type,createInfo.format,
			createInfo.tiling,createInfo.usage,
			createInfo.width,createInfo.height,1u,layers,
			createInfo.samples,queueFamilies,
			sharingMode,bUseFullMipmapChain,memoryFeatureFlags,imageCreateFlags,
			postCreateLayout,data
		)
	));
}
std::shared_ptr<prosper::Image> prosper::util::create_image(Anvil::BaseDevice &dev,const ImageCreateInfo &createInfo,const uint8_t *data)
{
	if(data == nullptr)
		return ::create_image(dev,createInfo,nullptr);
	auto byteSize = get_byte_size(createInfo.format);
	return create_image(dev,createInfo,std::vector<Anvil::MipmapRawData>{
		Anvil::MipmapRawData::create_2D_from_uchar_ptr(
			Anvil::ImageAspectFlagBits::COLOR_BIT,0u,
			data,createInfo.width *createInfo.height *byteSize,createInfo.width *byteSize
		)
	});
}
std::shared_ptr<prosper::Image> prosper::util::create_image(Anvil::BaseDevice &dev,const ImageCreateInfo &createInfo,const std::vector<Anvil::MipmapRawData> &data) {return ::create_image(dev,createInfo,&data);}

static std::shared_ptr<Image> create_image(Anvil::BaseDevice &dev,std::vector<std::shared_ptr<uimg::ImageBuffer>> &imgBuffers,bool cubemap)
{
	if(imgBuffers.empty() || imgBuffers.size() != (cubemap ? 6 : 1))
		return nullptr;
	auto &img0 = imgBuffers.at(0);
	auto format = img0->GetFormat();
	auto w = img0->GetWidth();
	auto h = img0->GetHeight();
	for(auto &img : imgBuffers)
	{
		if(img->GetFormat() != format || img->GetWidth() != w || img->GetHeight() != h)
			return nullptr;
	}

	std::optional<uimg::ImageBuffer::Format> conversionFormatRequired = {};
	Anvil::Format anvFormat;
	switch(format)
	{
	case uimg::ImageBuffer::Format::RGB8:
		conversionFormatRequired = uimg::ImageBuffer::Format::RGBA8;
		break;
	case uimg::ImageBuffer::Format::RGB16:
		conversionFormatRequired = uimg::ImageBuffer::Format::RGBA16;
		break;
	case uimg::ImageBuffer::Format::RGB32:
		conversionFormatRequired = uimg::ImageBuffer::Format::RGBA32;
		break;
	case uimg::ImageBuffer::Format::RGBA8:
		anvFormat = Anvil::Format::R8G8B8A8_UNORM;
		break;
	case uimg::ImageBuffer::Format::RGBA16:
		anvFormat = Anvil::Format::R16G16B16A16_UNORM;
		break;
	case uimg::ImageBuffer::Format::RGBA32:
		anvFormat = Anvil::Format::R32G32B32A32_SFLOAT;
		break;
	}
	static_assert(umath::to_integral(uimg::ImageBuffer::Format::Count) == 7);
	if(conversionFormatRequired.has_value())
	{
		std::vector<std::shared_ptr<uimg::ImageBuffer>> converted {};
		converted.reserve(imgBuffers.size());
		for(auto &img : imgBuffers)
		{
			auto copy = img->Copy(*conversionFormatRequired);
			converted.push_back(copy);
		}
		return create_image(dev,converted,cubemap);
	}
	prosper::util::ImageCreateInfo imgCreateInfo {};
	imgCreateInfo.format = anvFormat;
	imgCreateInfo.width = w;
	imgCreateInfo.height = h;
	imgCreateInfo.memoryFeatures = prosper::util::MemoryFeatureFlags::GPUBulk;
	imgCreateInfo.postCreateLayout = Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL;
	imgCreateInfo.tiling = Anvil::ImageTiling::OPTIMAL;
	imgCreateInfo.usage = Anvil::ImageUsageFlagBits::SAMPLED_BIT;
	imgCreateInfo.layers = imgBuffers.size();
	if(cubemap)
		imgCreateInfo.flags |= prosper::util::ImageCreateInfo::Flags::Cubemap;

	auto byteSize = prosper::util::get_byte_size(imgCreateInfo.format);
	std::vector<Anvil::MipmapRawData> layers {};
	layers.reserve(imgBuffers.size());
	for(auto iLayer=decltype(imgBuffers.size()){0u};iLayer<imgBuffers.size();++iLayer)
	{
		auto &imgBuf = imgBuffers.at(iLayer);
		if(cubemap)
		{
			layers.push_back(Anvil::MipmapRawData::create_cube_map_from_uchar_ptr(
				Anvil::ImageAspectFlagBits::COLOR_BIT,iLayer,0,
				static_cast<uint8_t*>(imgBuf->GetData()),imgCreateInfo.width *imgCreateInfo.height *byteSize,imgCreateInfo.width *byteSize
			));
		}
		else
		{
			layers.push_back(Anvil::MipmapRawData::create_2D_from_uchar_ptr(
				Anvil::ImageAspectFlagBits::COLOR_BIT,0u,
				static_cast<uint8_t*>(imgBuf->GetData()),imgCreateInfo.width *imgCreateInfo.height *byteSize,imgCreateInfo.width *byteSize
			));
		}
	}
	return create_image(dev,imgCreateInfo,layers);
}

std::shared_ptr<Image> prosper::util::create_cubemap(Anvil::BaseDevice &dev,std::array<std::shared_ptr<uimg::ImageBuffer>,6> &imgBuffers)
{
	std::vector<std::shared_ptr<uimg::ImageBuffer>> vImgBuffers {};
	vImgBuffers.reserve(imgBuffers.size());
	for(auto &imgBuf : imgBuffers)
		vImgBuffers.push_back(imgBuf);
	return ::create_image(dev,vImgBuffers,true);
}

std::shared_ptr<Image> prosper::util::create_image(Anvil::BaseDevice &dev,uimg::ImageBuffer &imgBuffer)
{
	return ::create_image(dev,std::vector<std::shared_ptr<uimg::ImageBuffer>>{imgBuffer.shared_from_this()},false);
}

std::shared_ptr<prosper::ImageView> prosper::util::create_image_view(Anvil::BaseDevice &dev,const ImageViewCreateInfo &createInfo,std::shared_ptr<Image> img)
{
	auto format = createInfo.format;
	if(format == Anvil::Format::UNKNOWN)
		format = img->GetFormat();

	auto aspectMask = get_aspect_mask(format);
	auto imgType = img->GetType();
	auto numLayers = createInfo.levelCount;
	auto type = Anvil::ImageViewType::_2D;
	if(numLayers > 1u && (img->GetCreateFlags() &Anvil::ImageCreateFlagBits::CUBE_COMPATIBLE_BIT) != 0)
		type = (numLayers > 6u) ? Anvil::ImageViewType::_CUBE_ARRAY : Anvil::ImageViewType::_CUBE;
	else
	{
		switch(imgType)
		{
			case Anvil::ImageType::_1D:
				type = (numLayers > 1u) ? Anvil::ImageViewType::_1D_ARRAY : Anvil::ImageViewType::_1D;
				break;
			case Anvil::ImageType::_2D:
				type = (numLayers > 1u) ? Anvil::ImageViewType::_2D_ARRAY : Anvil::ImageViewType::_2D;
				break;
			case Anvil::ImageType::_3D:
				type = Anvil::ImageViewType::_3D;
				break;
		}
	}
	switch(type)
	{
		case Anvil::ImageViewType::_2D:
			return ImageView::Create(prosper::Context::GetContext(dev),*img,Anvil::ImageView::create(
				Anvil::ImageViewCreateInfo::create_2D(
					&dev,&img->GetAnvilImage(),createInfo.baseLayer.has_value() ? *createInfo.baseLayer : 0u,createInfo.baseMipmap,umath::min(createInfo.baseMipmap +createInfo.mipmapLevels,(*img)->get_n_mipmaps()) -createInfo.baseMipmap,
					aspectMask,format,
					createInfo.swizzleRed,createInfo.swizzleGreen,
					createInfo.swizzleBlue,createInfo.swizzleAlpha
				)
			));
		case Anvil::ImageViewType::_CUBE:
			return ImageView::Create(prosper::Context::GetContext(dev),*img,Anvil::ImageView::create(
				Anvil::ImageViewCreateInfo::create_cube_map(
					&dev,&img->GetAnvilImage(),createInfo.baseLayer.has_value() ? *createInfo.baseLayer : 0u,createInfo.baseMipmap,umath::min(createInfo.baseMipmap +createInfo.mipmapLevels,(*img)->get_n_mipmaps()) -createInfo.baseMipmap,
					aspectMask,format,
					createInfo.swizzleRed,createInfo.swizzleGreen,
					createInfo.swizzleBlue,createInfo.swizzleAlpha
				)
			));
		case Anvil::ImageViewType::_2D_ARRAY:
			return ImageView::Create(prosper::Context::GetContext(dev),*img,Anvil::ImageView::create(
				Anvil::ImageViewCreateInfo::create_2D_array(
					&dev,&img->GetAnvilImage(),createInfo.baseLayer.has_value() ? *createInfo.baseLayer : 0u,numLayers,createInfo.baseMipmap,umath::min(createInfo.baseMipmap +createInfo.mipmapLevels,(*img)->get_n_mipmaps()) -createInfo.baseMipmap,
					aspectMask,format,
					createInfo.swizzleRed,createInfo.swizzleGreen,
					createInfo.swizzleBlue,createInfo.swizzleAlpha
				)
			));
		default:
			throw std::invalid_argument("Image view type " +std::to_string(umath::to_integral(type)) +" is currently unsupported!");
	}
}

Anvil::Format prosper::util::get_vk_format(uimg::ImageBuffer::Format format)
{
	switch(format)
	{
	case uimg::ImageBuffer::Format::RGB8:
		return Anvil::Format::R8G8B8_UNORM;
	case uimg::ImageBuffer::Format::RGBA8:
		return Anvil::Format::R8G8B8A8_UNORM;
	case uimg::ImageBuffer::Format::RGB16:
		return Anvil::Format::R16G16B16_UNORM;
	case uimg::ImageBuffer::Format::RGBA16:
		return Anvil::Format::R16G16B16A16_UNORM;
	case uimg::ImageBuffer::Format::RGB32:
		return Anvil::Format::R32G32B32_SFLOAT;
	case uimg::ImageBuffer::Format::RGBA32:
		return Anvil::Format::R32G32B32A32_SFLOAT;
	}
	static_assert(umath::to_integral(uimg::ImageBuffer::Format::Count) == 7);
	return Anvil::Format::UNKNOWN;
}

void prosper::util::initialize_image(Anvil::BaseDevice &dev,const uimg::ImageBuffer &imgSrc,Anvil::Image &img)
{
	auto extents = img.get_image_extent_2D(0u);
	auto w = extents.width;
	auto h = extents.height;
	auto srcFormat = img.get_create_info_ptr()->get_format();
	if(srcFormat != Anvil::Format::R8G8B8A8_UNORM && srcFormat != Anvil::Format::R8G8B8_UNORM)
		throw std::invalid_argument("Invalid image format for tga target image: Only VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R8G8B8_UNORM are supported!");
	if(imgSrc.GetWidth() != w || imgSrc.GetHeight() != h)
		throw std::invalid_argument("Invalid image extents for tga target image: Extents must be the same for source tga image and target image!");

	auto memBlock = img.get_memory_block();
	auto size = w *h *get_byte_size(srcFormat);
	uint8_t *outDataPtr = nullptr;
	memBlock->map(0ull,size,reinterpret_cast<void**>(&outDataPtr));

	auto *imgData = static_cast<const uint8_t*>(imgSrc.GetData());
	auto pos = decltype(imgSrc.GetSize()){0};
	auto tgaFormat = imgSrc.GetFormat();
	auto srcPxSize = get_byte_size(srcFormat);
	auto rowPitch = w *srcPxSize;
	for(auto y=h;y>0;)
	{
		--y;
		auto *row = outDataPtr +y *rowPitch;
		for(auto x=decltype(w){0};x<w;++x)
		{
			auto *px = row;
			px[0] = imgData[pos];
			px[1] = imgData[pos +1];
			px[2] = imgData[pos +2];
			switch(tgaFormat)
			{
			case uimg::ImageBuffer::Format::RGBA8:
				{
					pos += 4;
					if(srcFormat == Anvil::Format::R8G8B8A8_UNORM)
						px[3] = imgData[pos +3];
					break;
				}
				default:
				{
					if(srcFormat == Anvil::Format::R8G8B8A8_UNORM)
						px[3] = std::numeric_limits<uint8_t>::max();
					pos += 3;
					break;
				}
			}
			row += srcPxSize;
		}
	}

	memBlock->unmap();
}

bool prosper::util::record_generate_mipmaps(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,Anvil::ImageLayout currentLayout,Anvil::AccessFlags srcAccessMask,Anvil::PipelineStageFlags srcStage)
{
	auto blitInfo = prosper::util::BlitInfo {};
	auto numMipmaps = img.get_n_mipmaps();
	auto numLayers = img.get_create_info_ptr()->get_n_layers();
	prosper::util::PipelineBarrierInfo barrierInfo {};
	barrierInfo.srcStageMask = srcStage;
	barrierInfo.dstStageMask = Anvil::PipelineStageFlagBits::TRANSFER_BIT;

	prosper::util::ImageBarrierInfo imgBarrierInfo {};
	imgBarrierInfo.subresourceRange.levelCount = 1u;
	imgBarrierInfo.subresourceRange.baseArrayLayer = 0u;
	imgBarrierInfo.subresourceRange.layerCount = numLayers;
	imgBarrierInfo.oldLayout = currentLayout;
	imgBarrierInfo.newLayout = Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL;
	imgBarrierInfo.srcAccessMask = srcAccessMask;
	imgBarrierInfo.dstAccessMask = Anvil::AccessFlagBits::TRANSFER_READ_BIT;
	imgBarrierInfo.subresourceRange.baseMipLevel = 0u;

	barrierInfo.imageBarriers = decltype(barrierInfo.imageBarriers){create_image_barrier(img,imgBarrierInfo)};
	if(record_pipeline_barrier(cmdBuffer,barrierInfo) == false) // Move first mipmap into transfer-src layout
		return false;

	imgBarrierInfo.subresourceRange.baseMipLevel = 1u;
	imgBarrierInfo.subresourceRange.levelCount = img.get_n_mipmaps() -1;
	imgBarrierInfo.newLayout = Anvil::ImageLayout::TRANSFER_DST_OPTIMAL;
	imgBarrierInfo.dstAccessMask = Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;
	barrierInfo.imageBarriers = decltype(barrierInfo.imageBarriers){create_image_barrier(img,imgBarrierInfo)};
	if(record_pipeline_barrier(cmdBuffer,barrierInfo) == false) // Move other mipmaps into transfer-dst layout
		return false;

	barrierInfo.srcStageMask = Anvil::PipelineStageFlagBits::TRANSFER_BIT;

	imgBarrierInfo.subresourceRange.levelCount = 1u;
	imgBarrierInfo.subresourceRange.layerCount = 1u;
	imgBarrierInfo.oldLayout = Anvil::ImageLayout::TRANSFER_DST_OPTIMAL;
	imgBarrierInfo.newLayout = Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL;
	imgBarrierInfo.srcAccessMask = Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;
	imgBarrierInfo.dstAccessMask = Anvil::AccessFlagBits::TRANSFER_READ_BIT;

	for(auto iLayer=decltype(numLayers){0u};iLayer<numLayers;++iLayer)
	{
		imgBarrierInfo.subresourceRange.baseArrayLayer = iLayer;
		for(auto i=decltype(numMipmaps){1};i<numMipmaps;++i)
		{
			blitInfo.srcSubresourceLayer.mip_level = i -1u;
			blitInfo.dstSubresourceLayer.mip_level = i;

			blitInfo.srcSubresourceLayer.base_array_layer = iLayer;
			blitInfo.dstSubresourceLayer.base_array_layer = iLayer;

			imgBarrierInfo.subresourceRange.baseMipLevel = i;
			barrierInfo.imageBarriers = decltype(barrierInfo.imageBarriers){create_image_barrier(img,imgBarrierInfo)};
			if(record_blit_image(cmdBuffer,blitInfo,img,img) == false || record_pipeline_barrier(cmdBuffer,barrierInfo) == false)
				return false;
		}
	}
	return record_image_barrier(cmdBuffer,img,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
}

bool prosper::util::record_set_viewport(Anvil::CommandBufferBase &cmdBuffer,uint32_t width,uint32_t height,uint32_t x,uint32_t y)
{
	auto vp = vk::Viewport(x,y,width,height);
	return cmdBuffer.record_set_viewport(0u,1u,reinterpret_cast<VkViewport*>(&vp));
}
bool prosper::util::record_set_scissor(Anvil::CommandBufferBase &cmdBuffer,uint32_t width,uint32_t height,uint32_t x,uint32_t y)
{
	auto scissor = vk::Rect2D(vk::Offset2D(x,y),vk::Extent2D(width,height));
	return cmdBuffer.record_set_scissor(0u,1u,reinterpret_cast<VkRect2D*>(&scissor));
}

struct DebugRenderTargetInfo
{
	std::weak_ptr<prosper::RenderPass> renderPass;
	uint32_t baseLayer;
	std::weak_ptr<prosper::Image> image;
	std::weak_ptr<prosper::Framebuffer> framebuffer;
	std::weak_ptr<prosper::RenderTarget> renderTarget;
};
static std::unordered_map<Anvil::PrimaryCommandBuffer*,DebugRenderTargetInfo> s_wpCurrentRenderTargets = {};
bool prosper::util::get_current_render_pass_target(
	Anvil::PrimaryCommandBuffer &cmdBuffer,prosper::RenderPass **outRp,
	prosper::Image **outImg,prosper::Framebuffer **outFb,prosper::RenderTarget **outRt
)
{
	auto it = s_wpCurrentRenderTargets.find(&cmdBuffer);
	if(it == s_wpCurrentRenderTargets.end())
		return false;
	auto &dbgInfo = it->second;
	if(outRp)
		*outRp = dbgInfo.renderPass.lock().get();
	if(outImg)
		*outImg = dbgInfo.image.lock().get();
	if(outFb)
		*outFb = dbgInfo.framebuffer.lock().get();
	if(outRt)
		*outRt = dbgInfo.renderTarget.lock().get();
	return true;
}

static bool record_begin_render_pass(Anvil::PrimaryCommandBuffer &cmdBuffer,prosper::RenderTarget &rt,uint32_t *layerId,const std::vector<vk::ClearValue> &clearValues,prosper::RenderPass *rp)
{
#ifdef DEBUG_VERBOSE
	auto numAttachments = rt.GetAttachmentCount();
	std::cout<<"[VK] Beginning render pass with "<<numAttachments<<" attachments:"<<std::endl;
	for(auto i=decltype(numAttachments){0u};i<numAttachments;++i)
	{
		auto &tex = rt.GetTexture(i);
		auto img = (tex != nullptr) ? tex->GetImage() : nullptr;
		std::cout<<"\t"<<((img != nullptr) ? img->GetAnvilImage().get_image() : nullptr)<<std::endl;
	}
#endif
	auto &fb = (layerId != nullptr) ? rt.GetFramebuffer(*layerId) : rt.GetFramebuffer();
	auto &context = rt.GetContext();
	if(context.IsValidationEnabled())
	{
		auto &rpCreateInfo = *rt.GetRenderPass()->GetAnvilRenderPass().get_render_pass_create_info();
		auto numAttachments = fb->GetAttachmentCount();
		for(auto i=decltype(numAttachments){0};i<numAttachments;++i)
		{
			auto *imgView = fb->GetAttachment(i);
			if(imgView == nullptr)
				continue;
			auto &img = imgView->GetImage();
			Anvil::AttachmentType attType;
			if(rpCreateInfo.get_attachment_type(i,&attType) == false)
				continue;
			Anvil::ImageLayout initialLayout;
			auto bValid = false;
			switch(attType)
			{
				case Anvil::AttachmentType::COLOR:
					bValid = rpCreateInfo.get_color_attachment_properties(i,nullptr,nullptr,nullptr,nullptr,&initialLayout,nullptr);
					break;
				case Anvil::AttachmentType::DEPTH_STENCIL:
					bValid = rpCreateInfo.get_depth_stencil_attachment_properties(i,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&initialLayout,nullptr);
					break;
			}
			if(bValid == false)
				continue;
			Anvil::ImageLayout imgLayout;
			if(prosper::debug::get_last_recorded_image_layout(cmdBuffer,*img,imgLayout,(layerId != nullptr) ? *layerId : 0u) == false)
				continue;
			if(imgLayout != initialLayout)
			{
				debug::exec_debug_validation_callback(
					vk::DebugReportObjectTypeEXT::eImage,"Begin render pass: Image 0x" +::util::to_hex_string(reinterpret_cast<uint64_t>(img.GetAnvilImage().get_image())) +
					" has to be in layout " +vk::to_string(static_cast<vk::ImageLayout>(initialLayout)) +", but is in layout " +
					vk::to_string(static_cast<vk::ImageLayout>(imgLayout)) +"!"
				);
			}
		}
	}
	auto it = s_wpCurrentRenderTargets.find(&cmdBuffer);
	if(it != s_wpCurrentRenderTargets.end())
		s_wpCurrentRenderTargets.erase(it);
	if(rp == nullptr)
		rp = rt.GetRenderPass().get();
	auto &tex = rt.GetTexture();
	if(fb == nullptr)
		throw std::runtime_error("Attempted to begin render pass with NULL framebuffer object!");
	if(rp == nullptr)
		throw std::runtime_error("Attempted to begin render pass with NULL render pass object!");
	if(tex == nullptr)
		throw std::runtime_error("Attempted to begin render pass with NULL texture object!");
	auto &img = *tex->GetImage();
	auto extents = img->get_image_extent_2D(0u);
	auto renderArea = vk::Rect2D(vk::Offset2D(),extents);
	s_wpCurrentRenderTargets[&cmdBuffer] = {rp->shared_from_this(),(layerId != nullptr) ? *layerId : std::numeric_limits<uint32_t>::max(),img.shared_from_this(),fb,rt.shared_from_this()};
	return cmdBuffer.record_begin_render_pass(
		clearValues.size(),reinterpret_cast<const VkClearValue*>(clearValues.data()),
		&fb->GetAnvilFramebuffer(),renderArea,&rp->GetAnvilRenderPass(),Anvil::SubpassContents::INLINE
	);
}

bool prosper::util::record_begin_render_pass(Anvil::PrimaryCommandBuffer &cmdBuffer,prosper::Image &img,prosper::RenderPass &rp,prosper::Framebuffer &fb,const std::vector<vk::ClearValue> &clearValues)
{
	s_wpCurrentRenderTargets[&cmdBuffer] = {rp.shared_from_this(),0u,img.shared_from_this(),fb.shared_from_this(),std::weak_ptr<prosper::RenderTarget>{}};

	auto extents = img->get_image_extent_2D(0u);
	auto renderArea = vk::Rect2D(vk::Offset2D(),extents);
	return cmdBuffer.record_begin_render_pass(
		clearValues.size(),reinterpret_cast<const VkClearValue*>(clearValues.data()),
		&fb.GetAnvilFramebuffer(),renderArea,&rp.GetAnvilRenderPass(),Anvil::SubpassContents::INLINE
	);
}

bool prosper::util::record_begin_render_pass(Anvil::PrimaryCommandBuffer &cmdBuffer,prosper::RenderTarget &rt,uint32_t layerId,const std::vector<vk::ClearValue> &clearValues,prosper::RenderPass *rp)
{
	return ::record_begin_render_pass(cmdBuffer,rt,&layerId,clearValues,rp);
}

bool prosper::util::record_begin_render_pass(Anvil::PrimaryCommandBuffer &cmdBuffer,prosper::RenderTarget &rt,uint32_t layerId,const vk::ClearValue *clearValue,prosper::RenderPass *rp)
{
	return ::record_begin_render_pass(cmdBuffer,rt,&layerId,(clearValue != nullptr) ? std::vector<vk::ClearValue>{*clearValue} : std::vector<vk::ClearValue>{},rp);
}
bool prosper::util::record_begin_render_pass(Anvil::PrimaryCommandBuffer &cmdBuffer,prosper::RenderTarget &rt,const std::vector<vk::ClearValue> &clearValues,prosper::RenderPass *rp)
{
	return ::record_begin_render_pass(cmdBuffer,rt,nullptr,clearValues,rp);
}

bool prosper::util::record_begin_render_pass(Anvil::PrimaryCommandBuffer &cmdBuffer,prosper::RenderTarget &rt,const vk::ClearValue *clearValue,prosper::RenderPass *rp)
{
	return ::record_begin_render_pass(cmdBuffer,rt,nullptr,(clearValue != nullptr) ? std::vector<vk::ClearValue>{*clearValue} : std::vector<vk::ClearValue>{},rp);
}
bool prosper::util::record_end_render_pass(Anvil::PrimaryCommandBuffer &cmdBuffer)
{
#ifdef DEBUG_VERBOSE
	std::cout<<"[VK] Ending render pass..."<<std::endl;
#endif
	auto it = s_wpCurrentRenderTargets.find(&cmdBuffer);
	if(it != s_wpCurrentRenderTargets.end())
	{
		auto rp = it->second.renderPass.lock();
		auto fb = it->second.framebuffer.lock();
		if(rp && fb)
		{
			auto &context = rp->GetContext();
			auto rt = it->second.renderTarget.lock();
			if(context.IsValidationEnabled())
			{
				auto &rpCreateInfo = *rp->GetAnvilRenderPass().get_render_pass_create_info();
				auto numAttachments = fb->GetAttachmentCount();
				for(auto i=decltype(numAttachments){0};i<numAttachments;++i)
				{
					auto *imgView = fb->GetAttachment(i);
					if(!imgView)
						continue;
					auto &img = imgView->GetImage();
					Anvil::AttachmentType attType;
					if(rpCreateInfo.get_attachment_type(i,&attType) == false)
						continue;
					Anvil::ImageLayout finalLayout;
					auto bValid = false;
					switch(attType)
					{
						case Anvil::AttachmentType::COLOR:
							bValid = rpCreateInfo.get_color_attachment_properties(i,nullptr,nullptr,nullptr,nullptr,nullptr,&finalLayout);
							break;
						case Anvil::AttachmentType::DEPTH_STENCIL:
							bValid = rpCreateInfo.get_depth_stencil_attachment_properties(i,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&finalLayout);
							break;
					}
					if(bValid == false)
						continue;
					auto layerCount = umath::min(imgView->GetLayerCount(),fb->GetLayerCount());
					auto baseLayer = imgView->GetBaseLayer();
					prosper::debug::set_last_recorded_image_layout(cmdBuffer,*img,finalLayout,baseLayer,layerCount);
				}
			}
		}
		s_wpCurrentRenderTargets.erase(it);
	}
	return cmdBuffer.record_end_render_pass();
}
bool prosper::util::record_next_sub_pass(Anvil::PrimaryCommandBuffer &cmdBuffer)
{
	return cmdBuffer.record_next_subpass(Anvil::SubpassContents::INLINE);
}

bool prosper::util::set_descriptor_set_binding_storage_image(DescriptorSet &descSet,prosper::Texture &texture,uint32_t bindingIdx,uint32_t layerId)
{
	auto &imgView = texture.GetImageView(layerId);
	if(imgView == nullptr)
		return false;
	descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingStorageImage>(descSet,bindingIdx,texture,layerId));
	return descSet->set_binding_item(bindingIdx,Anvil::DescriptorSet::StorageImageBindingElement{
		Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
		&imgView->GetAnvilImageView()
	});
}
bool prosper::util::set_descriptor_set_binding_storage_image(DescriptorSet &descSet,prosper::Texture &texture,uint32_t bindingIdx)
{
	auto &imgView = texture.GetImageView();
	if(imgView == nullptr)
		return false;
	descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingStorageImage>(descSet,bindingIdx,texture));
	return descSet->set_binding_item(bindingIdx,Anvil::DescriptorSet::StorageImageBindingElement{
		Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
		&imgView->GetAnvilImageView()
	});
}
bool prosper::util::set_descriptor_set_binding_texture(DescriptorSet &descSet,prosper::Texture &texture,Anvil::BindingIndex bindingIdx,uint32_t layerId)
{
	auto imgView = texture.GetImageView(layerId);
	if(imgView == nullptr && texture.GetImage()->GetLayerCount() == 1)
		imgView = texture.GetImageView();
	auto &sampler = texture.GetSampler();
	if(imgView == nullptr || sampler == nullptr)
		return false;
	descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingTexture>(descSet,bindingIdx,texture,layerId));
	return descSet->set_binding_item(bindingIdx,Anvil::DescriptorSet::CombinedImageSamplerBindingElement{
		Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
		&imgView->GetAnvilImageView(),&sampler->GetAnvilSampler()
	});
}

bool prosper::util::set_descriptor_set_binding_texture(DescriptorSet &descSet,prosper::Texture &texture,Anvil::BindingIndex bindingIdx)
{
	auto &imgView = texture.GetImageView();
	auto &sampler = texture.GetSampler();
	if(imgView == nullptr || sampler == nullptr)
		return false;
	descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingTexture>(descSet,bindingIdx,texture));
	return descSet->set_binding_item(bindingIdx,Anvil::DescriptorSet::CombinedImageSamplerBindingElement{
		Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
		&imgView->GetAnvilImageView(),&sampler->GetAnvilSampler()
	});
}

bool prosper::util::set_descriptor_set_binding_array_texture(DescriptorSet &descSet,prosper::Texture &texture,uint32_t bindingIdx,uint32_t arrayIndex,uint32_t layerId)
{
	std::vector<Anvil::DescriptorSet::CombinedImageSamplerBindingElement> bindingElements(1u,Anvil::DescriptorSet::CombinedImageSamplerBindingElement{
		Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
		&texture.GetImageView(layerId)->GetAnvilImageView(),&texture.GetSampler()->GetAnvilSampler()
	});
	auto *binding = descSet.GetBinding(bindingIdx);
	if(binding == nullptr)
		binding = &descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingArrayTexture>(descSet,bindingIdx));
	if(binding && binding->GetType() == prosper::DescriptorSetBinding::Type::ArrayTexture)
		static_cast<prosper::DescriptorSetBindingArrayTexture*>(binding)->SetArrayBinding(arrayIndex,std::make_unique<DescriptorSetBindingTexture>(descSet,bindingIdx,texture,layerId));
	return descSet->set_binding_array_items(bindingIdx,{arrayIndex,1u},bindingElements.data());
}

bool prosper::util::set_descriptor_set_binding_array_texture(DescriptorSet &descSet,prosper::Texture &texture,uint32_t bindingIdx,uint32_t arrayIndex)
{
	std::vector<Anvil::DescriptorSet::CombinedImageSamplerBindingElement> bindingElements(1u,Anvil::DescriptorSet::CombinedImageSamplerBindingElement{
		Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
		&texture.GetImageView()->GetAnvilImageView(),&texture.GetSampler()->GetAnvilSampler()
	});
	auto *binding = descSet.GetBinding(bindingIdx);
	if(binding == nullptr)
		binding = &descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingArrayTexture>(descSet,bindingIdx));
	if(binding && binding->GetType() == prosper::DescriptorSetBinding::Type::ArrayTexture)
		static_cast<prosper::DescriptorSetBindingArrayTexture*>(binding)->SetArrayBinding(arrayIndex,std::make_unique<DescriptorSetBindingTexture>(descSet,bindingIdx,texture));
	return descSet->set_binding_array_items(bindingIdx,{arrayIndex,1u},bindingElements.data());
}

bool prosper::util::set_descriptor_set_binding_uniform_buffer(DescriptorSet &descSet,prosper::Buffer &buffer,uint32_t bindingIdx,uint64_t startOffset,uint64_t size)
{
	size = (size != std::numeric_limits<decltype(size)>::max()) ? size : buffer.GetSize();
	descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingUniformBuffer>(descSet,bindingIdx,buffer,startOffset,size));
	return descSet->set_binding_item(bindingIdx,Anvil::DescriptorSet::UniformBufferBindingElement{
		&buffer.GetBaseAnvilBuffer(),buffer.GetStartOffset() +startOffset,size
	});
}

bool prosper::util::set_descriptor_set_binding_dynamic_uniform_buffer(DescriptorSet &descSet,prosper::Buffer &buffer,uint32_t bindingIdx,uint64_t startOffset,uint64_t size)
{
	size = (size != std::numeric_limits<decltype(size)>::max()) ? size : buffer.GetSize();
	descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingDynamicUniformBuffer>(descSet,bindingIdx,buffer,startOffset,size));
	return descSet->set_binding_item(bindingIdx,Anvil::DescriptorSet::DynamicUniformBufferBindingElement{
		&buffer.GetBaseAnvilBuffer(),buffer.GetStartOffset() +startOffset,size
	});
}

bool prosper::util::set_descriptor_set_binding_storage_buffer(DescriptorSet &descSet,prosper::Buffer &buffer,uint32_t bindingIdx,uint64_t startOffset,uint64_t size)
{
	size = (size != std::numeric_limits<decltype(size)>::max()) ? size : buffer.GetSize();
	descSet.SetBinding(bindingIdx,std::make_unique<DescriptorSetBindingStorageBuffer>(descSet,bindingIdx,buffer,startOffset,size));
	return descSet->set_binding_item(bindingIdx,Anvil::DescriptorSet::StorageBufferBindingElement{
		&buffer.GetBaseAnvilBuffer(),buffer.GetStartOffset() +startOffset,size
	});
}

bool prosper::util::record_set_depth_bias(Anvil::CommandBufferBase &cmdBuffer,float depthBiasConstantFactor,float depthBiasClamp,float depthBiasSlopeFactor)
{
	return cmdBuffer.record_set_depth_bias(depthBiasConstantFactor,depthBiasClamp,depthBiasSlopeFactor);
}

bool prosper::util::record_clear_image(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,Anvil::ImageLayout layout,const std::array<float,4> &clearColor,const ClearImageInfo clearImageInfo)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) != nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");
	// if(cmdBuffer.GetContext().IsValidationEnabled() && is_compressed_format(img.get_create_info_ptr()->get_format()))
	// 	throw std::logic_error("Attempted to clear image with compressed image format! This is not allowed!");

	Anvil::ImageSubresourceRange resourceRange {};
	resourceRange.aspect_mask = get_aspect_mask(img);
	clearImageInfo.subresourceRange.ApplyRange(resourceRange,img);

	vk::ClearColorValue clearColorVal {clearColor};
	return cmdBuffer.record_clear_color_image(
		&img,layout,reinterpret_cast<VkClearColorValue*>(&clearColorVal),
		1u,&resourceRange
	);
}
bool prosper::util::record_clear_image(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,Anvil::ImageLayout layout,float clearDepth,const ClearImageInfo clearImageInfo)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) != nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");
	// if(cmdBuffer.GetContext().IsValidationEnabled() && is_compressed_format(img.get_create_info_ptr()->get_format()))
	// 	throw std::logic_error("Attempted to clear image with compressed image format! This is not allowed!");

	Anvil::ImageSubresourceRange resourceRange {};
	resourceRange.aspect_mask = get_aspect_mask(img);
	clearImageInfo.subresourceRange.ApplyRange(resourceRange,img);

	vk::ClearDepthStencilValue clearDepthValue {clearDepth};
	return cmdBuffer.record_clear_depth_stencil_image(
		&img,layout,reinterpret_cast<VkClearDepthStencilValue*>(&clearDepthValue),
		1u,&resourceRange
	);
}

static bool record_clear_color_attachment(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,const std::array<float,4> &clearColor,uint32_t attId,uint32_t baseLayer,uint32_t numLayers)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) == nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	vk::ClearValue clearVal {vk::ClearColorValue{clearColor}};
	Anvil::ClearAttachment clearAtt {Anvil::ImageAspectFlagBits::COLOR_BIT,attId,clearVal};
	vk::ClearRect clearRect {
		vk::Rect2D{vk::Offset2D{0,0},img.get_image_extent_2D(0u)},
		baseLayer,numLayers
	};
	return cmdBuffer.record_clear_attachments(1u,&clearAtt,1u,reinterpret_cast<VkClearRect*>(&clearRect));
}
bool prosper::util::record_clear_attachment(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,const std::array<float,4> &clearColor,uint32_t attId)
{
	return record_clear_color_attachment(cmdBuffer,img,clearColor,attId,0u,img.get_create_info_ptr()->get_n_layers());
}
bool prosper::util::record_clear_attachment(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,const std::array<float,4> &clearColor,uint32_t attId,uint32_t layerId)
{
	return record_clear_color_attachment(cmdBuffer,img,clearColor,attId,layerId,1u);
}

static bool record_clear_depth_attachment(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,float clearDepth,uint32_t baseLayer,uint32_t numLayers)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) == nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	vk::ClearValue clearVal {vk::ClearDepthStencilValue{clearDepth}};
	Anvil::ClearAttachment clearAtt {Anvil::ImageAspectFlagBits::DEPTH_BIT,0u /* color attachment */,clearVal};
	vk::ClearRect clearRect {
		vk::Rect2D{vk::Offset2D{0,0},img.get_image_extent_2D(0u)},
		baseLayer,numLayers
	};
	return cmdBuffer.record_clear_attachments(1u,&clearAtt,1u,reinterpret_cast<VkClearRect*>(&clearRect));
}
bool prosper::util::record_clear_attachment(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,float clearDepth,uint32_t layerId)
{
	return record_clear_depth_attachment(cmdBuffer,img,clearDepth,layerId,1u);
}

bool prosper::util::record_clear_attachment(Anvil::CommandBufferBase &cmdBuffer,Anvil::Image &img,float clearDepth)
{
	return record_clear_depth_attachment(cmdBuffer,img,clearDepth,0u,img.get_create_info_ptr()->get_n_layers());
}

bool prosper::util::record_copy_image(Anvil::CommandBufferBase &cmdBuffer,const CopyInfo &copyInfo,Anvil::Image &imgSrc,Anvil::Image &imgDst)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) != nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	auto width = copyInfo.width;
	if(width == std::numeric_limits<decltype(width)>::max())
		width = imgSrc.get_image_extent_2D(0u).width;
	auto height = copyInfo.height;
	if(height == std::numeric_limits<decltype(height)>::max())
		height = imgSrc.get_image_extent_2D(0u).height;

	vk::Extent3D extent(width,height,1);
	Anvil::ImageCopy copyRegion{copyInfo.srcSubresource,copyInfo.srcOffset,copyInfo.dstSubresource,copyInfo.dstOffset,extent};
	return cmdBuffer.record_copy_image(
		&imgSrc,copyInfo.srcImageLayout,
		&imgDst,copyInfo.dstImageLayout,
		1,&copyRegion
	);
}

bool prosper::util::record_copy_buffer_to_image(Anvil::CommandBufferBase &cmdBuffer,const BufferImageCopyInfo &copyInfo,Buffer &bufferSrc,Anvil::Image &imgDst)
{
	if(bufferSrc.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)))
		throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	uint32_t w,h;
	imgDst.get_image_mipmap_size(0,&w,&h,nullptr);
	if(copyInfo.width.has_value())
		w = *copyInfo.width;
	if(copyInfo.height.has_value())
		h = *copyInfo.height;
	Anvil::BufferImageCopy bufferImageCopy {};
	bufferImageCopy.buffer_offset = bufferSrc.GetStartOffset() +copyInfo.bufferOffset;
	bufferImageCopy.image_extent = vk::Extent3D(w,h,1);
	bufferImageCopy.image_subresource = Anvil::ImageSubresourceLayers{copyInfo.aspectMask,copyInfo.mipLevel,copyInfo.baseArrayLayer,copyInfo.layerCount};
	return cmdBuffer.record_copy_buffer_to_image(
		&bufferSrc.GetBaseAnvilBuffer(),&imgDst,
		copyInfo.dstImageLayout,1u,&bufferImageCopy
	);
}

bool prosper::util::record_copy_image_to_buffer(Anvil::CommandBufferBase &cmdBuffer,const BufferImageCopyInfo &copyInfo,Anvil::Image &imgSrc,Anvil::ImageLayout srcImageLayout,Buffer &bufferDst)
{
	if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)))
		throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	uint32_t w,h;
	imgSrc.get_image_mipmap_size(copyInfo.mipLevel,&w,&h,nullptr);
	if(copyInfo.width.has_value())
		w = *copyInfo.width;
	if(copyInfo.height.has_value())
		h = *copyInfo.height;
	Anvil::BufferImageCopy bufferImageCopy {};
	bufferImageCopy.buffer_offset = bufferDst.GetStartOffset() +copyInfo.bufferOffset;
	bufferImageCopy.image_extent = vk::Extent3D(w,h,1);
	bufferImageCopy.image_subresource = Anvil::ImageSubresourceLayers{copyInfo.aspectMask,copyInfo.mipLevel,copyInfo.baseArrayLayer,copyInfo.layerCount};
	return cmdBuffer.record_copy_image_to_buffer(
		&imgSrc,srcImageLayout,&bufferDst.GetAnvilBuffer(),1u,&bufferImageCopy
	);
}

bool prosper::util::record_copy_buffer(Anvil::CommandBufferBase &cmdBuffer,const Anvil::BufferCopy &copyInfo,Buffer &bufferSrc,Buffer &bufferDst)
{
	if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)))
		throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	auto ci = copyInfo;
	ci.src_offset += bufferSrc.GetStartOffset();
	ci.dst_offset += bufferDst.GetStartOffset();
	return cmdBuffer.record_copy_buffer(&bufferSrc.GetBaseAnvilBuffer(),&bufferDst.GetBaseAnvilBuffer(),1u,&ci);
}

bool prosper::util::record_update_buffer(Anvil::CommandBufferBase &cmdBuffer,Buffer &buffer,uint64_t offset,uint64_t size,const void *data)
{
	if(buffer.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)))
		throw std::logic_error("Attempted to update buffer while render pass is active!");
	return cmdBuffer.record_update_buffer(&buffer.GetBaseAnvilBuffer(),buffer.GetStartOffset() +offset,size,reinterpret_cast<const uint32_t*>(data));
}

bool prosper::util::record_update_generic_shader_read_buffer(Anvil::CommandBufferBase &cmdBuffer,Buffer &buffer,uint64_t offset,uint64_t size,const void *data)
{
	return record_buffer_barrier(
		cmdBuffer,buffer,
		Anvil::PipelineStageFlagBits::FRAGMENT_SHADER_BIT | Anvil::PipelineStageFlagBits::VERTEX_SHADER_BIT | Anvil::PipelineStageFlagBits::COMPUTE_SHADER_BIT | Anvil::PipelineStageFlagBits::GEOMETRY_SHADER_BIT,
		Anvil::PipelineStageFlagBits::TRANSFER_BIT,
		Anvil::AccessFlagBits::SHADER_READ_BIT,Anvil::AccessFlagBits::TRANSFER_WRITE_BIT,
		offset,size
	) &&
		prosper::util::record_update_buffer(cmdBuffer,buffer,offset,size,data) &&
		record_buffer_barrier(
			cmdBuffer,buffer,
			Anvil::PipelineStageFlagBits::TRANSFER_BIT,
			Anvil::PipelineStageFlagBits::FRAGMENT_SHADER_BIT | Anvil::PipelineStageFlagBits::VERTEX_SHADER_BIT | Anvil::PipelineStageFlagBits::COMPUTE_SHADER_BIT | Anvil::PipelineStageFlagBits::GEOMETRY_SHADER_BIT,
			Anvil::AccessFlagBits::TRANSFER_WRITE_BIT,Anvil::AccessFlagBits::SHADER_READ_BIT,
			offset,size
		);
}

uint32_t prosper::util::calculate_buffer_alignment(Anvil::BaseDevice &dev,Anvil::BufferUsageFlags usageFlags)
{
	auto alignment = 0u;
	if((usageFlags &Anvil::BufferUsageFlagBits::UNIFORM_BUFFER_BIT) != Anvil::BufferUsageFlagBits::NONE)
		alignment = dev.get_physical_device_properties().core_vk1_0_properties_ptr->limits.min_uniform_buffer_offset_alignment;
	if((usageFlags &Anvil::BufferUsageFlagBits::STORAGE_BUFFER_BIT) != Anvil::BufferUsageFlagBits::NONE)
	{
		alignment = umath::get_least_common_multiple(
			static_cast<vk::DeviceSize>(alignment),
			dev.get_physical_device_properties().core_vk1_0_properties_ptr->limits.min_storage_buffer_offset_alignment
		);
	}
	return alignment;
}

void prosper::util::calculate_mipmap_size(uint32_t w,uint32_t h,uint32_t *wMipmap,uint32_t *hMipmap,uint32_t level)
{
	*wMipmap = w;
	*hMipmap = h;
	int scale = static_cast<int>(std::pow(2,level));
	*wMipmap /= scale;
	*hMipmap /= scale;
	if(*wMipmap == 0)
		*wMipmap = 1;
	if(*hMipmap == 0)
		*hMipmap = 1;
}

uint32_t prosper::util::calculate_mipmap_size(uint32_t v,uint32_t level)
{
	auto r = v;
	auto scale = static_cast<int>(std::pow(2,level));
	r /= scale;
	if(r == 0)
		r = 1;
	return r;
}

uint32_t prosper::util::calculate_mipmap_count(uint32_t w,uint32_t h) {return 1 +static_cast<uint32_t>(floor(log2(fmaxf(static_cast<float>(w),static_cast<float>(h)))));}
std::string prosper::util::to_string(vk::Result r) {return vk::to_string(r);}

std::string prosper::util::to_string(vk::DebugReportFlagsEXT flags)
{
	auto values = umath::get_power_of_2_values(static_cast<uint32_t>(flags));
	std::string r;
	for(auto it=values.begin();it!=values.end();++it)
	{
		if(it != values.begin())
			r += " | ";
		r += prosper::util::to_string(static_cast<vk::DebugReportFlagBitsEXT>(*it));
	}
	return r;
}
std::string prosper::util::to_string(vk::DebugReportObjectTypeEXT type) {return vk::to_string(type);}
std::string prosper::util::to_string(vk::DebugReportFlagBitsEXT flags) {return vk::to_string(flags);}
std::string prosper::util::to_string(Anvil::Format format) {return vk::to_string(static_cast<vk::Format>(format));}
std::string prosper::util::to_string(Anvil::ShaderStageFlagBits shaderStage) {return vk::to_string(static_cast<vk::ShaderStageFlagBits>(shaderStage));}
std::string prosper::util::to_string(Anvil::ImageUsageFlags usage)
{
	auto values = umath::get_power_of_2_values(static_cast<uint32_t>(usage.get_vk()));
	std::string r;
	for(auto it=values.begin();it!=values.end();++it)
	{
		if(it != values.begin())
			r += " | ";
		r += prosper::util::to_string(static_cast<Anvil::ImageUsageFlagBits>(*it));
	}
	return r;
}
std::string prosper::util::to_string(Anvil::ImageCreateFlags createFlags)
{
	auto values = umath::get_power_of_2_values(static_cast<uint32_t>(createFlags.get_vk()));
	std::string r;
	for(auto it=values.begin();it!=values.end();++it)
	{
		if(it != values.begin())
			r += " | ";
		r += prosper::util::to_string(static_cast<Anvil::ImageCreateFlagBits>(*it));
	}
	return r;
}
std::string prosper::util::to_string(Anvil::ImageType type) {return vk::to_string(static_cast<vk::ImageType>(type));}
std::string prosper::util::to_string(Anvil::ImageUsageFlagBits usage) {return vk::to_string(static_cast<vk::ImageUsageFlagBits>(usage));}
std::string prosper::util::to_string(Anvil::ImageCreateFlagBits createFlags) {return vk::to_string(static_cast<vk::ImageCreateFlagBits>(createFlags));}
std::string prosper::util::to_string(Anvil::ShaderStage stage)
{
	auto values = umath::get_power_of_2_values(umath::to_integral(stage));
	std::stringstream ss;
	auto bFirst = true;
	for(auto &v : values)
	{
		if(bFirst == false)
			ss<<" | ";
		else
			bFirst = false;
		ss<<vk::to_string(static_cast<vk::ShaderStageFlagBits>(v));
	}
	return ss.str();
}
std::string prosper::util::to_string(Anvil::PipelineStageFlagBits stage) {return vk::to_string(static_cast<vk::PipelineStageFlagBits>(stage));}
std::string prosper::util::to_string(Anvil::ImageTiling tiling) {return vk::to_string(static_cast<vk::ImageTiling>(tiling));}
std::string prosper::util::to_string(vk::PhysicalDeviceType type) {return vk::to_string(type);}

bool prosper::util::has_alpha(Anvil::Format format)
{
	switch(static_cast<vk::Format>(format))
	{
		case vk::Format::eBc1RgbaUnormBlock:
		case vk::Format::eBc1RgbaSrgbBlock:
		case vk::Format::eBc2UnormBlock:
		case vk::Format::eBc2SrgbBlock:
		case vk::Format::eBc3UnormBlock:
		case vk::Format::eBc3SrgbBlock:
		case vk::Format::eEtc2R8G8B8A1UnormBlock:
		case vk::Format::eEtc2R8G8B8A1SrgbBlock:
		case vk::Format::eEtc2R8G8B8A8UnormBlock:
		case vk::Format::eEtc2R8G8B8A8SrgbBlock:
		case vk::Format::eR4G4B4A4UnormPack16:
		case vk::Format::eB4G4R4A4UnormPack16:
		case vk::Format::eR5G5B5A1UnormPack16:
		case vk::Format::eB5G5R5A1UnormPack16:
		case vk::Format::eA1R5G5B5UnormPack16:
		case vk::Format::eR8G8B8A8Unorm:
		case vk::Format::eR8G8B8A8Snorm:
		case vk::Format::eR8G8B8A8Uscaled:
		case vk::Format::eR8G8B8A8Sscaled:
		case vk::Format::eR8G8B8A8Uint:
		case vk::Format::eR8G8B8A8Sint:
		case vk::Format::eR8G8B8A8Srgb:
		case vk::Format::eB8G8R8A8Unorm:
		case vk::Format::eB8G8R8A8Snorm:
		case vk::Format::eB8G8R8A8Uscaled:
		case vk::Format::eB8G8R8A8Sscaled:
		case vk::Format::eB8G8R8A8Uint:
		case vk::Format::eB8G8R8A8Sint:
		case vk::Format::eB8G8R8A8Srgb:
		case vk::Format::eA8B8G8R8UnormPack32:
		case vk::Format::eA8B8G8R8SnormPack32:
		case vk::Format::eA8B8G8R8UscaledPack32:
		case vk::Format::eA8B8G8R8SscaledPack32:
		case vk::Format::eA8B8G8R8UintPack32:
		case vk::Format::eA8B8G8R8SintPack32:
		case vk::Format::eA8B8G8R8SrgbPack32:
		case vk::Format::eA2R10G10B10UnormPack32:
		case vk::Format::eA2R10G10B10SnormPack32:
		case vk::Format::eA2R10G10B10UscaledPack32:
		case vk::Format::eA2R10G10B10SscaledPack32:
		case vk::Format::eA2R10G10B10UintPack32:
		case vk::Format::eA2R10G10B10SintPack32:
		case vk::Format::eA2B10G10R10UnormPack32:
		case vk::Format::eA2B10G10R10SnormPack32:
		case vk::Format::eA2B10G10R10UscaledPack32:
		case vk::Format::eA2B10G10R10SscaledPack32:
		case vk::Format::eA2B10G10R10UintPack32:
		case vk::Format::eA2B10G10R10SintPack32:
		case vk::Format::eR16G16B16A16Unorm:
		case vk::Format::eR16G16B16A16Snorm:
		case vk::Format::eR16G16B16A16Uscaled:
		case vk::Format::eR16G16B16A16Sscaled:
		case vk::Format::eR16G16B16A16Uint:
		case vk::Format::eR16G16B16A16Sint:
		case vk::Format::eR16G16B16A16Sfloat:
		case vk::Format::eR32G32B32A32Uint:
		case vk::Format::eR32G32B32A32Sint:
		case vk::Format::eR32G32B32A32Sfloat:
		case vk::Format::eR64G64B64A64Uint:
		case vk::Format::eR64G64B64A64Sint:
		case vk::Format::eR64G64B64A64Sfloat:
		case vk::Format::eBc7UnormBlock:
		case vk::Format::eBc7SrgbBlock:
		case vk::Format::eAstc4x4UnormBlock:
		case vk::Format::eAstc4x4SrgbBlock:
		case vk::Format::eAstc5x4UnormBlock:
		case vk::Format::eAstc5x4SrgbBlock:
		case vk::Format::eAstc5x5UnormBlock:
		case vk::Format::eAstc5x5SrgbBlock:
		case vk::Format::eAstc6x5UnormBlock:
		case vk::Format::eAstc6x5SrgbBlock:
		case vk::Format::eAstc6x6UnormBlock:
		case vk::Format::eAstc6x6SrgbBlock:
		case vk::Format::eAstc8x5UnormBlock:
		case vk::Format::eAstc8x5SrgbBlock:
		case vk::Format::eAstc8x6UnormBlock:
		case vk::Format::eAstc8x6SrgbBlock:
		case vk::Format::eAstc8x8UnormBlock:
		case vk::Format::eAstc8x8SrgbBlock:
		case vk::Format::eAstc10x5UnormBlock:
		case vk::Format::eAstc10x5SrgbBlock:
		case vk::Format::eAstc10x6UnormBlock:
		case vk::Format::eAstc10x6SrgbBlock:
		case vk::Format::eAstc10x8UnormBlock:
		case vk::Format::eAstc10x8SrgbBlock:
		case vk::Format::eAstc10x10UnormBlock:
		case vk::Format::eAstc10x10SrgbBlock:
		case vk::Format::eAstc12x10UnormBlock:
		case vk::Format::eAstc12x10SrgbBlock:
		case vk::Format::eAstc12x12UnormBlock:
		case vk::Format::eAstc12x12SrgbBlock:
			return true;
	}
	return false;
}

bool prosper::util::is_depth_format(Anvil::Format format)
{
	switch(static_cast<vk::Format>(format))
	{
		case vk::Format::eD16Unorm:
		case vk::Format::eD16UnormS8Uint:
		case vk::Format::eD24UnormS8Uint:
		case vk::Format::eD32Sfloat:
		case vk::Format::eD32SfloatS8Uint:
		case vk::Format::eX8D24UnormPack32:
			return true;
	};
	return false;
}

bool prosper::util::is_compressed_format(Anvil::Format format)
{
	switch(static_cast<vk::Format>(format))
	{
		case vk::Format::eBc1RgbUnormBlock:
		case vk::Format::eBc1RgbSrgbBlock:
		case vk::Format::eBc1RgbaUnormBlock:
		case vk::Format::eBc1RgbaSrgbBlock:
		case vk::Format::eBc2UnormBlock:
		case vk::Format::eBc2SrgbBlock:
		case vk::Format::eBc3UnormBlock:
		case vk::Format::eBc3SrgbBlock:
		case vk::Format::eBc4UnormBlock:
		case vk::Format::eBc4SnormBlock:
		case vk::Format::eBc5UnormBlock:
		case vk::Format::eBc5SnormBlock:
		case vk::Format::eBc6HUfloatBlock:
		case vk::Format::eBc6HSfloatBlock:
		case vk::Format::eBc7UnormBlock:
		case vk::Format::eBc7SrgbBlock:
		case vk::Format::eEtc2R8G8B8UnormBlock:
		case vk::Format::eEtc2R8G8B8SrgbBlock:
		case vk::Format::eEtc2R8G8B8A1UnormBlock:
		case vk::Format::eEtc2R8G8B8A1SrgbBlock:
		case vk::Format::eEtc2R8G8B8A8UnormBlock:
		case vk::Format::eEtc2R8G8B8A8SrgbBlock:
		case vk::Format::eEacR11UnormBlock:
		case vk::Format::eEacR11SnormBlock:
		case vk::Format::eEacR11G11UnormBlock:
		case vk::Format::eEacR11G11SnormBlock:
		case vk::Format::eAstc4x4UnormBlock:
		case vk::Format::eAstc4x4SrgbBlock:
		case vk::Format::eAstc5x4UnormBlock:
		case vk::Format::eAstc5x4SrgbBlock:
		case vk::Format::eAstc5x5UnormBlock:
		case vk::Format::eAstc5x5SrgbBlock:
		case vk::Format::eAstc6x5UnormBlock:
		case vk::Format::eAstc6x5SrgbBlock:
		case vk::Format::eAstc6x6UnormBlock:
		case vk::Format::eAstc6x6SrgbBlock:
		case vk::Format::eAstc8x5UnormBlock:
		case vk::Format::eAstc8x5SrgbBlock:
		case vk::Format::eAstc8x6UnormBlock:
		case vk::Format::eAstc8x6SrgbBlock:
		case vk::Format::eAstc8x8UnormBlock:
		case vk::Format::eAstc8x8SrgbBlock:
		case vk::Format::eAstc10x5UnormBlock:
		case vk::Format::eAstc10x5SrgbBlock:
		case vk::Format::eAstc10x6UnormBlock:
		case vk::Format::eAstc10x6SrgbBlock:
		case vk::Format::eAstc10x8UnormBlock:
		case vk::Format::eAstc10x8SrgbBlock:
		case vk::Format::eAstc10x10UnormBlock:
		case vk::Format::eAstc10x10SrgbBlock:
		case vk::Format::eAstc12x10UnormBlock:
		case vk::Format::eAstc12x10SrgbBlock:
		case vk::Format::eAstc12x12UnormBlock:
		case vk::Format::eAstc12x12SrgbBlock:
			return true;
	};
	return false;
}
bool prosper::util::is_uncompressed_format(Anvil::Format format)
{
	if(format == Anvil::Format::UNKNOWN)
		return false;
	return !is_compressed_format(format);
}

uint32_t prosper::util::get_bit_size(Anvil::Format format)
{
	switch(static_cast<vk::Format>(format))
	{
		// Sub-Byte formats
		case vk::Format::eUndefined:
			return 0;
		case vk::Format::eR4G4UnormPack8:
			return 8;
		case vk::Format::eR4G4B4A4UnormPack16:
		case vk::Format::eB4G4R4A4UnormPack16:
		case vk::Format::eR5G6B5UnormPack16:
		case vk::Format::eB5G6R5UnormPack16:
		case vk::Format::eR5G5B5A1UnormPack16:
		case vk::Format::eB5G5R5A1UnormPack16:
		case vk::Format::eA1R5G5B5UnormPack16:
			return 16;
		//
		case vk::Format::eR8Unorm:
		case vk::Format::eR8Snorm:
		case vk::Format::eR8Uscaled:
		case vk::Format::eR8Sscaled:
		case vk::Format::eR8Uint:
		case vk::Format::eR8Sint:
		case vk::Format::eR8Srgb:
			return 8;
		case vk::Format::eR8G8Unorm:
		case vk::Format::eR8G8Snorm:
		case vk::Format::eR8G8Uscaled:
		case vk::Format::eR8G8Sscaled:
		case vk::Format::eR8G8Uint:
		case vk::Format::eR8G8Sint:
		case vk::Format::eR8G8Srgb:
			return 16;
		case vk::Format::eR8G8B8Unorm:
		case vk::Format::eR8G8B8Snorm:
		case vk::Format::eR8G8B8Uscaled:
		case vk::Format::eR8G8B8Sscaled:
		case vk::Format::eR8G8B8Uint:
		case vk::Format::eR8G8B8Sint:
		case vk::Format::eR8G8B8Srgb:
		case vk::Format::eB8G8R8Unorm:
		case vk::Format::eB8G8R8Snorm:
		case vk::Format::eB8G8R8Uscaled:
		case vk::Format::eB8G8R8Sscaled:
		case vk::Format::eB8G8R8Uint:
		case vk::Format::eB8G8R8Sint:
		case vk::Format::eB8G8R8Srgb:
			return 24;
		case vk::Format::eR8G8B8A8Unorm:
		case vk::Format::eR8G8B8A8Snorm:
		case vk::Format::eR8G8B8A8Uscaled:
		case vk::Format::eR8G8B8A8Sscaled:
		case vk::Format::eR8G8B8A8Uint:
		case vk::Format::eR8G8B8A8Sint:
		case vk::Format::eR8G8B8A8Srgb:
		case vk::Format::eB8G8R8A8Unorm:
		case vk::Format::eB8G8R8A8Snorm:
		case vk::Format::eB8G8R8A8Uscaled:
		case vk::Format::eB8G8R8A8Sscaled:
		case vk::Format::eB8G8R8A8Uint:
		case vk::Format::eB8G8R8A8Sint:
		case vk::Format::eB8G8R8A8Srgb:
		case vk::Format::eA8B8G8R8UnormPack32:
		case vk::Format::eA8B8G8R8SnormPack32:
		case vk::Format::eA8B8G8R8UscaledPack32:
		case vk::Format::eA8B8G8R8SscaledPack32:
		case vk::Format::eA8B8G8R8UintPack32:
		case vk::Format::eA8B8G8R8SintPack32:
		case vk::Format::eA8B8G8R8SrgbPack32:
			return 32;
		case vk::Format::eA2R10G10B10UnormPack32:
		case vk::Format::eA2R10G10B10SnormPack32:
		case vk::Format::eA2R10G10B10UscaledPack32:
		case vk::Format::eA2R10G10B10SscaledPack32:
		case vk::Format::eA2R10G10B10UintPack32:
		case vk::Format::eA2R10G10B10SintPack32:
		case vk::Format::eA2B10G10R10UnormPack32:
		case vk::Format::eA2B10G10R10SnormPack32:
		case vk::Format::eA2B10G10R10UscaledPack32:
		case vk::Format::eA2B10G10R10SscaledPack32:
		case vk::Format::eA2B10G10R10UintPack32:
		case vk::Format::eA2B10G10R10SintPack32:
			return 32;
		case vk::Format::eR16Unorm:
		case vk::Format::eR16Snorm:
		case vk::Format::eR16Uscaled:
		case vk::Format::eR16Sscaled:
		case vk::Format::eR16Uint:
		case vk::Format::eR16Sint:
		case vk::Format::eR16Sfloat:
			return 16;
		case vk::Format::eR16G16Unorm:
		case vk::Format::eR16G16Snorm:
		case vk::Format::eR16G16Uscaled:
		case vk::Format::eR16G16Sscaled:
		case vk::Format::eR16G16Uint:
		case vk::Format::eR16G16Sint:
		case vk::Format::eR16G16Sfloat:
			return 32;
		case vk::Format::eR16G16B16Unorm:
		case vk::Format::eR16G16B16Snorm:
		case vk::Format::eR16G16B16Uscaled:
		case vk::Format::eR16G16B16Sscaled:
		case vk::Format::eR16G16B16Uint:
		case vk::Format::eR16G16B16Sint:
		case vk::Format::eR16G16B16Sfloat:
			return 48;
		case vk::Format::eR16G16B16A16Unorm:
		case vk::Format::eR16G16B16A16Snorm:
		case vk::Format::eR16G16B16A16Uscaled:
		case vk::Format::eR16G16B16A16Sscaled:
		case vk::Format::eR16G16B16A16Uint:
		case vk::Format::eR16G16B16A16Sint:
		case vk::Format::eR16G16B16A16Sfloat:
			return 64;
		case vk::Format::eR32Uint:
		case vk::Format::eR32Sint:
		case vk::Format::eR32Sfloat:
			return 32;
		case vk::Format::eR32G32Uint:
		case vk::Format::eR32G32Sint:
		case vk::Format::eR32G32Sfloat:
			return 64;
		case vk::Format::eR32G32B32Uint:
		case vk::Format::eR32G32B32Sint:
		case vk::Format::eR32G32B32Sfloat:
			return 96;
		case vk::Format::eR32G32B32A32Uint:
		case vk::Format::eR32G32B32A32Sint:
		case vk::Format::eR32G32B32A32Sfloat:
			return 128;
		case vk::Format::eR64Uint:
		case vk::Format::eR64Sint:
		case vk::Format::eR64Sfloat:
			return 64;
		case vk::Format::eR64G64Uint:
		case vk::Format::eR64G64Sint:
		case vk::Format::eR64G64Sfloat:
			return 128;
		case vk::Format::eR64G64B64Uint:
		case vk::Format::eR64G64B64Sint:
		case vk::Format::eR64G64B64Sfloat:
			return 192;
		case vk::Format::eR64G64B64A64Uint:
		case vk::Format::eR64G64B64A64Sint:
		case vk::Format::eR64G64B64A64Sfloat:
			return 256;
		case vk::Format::eB10G11R11UfloatPack32:
		case vk::Format::eE5B9G9R9UfloatPack32:
			return 32;
		case vk::Format::eD16Unorm:
			return 16;
		case vk::Format::eX8D24UnormPack32:
			return 32;
		case vk::Format::eD32Sfloat:
			return 32;
		case vk::Format::eS8Uint:
			return 8;
		case vk::Format::eD16UnormS8Uint:
			return 16;
		case vk::Format::eD24UnormS8Uint:
			return 24;
		case vk::Format::eD32SfloatS8Uint:
			return 32;
	};
	return 0;
}

bool prosper::util::is_8bit_format(Anvil::Format format)
{
	switch(static_cast<vk::Format>(format))
	{
	case vk::Format::eR8Unorm:
	case vk::Format::eR8Snorm:
	case vk::Format::eR8Uscaled:
	case vk::Format::eR8Sscaled:
	case vk::Format::eR8Uint:
	case vk::Format::eR8Sint:
	case vk::Format::eR8Srgb:
	case vk::Format::eR8G8Unorm:
	case vk::Format::eR8G8Snorm:
	case vk::Format::eR8G8Uscaled:
	case vk::Format::eR8G8Sscaled:
	case vk::Format::eR8G8Uint:
	case vk::Format::eR8G8Sint:
	case vk::Format::eR8G8Srgb:
	case vk::Format::eR8G8B8Unorm:
	case vk::Format::eR8G8B8Snorm:
	case vk::Format::eR8G8B8Uscaled:
	case vk::Format::eR8G8B8Sscaled:
	case vk::Format::eR8G8B8Uint:
	case vk::Format::eR8G8B8Sint:
	case vk::Format::eR8G8B8Srgb:
	case vk::Format::eB8G8R8Unorm:
	case vk::Format::eB8G8R8Snorm:
	case vk::Format::eB8G8R8Uscaled:
	case vk::Format::eB8G8R8Sscaled:
	case vk::Format::eB8G8R8Uint:
	case vk::Format::eB8G8R8Sint:
	case vk::Format::eB8G8R8Srgb:
	case vk::Format::eR8G8B8A8Unorm:
	case vk::Format::eR8G8B8A8Snorm:
	case vk::Format::eR8G8B8A8Uscaled:
	case vk::Format::eR8G8B8A8Sscaled:
	case vk::Format::eR8G8B8A8Uint:
	case vk::Format::eR8G8B8A8Sint:
	case vk::Format::eR8G8B8A8Srgb:
	case vk::Format::eB8G8R8A8Unorm:
	case vk::Format::eB8G8R8A8Snorm:
	case vk::Format::eB8G8R8A8Uscaled:
	case vk::Format::eB8G8R8A8Sscaled:
	case vk::Format::eB8G8R8A8Uint:
	case vk::Format::eB8G8R8A8Sint:
	case vk::Format::eB8G8R8A8Srgb:
	case vk::Format::eA8B8G8R8UnormPack32:
	case vk::Format::eA8B8G8R8SnormPack32:
	case vk::Format::eA8B8G8R8UscaledPack32:
	case vk::Format::eA8B8G8R8SscaledPack32:
	case vk::Format::eA8B8G8R8UintPack32:
	case vk::Format::eA8B8G8R8SintPack32:
	case vk::Format::eA8B8G8R8SrgbPack32:
	case vk::Format::eS8Uint:
		return true;
	};
	return false;
}
bool prosper::util::is_16bit_format(Anvil::Format format)
{
	switch(static_cast<vk::Format>(format))
	{
	case vk::Format::eR16Unorm:
	case vk::Format::eR16Snorm:
	case vk::Format::eR16Uscaled:
	case vk::Format::eR16Sscaled:
	case vk::Format::eR16Uint:
	case vk::Format::eR16Sint:
	case vk::Format::eR16Sfloat:
	case vk::Format::eR16G16Unorm:
	case vk::Format::eR16G16Snorm:
	case vk::Format::eR16G16Uscaled:
	case vk::Format::eR16G16Sscaled:
	case vk::Format::eR16G16Uint:
	case vk::Format::eR16G16Sint:
	case vk::Format::eR16G16Sfloat:
	case vk::Format::eR16G16B16Unorm:
	case vk::Format::eR16G16B16Snorm:
	case vk::Format::eR16G16B16Uscaled:
	case vk::Format::eR16G16B16Sscaled:
	case vk::Format::eR16G16B16Uint:
	case vk::Format::eR16G16B16Sint:
	case vk::Format::eR16G16B16Sfloat:
	case vk::Format::eR16G16B16A16Unorm:
	case vk::Format::eR16G16B16A16Snorm:
	case vk::Format::eR16G16B16A16Uscaled:
	case vk::Format::eR16G16B16A16Sscaled:
	case vk::Format::eR16G16B16A16Uint:
	case vk::Format::eR16G16B16A16Sint:
	case vk::Format::eR16G16B16A16Sfloat:
	case vk::Format::eD16Unorm:
	case vk::Format::eD16UnormS8Uint:
		return true;
	};
	return false;
}
bool prosper::util::is_32bit_format(Anvil::Format format)
{
	switch(static_cast<vk::Format>(format))
	{
	case vk::Format::eR16Unorm:
	case vk::Format::eR16Snorm:
	case vk::Format::eR16Uscaled:
	case vk::Format::eR16Sscaled:
	case vk::Format::eR16Uint:
	case vk::Format::eR16Sint:
	case vk::Format::eR16Sfloat:
	case vk::Format::eR16G16Unorm:
	case vk::Format::eR16G16Snorm:
	case vk::Format::eR16G16Uscaled:
	case vk::Format::eR16G16Sscaled:
	case vk::Format::eR16G16Uint:
	case vk::Format::eR16G16Sint:
	case vk::Format::eR16G16Sfloat:
	case vk::Format::eR16G16B16Unorm:
	case vk::Format::eR16G16B16Snorm:
	case vk::Format::eR16G16B16Uscaled:
	case vk::Format::eR16G16B16Sscaled:
	case vk::Format::eR16G16B16Uint:
	case vk::Format::eR16G16B16Sint:
	case vk::Format::eR16G16B16Sfloat:
	case vk::Format::eR16G16B16A16Unorm:
	case vk::Format::eR16G16B16A16Snorm:
	case vk::Format::eR16G16B16A16Uscaled:
	case vk::Format::eR16G16B16A16Sscaled:
	case vk::Format::eR16G16B16A16Uint:
	case vk::Format::eR16G16B16A16Sint:
	case vk::Format::eR16G16B16A16Sfloat:
	case vk::Format::eD16Unorm:
	case vk::Format::eD16UnormS8Uint:
		return true;
	};
	return false;
}
bool prosper::util::is_64bit_format(Anvil::Format format)
{
	switch(static_cast<vk::Format>(format))
	{
	case vk::Format::eR64Uint:
	case vk::Format::eR64Sint:
	case vk::Format::eR64Sfloat:
	case vk::Format::eR64G64Uint:
	case vk::Format::eR64G64Sint:
	case vk::Format::eR64G64Sfloat:
	case vk::Format::eR64G64B64Uint:
	case vk::Format::eR64G64B64Sint:
	case vk::Format::eR64G64B64Sfloat:
	case vk::Format::eR64G64B64A64Uint:
	case vk::Format::eR64G64B64A64Sint:
	case vk::Format::eR64G64B64A64Sfloat:
		return true;
	};
	return false;
}

uint32_t prosper::util::get_byte_size(Anvil::Format format)
{
	auto numBits = get_bit_size(format);
	if(numBits == 0 || (numBits %8) != 0)
		return 0;
	return numBits /8;
}

Anvil::AccessFlags prosper::util::get_read_access_mask() {return Anvil::AccessFlagBits::COLOR_ATTACHMENT_READ_BIT | Anvil::AccessFlagBits::DEPTH_STENCIL_ATTACHMENT_READ_BIT | Anvil::AccessFlagBits::HOST_READ_BIT | Anvil::AccessFlagBits::INDEX_READ_BIT | Anvil::AccessFlagBits::INDIRECT_COMMAND_READ_BIT | Anvil::AccessFlagBits::INPUT_ATTACHMENT_READ_BIT | Anvil::AccessFlagBits::MEMORY_READ_BIT | Anvil::AccessFlagBits::SHADER_READ_BIT | Anvil::AccessFlagBits::TRANSFER_READ_BIT | Anvil::AccessFlagBits::UNIFORM_READ_BIT | Anvil::AccessFlagBits::VERTEX_ATTRIBUTE_READ_BIT;}
Anvil::AccessFlags prosper::util::get_write_access_mask() {return Anvil::AccessFlagBits::COLOR_ATTACHMENT_WRITE_BIT | Anvil::AccessFlagBits::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | Anvil::AccessFlagBits::HOST_WRITE_BIT | Anvil::AccessFlagBits::MEMORY_WRITE_BIT | Anvil::AccessFlagBits::SHADER_WRITE_BIT | Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;}

Anvil::AccessFlags prosper::util::get_image_read_access_mask() {return Anvil::AccessFlagBits::COLOR_ATTACHMENT_READ_BIT | Anvil::AccessFlagBits::HOST_READ_BIT | Anvil::AccessFlagBits::MEMORY_READ_BIT | Anvil::AccessFlagBits::SHADER_READ_BIT | Anvil::AccessFlagBits::TRANSFER_READ_BIT | Anvil::AccessFlagBits::UNIFORM_READ_BIT;}
Anvil::AccessFlags prosper::util::get_image_write_access_mask() {return Anvil::AccessFlagBits::COLOR_ATTACHMENT_WRITE_BIT | Anvil::AccessFlagBits::HOST_WRITE_BIT | Anvil::AccessFlagBits::MEMORY_WRITE_BIT | Anvil::AccessFlagBits::SHADER_WRITE_BIT | Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;}

Anvil::PipelineBindPoint prosper::util::get_pipeline_bind_point(Anvil::ShaderStageFlags shaderStages)
{
	return ((shaderStages &Anvil::ShaderStageFlagBits::COMPUTE_BIT) != Anvil::ShaderStageFlagBits(0)) ? Anvil::PipelineBindPoint::COMPUTE : Anvil::PipelineBindPoint::GRAPHICS;
}

Anvil::ImageAspectFlagBits prosper::util::get_aspect_mask(Anvil::Format format)
{
	auto aspectMask = Anvil::ImageAspectFlagBits::COLOR_BIT;
	if(is_depth_format(format))
		aspectMask = Anvil::ImageAspectFlagBits::DEPTH_BIT;
	return aspectMask;
}

uint32_t prosper::util::get_offset_alignment_padding(uint32_t offset,uint32_t alignment)
{
	if(alignment == 0u)
		return 0u;
	auto r = offset %alignment;
	if(r == 0u)
		return 0u;
	return alignment -r;
}
uint32_t prosper::util::get_aligned_size(uint32_t size,uint32_t alignment)
{
	if(alignment == 0u)
		return size;
	auto r = size %alignment;
	if(r == 0u)
		return size;
	return size +(alignment -r);
}

Anvil::ImageAspectFlagBits prosper::util::get_aspect_mask(Anvil::Image &img) {return get_aspect_mask(static_cast<Anvil::Format>(img.get_create_info_ptr()->get_format()));}

void prosper::util::get_image_layout_transition_access_masks(Anvil::ImageLayout oldLayout,Anvil::ImageLayout newLayout,Anvil::AccessFlags &readAccessMask,Anvil::AccessFlags &writeAccessMask)
{
	// Source Layout
	switch(oldLayout)
	{
		// Undefined layout
		// Only allowed as initial layout!
		// Make sure any writes to the image have been finished
		case Anvil::ImageLayout::PREINITIALIZED:
			readAccessMask = Anvil::AccessFlagBits::HOST_WRITE_BIT | Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;
			break;
		// Old layout is color attachment
		// Make sure any writes to the color buffer have been finished
		case Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL:
			readAccessMask = Anvil::AccessFlagBits::COLOR_ATTACHMENT_WRITE_BIT;
			break;
		// Old layout is depth/stencil attachment
		// Make sure any writes to the depth/stencil buffer have been finished
		case Anvil::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			readAccessMask = Anvil::AccessFlagBits::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		// Old layout is transfer source
		// Make sure any reads from the image have been finished
		case Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL:
			readAccessMask = Anvil::AccessFlagBits::TRANSFER_READ_BIT;
			break;
		// Old layout is shader read (sampler, input attachment)
		// Make sure any shader reads from the image have been finished
		case Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL:
			readAccessMask = Anvil::AccessFlagBits::SHADER_READ_BIT;
			break;
	};

	// Target Layout
	switch(newLayout)
	{
		// New layout is transfer destination (copy, blit)
		// Make sure any copyies to the image have been finished
		case Anvil::ImageLayout::TRANSFER_DST_OPTIMAL:
			writeAccessMask = Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;
			break;
		// New layout is transfer source (copy, blit)
		// Make sure any reads from and writes to the image have been finished
		case Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL:
			readAccessMask = readAccessMask | Anvil::AccessFlagBits::TRANSFER_READ_BIT;
			writeAccessMask = Anvil::AccessFlagBits::TRANSFER_READ_BIT;
			break;
		// New layout is color attachment
		// Make sure any writes to the color buffer hav been finished
		case Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL:
			writeAccessMask = Anvil::AccessFlagBits::COLOR_ATTACHMENT_WRITE_BIT;
			readAccessMask = readAccessMask | Anvil::AccessFlagBits::TRANSFER_READ_BIT;
			break;
		// New layout is depth attachment
		// Make sure any writes to depth/stencil buffer have been finished
		case Anvil::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			writeAccessMask = writeAccessMask | Anvil::AccessFlagBits::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		// New layout is shader read (sampler, input attachment)
		// Make sure any writes to the image have been finished
		case Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL:
			readAccessMask = readAccessMask | Anvil::AccessFlagBits::HOST_WRITE_BIT | Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;
			writeAccessMask = Anvil::AccessFlagBits::SHADER_READ_BIT;
			break;
		case Anvil::ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			readAccessMask = readAccessMask | Anvil::AccessFlagBits::SHADER_WRITE_BIT;
			writeAccessMask = Anvil::AccessFlagBits::SHADER_READ_BIT;
			break;
	};
}

static Anvil::Format get_anvil_format(uimg::TextureInfo::InputFormat format)
{
	Anvil::Format anvFormat = Anvil::Format::R8G8B8A8_UNORM;
	switch(format)
	{
	case uimg::TextureInfo::InputFormat::R8G8B8A8_UInt:
		anvFormat = Anvil::Format::B8G8R8A8_UNORM;
		break;
	case uimg::TextureInfo::InputFormat::R16G16B16A16_Float:
		anvFormat = Anvil::Format::R16G16B16A16_SFLOAT;
		break;
	case uimg::TextureInfo::InputFormat::R32G32B32A32_Float:
		anvFormat = Anvil::Format::R32G32B32A32_SFLOAT;
		break;
	case uimg::TextureInfo::InputFormat::R32_Float:
		anvFormat = Anvil::Format::R32_SFLOAT;
		break;
	case uimg::TextureInfo::InputFormat::KeepInputImageFormat:
		break;
	}
	return anvFormat;
}
static Anvil::Format get_anvil_format(uimg::TextureInfo::OutputFormat format)
{
	Anvil::Format anvFormat = Anvil::Format::R8G8B8A8_UNORM;
	switch(format)
	{
	case uimg::TextureInfo::OutputFormat::RGB:
	case uimg::TextureInfo::OutputFormat::RGBA:
		return Anvil::Format::R8G8B8A8_UNORM;
	case uimg::TextureInfo::OutputFormat::DXT1:
	case uimg::TextureInfo::OutputFormat::BC1:
	case uimg::TextureInfo::OutputFormat::DXT1n:
	case uimg::TextureInfo::OutputFormat::CTX1:
	case uimg::TextureInfo::OutputFormat::DXT1a:
	case uimg::TextureInfo::OutputFormat::BC1a:
		return Anvil::Format::BC1_RGBA_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::DXT3:
	case uimg::TextureInfo::OutputFormat::BC2:
		return Anvil::Format::BC2_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::DXT5:
	case uimg::TextureInfo::OutputFormat::DXT5n:
	case uimg::TextureInfo::OutputFormat::BC3:
	case uimg::TextureInfo::OutputFormat::BC3n:
	case uimg::TextureInfo::OutputFormat::BC3_RGBM:
		return Anvil::Format::BC3_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::BC4:
		return Anvil::Format::BC4_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::BC5:
		return Anvil::Format::BC5_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::BC6:
		return Anvil::Format::BC6H_SFLOAT_BLOCK;
	case uimg::TextureInfo::OutputFormat::BC7:
		return Anvil::Format::BC7_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::ETC1:
	case uimg::TextureInfo::OutputFormat::ETC2_R:
	case uimg::TextureInfo::OutputFormat::ETC2_RG:
	case uimg::TextureInfo::OutputFormat::ETC2_RGB:
	case uimg::TextureInfo::OutputFormat::ETC2_RGBA:
	case uimg::TextureInfo::OutputFormat::ETC2_RGB_A1:
	case uimg::TextureInfo::OutputFormat::ETC2_RGBM:
		return Anvil::Format::ETC2_R8G8B8A8_UNORM_BLOCK;
	}
	return anvFormat;
}
static bool is_compatible(Anvil::Format imgFormat,uimg::TextureInfo::OutputFormat outputFormat)
{
	if(outputFormat == uimg::TextureInfo::OutputFormat::KeepInputImageFormat)
		return true;
	switch(outputFormat)
	{
	case uimg::TextureInfo::OutputFormat::DXT1:
	case uimg::TextureInfo::OutputFormat::DXT1a:
	case uimg::TextureInfo::OutputFormat::DXT1n:
	case uimg::TextureInfo::OutputFormat::BC1:
	case uimg::TextureInfo::OutputFormat::BC1a:
		return imgFormat == Anvil::Format::BC1_RGBA_SRGB_BLOCK ||
			imgFormat == Anvil::Format::BC1_RGBA_UNORM_BLOCK ||
			imgFormat == Anvil::Format::BC1_RGB_SRGB_BLOCK ||
			imgFormat == Anvil::Format::BC1_RGB_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::DXT3:
	case uimg::TextureInfo::OutputFormat::BC2:
		return imgFormat == Anvil::Format::BC2_SRGB_BLOCK ||
			imgFormat == Anvil::Format::BC2_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::DXT5:
	case uimg::TextureInfo::OutputFormat::DXT5n:
	case uimg::TextureInfo::OutputFormat::BC3:
	case uimg::TextureInfo::OutputFormat::BC3n:
	case uimg::TextureInfo::OutputFormat::BC3_RGBM:
		return imgFormat == Anvil::Format::BC3_SRGB_BLOCK ||
			imgFormat == Anvil::Format::BC3_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::BC4:
		return imgFormat == Anvil::Format::BC4_SNORM_BLOCK ||
			imgFormat == Anvil::Format::BC4_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::BC5:
		return imgFormat == Anvil::Format::BC5_SNORM_BLOCK ||
			imgFormat == Anvil::Format::BC5_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::BC6:
		return imgFormat == Anvil::Format::BC6H_SFLOAT_BLOCK ||
			imgFormat == Anvil::Format::BC6H_UFLOAT_BLOCK;
	case uimg::TextureInfo::OutputFormat::BC7:
		return imgFormat == Anvil::Format::BC7_SRGB_BLOCK ||
			imgFormat == Anvil::Format::BC7_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::ETC2_R:
		return imgFormat == Anvil::Format::ETC2_R8G8B8A1_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A1_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::ETC2_RG:
		return imgFormat == Anvil::Format::ETC2_R8G8B8A1_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A1_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::ETC2_RGB:
		return imgFormat == Anvil::Format::ETC2_R8G8B8A1_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A1_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::ETC2_RGBA:
		return imgFormat == Anvil::Format::ETC2_R8G8B8A1_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A1_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::ETC2_RGB_A1:
		return imgFormat == Anvil::Format::ETC2_R8G8B8A1_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A1_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::ETC2_RGBM:
		return imgFormat == Anvil::Format::ETC2_R8G8B8A1_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A1_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8A8_UNORM_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_SRGB_BLOCK ||
			imgFormat == Anvil::Format::ETC2_R8G8B8_UNORM_BLOCK;
	case uimg::TextureInfo::OutputFormat::RGB:
	case uimg::TextureInfo::OutputFormat::RGBA:
		return prosper::util::is_uncompressed_format(imgFormat);
	case uimg::TextureInfo::OutputFormat::CTX1:
	case uimg::TextureInfo::OutputFormat::ETC1:
	default:
		return false;
	}
	return false;
}
bool prosper::util::save_texture(const std::string &fileName,prosper::Image &image,const uimg::TextureInfo &texInfo,const std::function<void(const std::string&)> &errorHandler)
{
	std::shared_ptr<prosper::Image> imgRead = image.shared_from_this();
	auto srcFormat = image.GetFormat();
	auto dstFormat = srcFormat;
	if(texInfo.inputFormat != uimg::TextureInfo::InputFormat::KeepInputImageFormat)
		dstFormat = get_anvil_format(texInfo.inputFormat);
	if(texInfo.outputFormat == uimg::TextureInfo::OutputFormat::KeepInputImageFormat || is_compressed_format(imgRead->GetFormat()))
	{
		std::optional<uimg::TextureInfo> convTexInfo = {};
		if(is_compatible(imgRead->GetFormat(),texInfo.outputFormat) == false)
		{
			// Convert the image into the target format
			auto &context = image.GetContext();
			auto &setupCmd = context.GetSetupCommandBuffer();

			// Note: We should just be able to convert the image to
			// the destination format directly, but for some reason this code
			// causes issues when dealing with compressed formats. So instead,
			// we convert it to a generic color format, and then redirect the image compression
			// to the util_image library instead of gli.
			// TODO: It would be much more efficient to use gli for this, too, find out why this isn't working?
			prosper::util::ImageCreateInfo copyCreateInfo {};
			image.GetCreateInfo(copyCreateInfo);
			//copyCreateInfo.format = Anvil::Format::BC1_RGBA_UNORM_BLOCK;//get_anvil_format(texInfo.outputFormat);
			copyCreateInfo.format = Anvil::Format::R32G32B32A32_SFLOAT;
			copyCreateInfo.memoryFeatures = prosper::util::MemoryFeatureFlags::DeviceLocal;
			copyCreateInfo.postCreateLayout = Anvil::ImageLayout::TRANSFER_DST_OPTIMAL;
			copyCreateInfo.tiling = Anvil::ImageTiling::OPTIMAL; // Needs to be in optimal tiling because some GPUs do not support linear tiling with mipmaps
			prosper::util::record_image_barrier(**setupCmd,*image,Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL);
			imgRead = image.Copy(*setupCmd,copyCreateInfo);
			prosper::util::record_image_barrier(**setupCmd,*image,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
			prosper::util::record_image_barrier(**setupCmd,**imgRead,Anvil::ImageLayout::TRANSFER_DST_OPTIMAL,Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
			context.FlushSetupCommandBuffer();

			convTexInfo = texInfo;
			convTexInfo->inputFormat = uimg::TextureInfo::InputFormat::R32G32B32A32_Float;
		}

		// No conversion needed, just copy the image data to a buffer and save it directly
		auto extents = imgRead->GetExtents();
		auto numLayers = imgRead->GetLayerCount();
		auto numLevels = imgRead->GetMipmapCount();
		auto &context = imgRead->GetContext();
		auto &setupCmd = context.GetSetupCommandBuffer();

		gli::extent2d gliExtents {extents.width,extents.height};
		gli::texture2d gliTex {static_cast<gli::texture::format_type>(imgRead->GetFormat()),gliExtents,numLevels};

		prosper::util::BufferCreateInfo bufCreateInfo {};
		bufCreateInfo.memoryFeatures = prosper::util::MemoryFeatureFlags::GPUToCPU;
		bufCreateInfo.size = gliTex.size();
		bufCreateInfo.usageFlags = Anvil::BufferUsageFlagBits::TRANSFER_DST_BIT;
		auto buf = prosper::util::create_buffer(context.GetDevice(),bufCreateInfo);
		buf->SetPermanentlyMapped(true);

		prosper::util::record_image_barrier(**setupCmd,**imgRead,Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL);
		// Initialize buffer with image data
		size_t bufferOffset = 0;
		for(auto iLayer=decltype(numLayers){0u};iLayer<numLayers;++iLayer)
		{
			for(auto iMipmap=decltype(numLevels){0u};iMipmap<numLevels;++iMipmap)
			{
				auto extents = imgRead->GetExtents(iMipmap);
				auto mipmapSize = gliTex.size(iMipmap);
				prosper::util::BufferImageCopyInfo copyInfo {};
				copyInfo.baseArrayLayer = iLayer;
				copyInfo.bufferOffset = bufferOffset;
				copyInfo.dstImageLayout = Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL;
				copyInfo.width = extents.width;
				copyInfo.height = extents.height;
				copyInfo.layerCount = 1;
				copyInfo.mipLevel = iMipmap;
				prosper::util::record_copy_image_to_buffer(**setupCmd,copyInfo,**imgRead,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,*buf);

				bufferOffset += mipmapSize;
			}
		}
		prosper::util::record_image_barrier(**setupCmd,**imgRead,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
		context.FlushSetupCommandBuffer();

		// Copy the data to a gli texture object
		bufferOffset = 0ull;
		for(auto iLayer=decltype(numLayers){0u};iLayer<numLayers;++iLayer)
		{
			for(auto iMipmap=decltype(numLevels){0u};iMipmap<numLevels;++iMipmap)
			{
				auto extents = imgRead->GetExtents(iMipmap);
				auto mipmapSize = gliTex.size(iMipmap);
				auto *dstData = gliTex.data(iLayer,0u /* face */,iMipmap);
				memset(dstData,1,mipmapSize);
				buf->Read(bufferOffset,mipmapSize,dstData);
				bufferOffset += mipmapSize;
			}
		}
		if(convTexInfo.has_value())
		{
			std::vector<std::vector<const void*>> layerMipmapData {};
			layerMipmapData.reserve(numLayers);
			for(auto iLayer=decltype(numLayers){0u};iLayer<numLayers;++iLayer)
			{
				layerMipmapData.push_back({});
				auto &mipmapData = layerMipmapData.back();
				mipmapData.reserve(numLevels);
				for(auto iMipmap=decltype(numLevels){0u};iMipmap<numLevels;++iMipmap)
				{
					auto *dstData = gliTex.data(iLayer,0u /* face */,iMipmap);
					mipmapData.push_back(dstData);
				}
			}
			return uimg::save_texture(fileName,layerMipmapData,extents.width,extents.height,sizeof(float) *4,*convTexInfo,false);
		}
		auto fullFileName = uimg::get_absolute_path(fileName,texInfo.containerFormat);
		switch(texInfo.containerFormat)
		{
		case uimg::TextureInfo::ContainerFormat::DDS:
			return gli::save_dds(gliTex,fullFileName);
		case uimg::TextureInfo::ContainerFormat::KTX:
			return gli::save_ktx(gliTex,fullFileName);
		}
		return false;
	}

	std::shared_ptr<prosper::Buffer> buf = nullptr;
	std::vector<std::vector<size_t>> layerMipmapOffsets {};
	uint32_t sizePerPixel = 0u;
	if(
		image.GetTiling() != Anvil::ImageTiling::LINEAR ||
		(image.GetAnvilImage().get_memory_block()->get_create_info_ptr()->get_memory_features() &Anvil::MemoryFeatureFlagBits::MAPPABLE_BIT) == Anvil::MemoryFeatureFlagBits::NONE ||
		image.GetFormat() != dstFormat
		)
	{
		// Convert the image into the target format
		auto &context = image.GetContext();
		auto &setupCmd = context.GetSetupCommandBuffer();

		prosper::util::ImageCreateInfo copyCreateInfo {};
		image.GetCreateInfo(copyCreateInfo);
		copyCreateInfo.format = dstFormat;
		copyCreateInfo.memoryFeatures = prosper::util::MemoryFeatureFlags::DeviceLocal;
		copyCreateInfo.postCreateLayout = Anvil::ImageLayout::TRANSFER_DST_OPTIMAL;
		copyCreateInfo.tiling = Anvil::ImageTiling::OPTIMAL; // Needs to be in optimal tiling because some GPUs do not support linear tiling with mipmaps
		prosper::util::record_image_barrier(**setupCmd,*image,Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL);
		imgRead = image.Copy(*setupCmd,copyCreateInfo);
		prosper::util::record_image_barrier(**setupCmd,*image,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL);

		// Copy the image data to a buffer
		uint64_t size = 0;
		uint64_t offset = 0;
		auto numLayers = image.GetLayerCount();
		auto numMipmaps = image.GetMipmapCount();
		sizePerPixel = prosper::util::is_compressed_format(dstFormat) ? gli::block_size(static_cast<gli::texture::format_type>(dstFormat)) : prosper::util::get_byte_size(dstFormat);
		layerMipmapOffsets.resize(numLayers);
		for(auto iLayer=decltype(numLayers){0u};iLayer<numLayers;++iLayer)
		{
			auto &mipmapOffsets = layerMipmapOffsets.at(iLayer);
			mipmapOffsets.resize(numMipmaps);
			for(auto iMipmap=decltype(numMipmaps){0u};iMipmap<numMipmaps;++iMipmap)
			{
				mipmapOffsets.at(iMipmap) = size;

				auto extents = image.GetExtents(iMipmap);
				size += extents.width *extents.height *sizePerPixel;
			}
		}
		buf = context.AllocateTemporaryBuffer(size);
		if(buf == nullptr)
		{
			context.FlushSetupCommandBuffer();
			return false; // Buffer allocation failed; Requested size too large?
		}
		prosper::util::record_image_barrier(**setupCmd,**imgRead,Anvil::ImageLayout::TRANSFER_DST_OPTIMAL,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL);

		prosper::util::BufferImageCopyInfo copyInfo {};
		copyInfo.dstImageLayout = Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL;

		struct ImageMipmapData
		{
			uint32_t mipmapIndex = 0u;
			uint64_t bufferOffset = 0ull;
			uint64_t bufferSize = 0ull;
			vk::Extent2D extents = {};
		};
		struct ImageLayerData
		{
			std::vector<ImageMipmapData> mipmaps = {};
			uint32_t layerIndex = 0u;
		};
		std::vector<ImageLayerData> layers = {};
		for(auto iLayer=decltype(numLayers){0u};iLayer<numLayers;++iLayer)
		{
			layers.push_back({});
			auto &layerData = layers.back();
			layerData.layerIndex = iLayer;
			for(auto iMipmap=decltype(numMipmaps){0u};iMipmap<numMipmaps;++iMipmap)
			{
				layerData.mipmaps.push_back({});
				auto &mipmapData = layerData.mipmaps.back();
				mipmapData.mipmapIndex = iMipmap;
				mipmapData.bufferOffset = layerMipmapOffsets.at(iLayer).at(iMipmap);

				auto extents = image.GetExtents(iMipmap);
				mipmapData.extents = extents;
				mipmapData.bufferSize = extents.width *extents.height *sizePerPixel;
			}
		}
		for(auto &layerData : layers)
		{
			for(auto &mipmapData : layerData.mipmaps)
			{
				copyInfo.mipLevel = mipmapData.mipmapIndex;
				copyInfo.baseArrayLayer = layerData.layerIndex;
				copyInfo.bufferOffset = mipmapData.bufferOffset;
				prosper::util::record_copy_image_to_buffer(**setupCmd,copyInfo,**imgRead,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,*buf);
			}
		}
		context.FlushSetupCommandBuffer();

		/*// The dds library expects the image data in RGB form, so we have to do some conversions in some cases.
		std::optional<util::ImageBuffer::Format> imgBufFormat = {};
		switch(srcFormat)
		{
		case Anvil::Format::B8G8R8_UNORM:
		case Anvil::Format::B8G8R8A8_UNORM:
		imgBufFormat = util::ImageBuffer::Format::RGBA32; // TODO: dst format
		break;
		}
		if(imgBufFormat.has_value())
		{
		for(auto &layerData : layers)
		{
		for(auto &mipmapData : layerData.mipmaps)
		{
		std::vector<uint8_t> imgData {};
		imgData.resize(mipmapData.bufferSize);
		if(buf->Read(mipmapData.bufferOffset,mipmapData.bufferSize,imgData.data()))
		{
		// Swap red and blue channels, then write the new image data back to the buffer
		auto imgBuf = util::ImageBuffer::Create(imgData.data(),mipmapData.extents.width,mipmapData.extents.height,*imgBufFormat,true);
		imgBuf->SwapChannels(util::ImageBuffer::Channel::Red,util::ImageBuffer::Channel::Blue);
		buf->Write(mipmapData.bufferOffset,mipmapData.bufferSize,imgData.data());
		}
		}
		}
		}*/
	}
	auto numLayers = imgRead->GetLayerCount();
	auto numMipmaps = imgRead->GetMipmapCount();
	auto cubemap = imgRead->IsCubemap();
	auto extents = imgRead->GetExtents();
	if(buf != nullptr)
	{
		return uimg::save_texture(fileName,[imgRead,buf,&layerMipmapOffsets,sizePerPixel](uint32_t iLayer,uint32_t iMipmap,std::function<void(void)> &outDeleter) -> const uint8_t* {
			void *data;
			auto *memBlock = (*buf)->get_memory_block(0u);
			auto offset = layerMipmapOffsets.at(iLayer).at(iMipmap);
			auto extents = imgRead->GetExtents(iMipmap);
			auto size = extents.width *extents.height *sizePerPixel;
			if(memBlock->map(offset,sizePerPixel,&data) == false)
				return nullptr;
			outDeleter = [memBlock]() {
				memBlock->unmap(); // Note: setMipmapData copies the data, so we don't need to keep it mapped
			};
			return static_cast<uint8_t*>(data);
		},extents.width,extents.height,get_byte_size(dstFormat),numLayers,numMipmaps,cubemap,texInfo,errorHandler);
	}
	return uimg::save_texture(fileName,[imgRead](uint32_t iLayer,uint32_t iMipmap,std::function<void(void)> &outDeleter) -> const uint8_t* {
		auto subresourceLayout = imgRead->GetSubresourceLayout(iLayer,iMipmap);
		if(subresourceLayout.has_value() == false)
			return nullptr;
		void *data;
		auto *memBlock = (*imgRead)->get_memory_block();
		if(memBlock->map(subresourceLayout->offset,subresourceLayout->size,&data) == false)
			return nullptr;
		outDeleter = [memBlock]() {
			memBlock->unmap(); // Note: setMipmapData copies the data, so we don't need to keep it mapped
		};
		return static_cast<uint8_t*>(data);
		},extents.width,extents.height,get_byte_size(dstFormat),numLayers,numMipmaps,cubemap,texInfo,errorHandler);
}
#pragma optimize("",on)
