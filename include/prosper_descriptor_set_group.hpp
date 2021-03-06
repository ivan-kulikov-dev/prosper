/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROSPER_DESCRIPTOR_SET_GROUP_HPP__
#define __PROSPER_DESCRIPTOR_SET_GROUP_HPP__

#include "prosper_definitions.hpp"
#include "prosper_includes.hpp"
#include "prosper_context_object.hpp"
#include "prosper_structs.hpp"

#undef max

#pragma warning(push)
#pragma warning(disable : 4251)
namespace prosper
{
	class Texture;
	class IDescriptorSet;
	class IBuffer;
	class DLLPROSPER DescriptorSetBinding
	{
	public:
		enum class Type : uint8_t
		{
			StorageImage = 0,
			Texture,
			ArrayTexture,
			UniformBuffer,
			DynamicUniformBuffer,
			StorageBuffer
		};
		DescriptorSetBinding(const DescriptorSetBinding&)=delete;
		DescriptorSetBinding &operator=(const DescriptorSetBinding&)=delete;
		virtual ~DescriptorSetBinding()=default;
		uint32_t GetBindingIndex() const;
		IDescriptorSet &GetDescriptorSet() const;
		virtual Type GetType() const=0;
	protected:
		DescriptorSetBinding(IDescriptorSet &descSet,uint32_t bindingIdx);
		IDescriptorSet &m_descriptorSet;
		uint32_t m_bindingIndex = 0u;
	};

	class DLLPROSPER DescriptorSetBindingStorageImage
		: public DescriptorSetBinding
	{
	public:
		DescriptorSetBindingStorageImage(IDescriptorSet &descSet,uint32_t bindingIdx,prosper::Texture &texture,std::optional<uint32_t> layerId={});
		virtual Type GetType() const override {return Type::StorageImage;}
		std::optional<uint32_t> GetLayerIndex() const;
		const std::shared_ptr<prosper::Texture> &GetTexture() const;
	private:
		std::shared_ptr<prosper::Texture> m_texture = nullptr;
		std::optional<uint32_t> m_layerId = {};
	};

	class DLLPROSPER DescriptorSetBindingTexture
		: public DescriptorSetBinding
	{
	public:
		DescriptorSetBindingTexture(IDescriptorSet &descSet,uint32_t bindingIdx,prosper::Texture &texture,std::optional<uint32_t> layerId={});
		virtual Type GetType() const override {return Type::Texture;}
		std::optional<uint32_t> GetLayerIndex() const;
		const std::shared_ptr<prosper::Texture> &GetTexture() const;
	private:
		std::shared_ptr<prosper::Texture> m_texture = nullptr;
		std::optional<uint32_t> m_layerId = {};
	};

	class DLLPROSPER DescriptorSetBindingArrayTexture
		: public DescriptorSetBinding
	{
	public:
		DescriptorSetBindingArrayTexture(IDescriptorSet &descSet,uint32_t bindingIdx);
		virtual Type GetType() const override {return Type::ArrayTexture;}
		void SetArrayBinding(uint32_t arrayIndex,std::unique_ptr<DescriptorSetBindingTexture> bindingTexture);
	private:
		std::vector<std::unique_ptr<DescriptorSetBindingTexture>> m_arrayItems = {};
	};

	class DLLPROSPER DescriptorSetBindingUniformBuffer
		: public DescriptorSetBinding
	{
	public:
		DescriptorSetBindingUniformBuffer(IDescriptorSet &descSet,uint32_t bindingIdx,prosper::IBuffer &buffer,uint64_t startOffset,uint64_t size);
		virtual Type GetType() const override {return Type::UniformBuffer;}
		uint64_t GetStartOffset() const;
		uint64_t GetSize() const;
		const std::shared_ptr<prosper::IBuffer> &GetBuffer() const;
	private:
		std::shared_ptr<prosper::IBuffer> m_buffer = nullptr;
		uint64_t m_startOffset = 0u;
		uint64_t m_size = 0u;
	};

	class DLLPROSPER DescriptorSetBindingDynamicUniformBuffer
		: public DescriptorSetBinding
	{
	public:
		DescriptorSetBindingDynamicUniformBuffer(IDescriptorSet &descSet,uint32_t bindingIdx,prosper::IBuffer &buffer,uint64_t startOffset,uint64_t size);
		virtual Type GetType() const override {return Type::DynamicUniformBuffer;}
		uint64_t GetStartOffset() const;
		uint64_t GetSize() const;
		const std::shared_ptr<prosper::IBuffer> &GetBuffer() const;
	private:
		std::shared_ptr<prosper::IBuffer> m_buffer = nullptr;
		uint64_t m_startOffset = 0u;
		uint64_t m_size = 0u;
	};

	class DLLPROSPER DescriptorSetBindingStorageBuffer
		: public DescriptorSetBinding
	{
	public:
		DescriptorSetBindingStorageBuffer(IDescriptorSet &descSet,uint32_t bindingIdx,prosper::IBuffer &buffer,uint64_t startOffset,uint64_t size);
		virtual Type GetType() const override {return Type::StorageBuffer;}
		uint64_t GetStartOffset() const;
		uint64_t GetSize() const;
		const std::shared_ptr<prosper::IBuffer> &GetBuffer() const;
	private:
		std::shared_ptr<prosper::IBuffer> m_buffer = nullptr;
		uint64_t m_startOffset = 0u;
		uint64_t m_size = 0u;
	};

	class IDescriptorSetGroup;
	class DLLPROSPER IDescriptorSet
	{
	public:
		IDescriptorSet(IDescriptorSetGroup &dsg);
		IDescriptorSet(const IDescriptorSet&)=delete;
		IDescriptorSet &operator=(const IDescriptorSet&)=delete;

		uint32_t GetBindingCount() const;
		DescriptorSetBinding *GetBinding(uint32_t bindingIndex);
		const DescriptorSetBinding *GetBinding(uint32_t bindingIndex) const;
		std::vector<std::unique_ptr<DescriptorSetBinding>> &GetBindings();
		const std::vector<std::unique_ptr<DescriptorSetBinding>> &GetBindings() const;
		DescriptorSetBinding &SetBinding(uint32_t bindingIndex,std::unique_ptr<DescriptorSetBinding> binding);
		virtual bool Update()=0;

		prosper::Texture *GetBoundTexture(uint32_t bindingIndex,std::optional<uint32_t> *optOutLayerIndex=nullptr);
		prosper::IImage *GetBoundImage(uint32_t bindingIndex,std::optional<uint32_t> *optOutLayerIndex=nullptr);
		prosper::IBuffer *GetBoundBuffer(uint32_t bindingIndex,uint64_t *outStartOffset=nullptr,uint64_t *outSize=nullptr);

		virtual bool SetBindingStorageImage(prosper::Texture &texture,uint32_t bindingIdx,uint32_t layerId)=0;
		virtual bool SetBindingStorageImage(prosper::Texture &texture,uint32_t bindingIdx)=0;
		virtual bool SetBindingTexture(prosper::Texture &texture,uint32_t bindingIdx,uint32_t layerId)=0;
		virtual bool SetBindingTexture(prosper::Texture &texture,uint32_t bindingIdx)=0;
		virtual bool SetBindingArrayTexture(prosper::Texture &texture,uint32_t bindingIdx,uint32_t arrayIndex,uint32_t layerId)=0;
		virtual bool SetBindingArrayTexture(prosper::Texture &texture,uint32_t bindingIdx,uint32_t arrayIndex)=0;
		virtual bool SetBindingUniformBuffer(prosper::IBuffer &buffer,uint32_t bindingIdx,uint64_t startOffset=0ull,uint64_t size=std::numeric_limits<uint64_t>::max())=0;
		virtual bool SetBindingDynamicUniformBuffer(prosper::IBuffer &buffer,uint32_t bindingIdx,uint64_t startOffset=0ull,uint64_t size=std::numeric_limits<uint64_t>::max())=0;
		virtual bool SetBindingStorageBuffer(prosper::IBuffer &buffer,uint32_t bindingIdx,uint64_t startOffset=0ull,uint64_t size=std::numeric_limits<uint64_t>::max())=0;

		IDescriptorSetGroup &GetDescriptorSetGroup() const;
	private:
		IDescriptorSetGroup &m_dsg;
		std::vector<std::unique_ptr<DescriptorSetBinding>> m_bindings = {};
	};

	class DLLPROSPER IDescriptorSetGroup
		: public ContextObject,
		public std::enable_shared_from_this<IDescriptorSetGroup>
	{
	public:
		IDescriptorSetGroup(const IDescriptorSetGroup&)=delete;
		IDescriptorSetGroup &operator=(const IDescriptorSetGroup&)=delete;
		virtual ~IDescriptorSetGroup() override;
		IDescriptorSet *GetDescriptorSet(uint32_t index=0);
		const IDescriptorSet *GetDescriptorSet(uint32_t index=0) const;
		uint32_t GetBindingCount() const;

		const DescriptorSetCreateInfo &GetDescriptorSetCreateInfo() const;
	protected:
		IDescriptorSetGroup(IPrContext &context,const DescriptorSetCreateInfo &createInfo);
		std::vector<std::shared_ptr<IDescriptorSet>> m_descriptorSets = {};
		DescriptorSetCreateInfo m_createInfo;
	};
};
#pragma warning(pop)

#endif
