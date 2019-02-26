/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROSPER_IMAGE_HPP__
#define __PROSPER_IMAGE_HPP__

#include "prosper_definitions.hpp"
#include "prosper_includes.hpp"
#include "prosper_context_object.hpp"
#include <wrappers/image.h>
#include <wrappers/image_view.h>
#include <wrappers/sampler.h>

#undef max

#pragma warning(push)
#pragma warning(disable : 4251)
namespace prosper
{
	class DLLPROSPER Image
		: public ContextObject,
		public std::enable_shared_from_this<Image>
	{
	public:
		static std::shared_ptr<Image> Create(Context &context,std::unique_ptr<Anvil::Image,std::function<void(Anvil::Image*)>> img,const std::function<void(Image&)> &onDestroyedCallback=nullptr);
		virtual ~Image() override;
		Anvil::Image &GetAnvilImage() const;
		Anvil::Image &operator*();
		const Anvil::Image &operator*() const;
		Anvil::Image *operator->();
		const Anvil::Image *operator->() const;

		Anvil::ImageType GetType() const;
		Anvil::ImageUsageFlags GetUsageFlags() const;
		uint32_t GetLayerCount() const;
		Anvil::ImageTiling GetTiling() const;
		Anvil::Format GetFormat() const;
		Anvil::SampleCountFlagBits GetSampleCount() const;
		vk::Extent2D GetExtents(uint32_t mipLevel=0u) const;
		Anvil::ImageCreateFlags GetCreateFlags() const;
		uint32_t GetMipmapCount() const;
		Anvil::SharingMode GetSharingMode() const;
	protected:
		Image(Context &context,std::unique_ptr<Anvil::Image,std::function<void(Anvil::Image*)>> img);
		std::unique_ptr<Anvil::Image,std::function<void(Anvil::Image*)>> m_image = nullptr;
	};
};
#pragma warning(pop)

#endif