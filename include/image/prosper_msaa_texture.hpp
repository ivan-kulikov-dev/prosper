/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROSPER_MSAA_TEXTURE_HPP__
#define __PROSPER_MSAA_TEXTURE_HPP__

#include "image/prosper_texture.hpp"

namespace prosper
{
	class DLLPROSPER MSAATexture
		: public Texture
	{
	public:
		const std::shared_ptr<Texture> &GetResolvedTexture() const;

		virtual bool IsMSAATexture() const override;
		std::shared_ptr<Texture> Resolve(
			Anvil::CommandBufferBase &cmdBuffer,Anvil::ImageLayout msaaLayoutIn,Anvil::ImageLayout msaaLayoutOut,Anvil::ImageLayout resolvedLayoutIn,Anvil::ImageLayout resolvedLayoutOut
		);

		// Resets resolved flag
		void Reset();
		virtual void SetDebugName(const std::string &name) override;
	protected:
		friend std::shared_ptr<Texture> util::create_texture(
			Anvil::BaseDevice &dev,const util::TextureCreateInfo &createInfo,const std::shared_ptr<Image> &img,
			const util::ImageViewCreateInfo *imageViewCreateInfo,const util::SamplerCreateInfo *samplerCreateInfo
		);

		MSAATexture(
			Context &context,const std::shared_ptr<Image> &img,const std::vector<std::shared_ptr<ImageView>> &imgViews,const std::shared_ptr<Sampler> &sampler,
			const std::shared_ptr<Texture> &resolvedTexture
		);
		std::shared_ptr<Texture> m_resolvedTexture = nullptr;
		bool m_bResolved = false;
	};
};

#endif