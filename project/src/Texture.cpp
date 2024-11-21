#include "Texture.h"

#include <iostream>
#include <ostream>

#include "Vector2.h"
#include <SDL_image.h>

namespace dae
{
	Texture::Texture(SDL_Surface* pSurface) :
		m_pSurface{ pSurface },
		m_pSurfacePixels{ (uint32_t*)pSurface->pixels }
	{
	}

	Texture::~Texture()
	{
		if (m_pSurface)
		{
			SDL_FreeSurface(m_pSurface);
			m_pSurface = nullptr;
		}
	}


	Texture* Texture::LoadFromFile(const std::string& path)
	{
		//TODO
		//Load SDL_Surface using IMG_LOAD
		//Create & Return a new Texture Object (using SDL_Surface)
		SDL_Surface* imgSurface = IMG_Load(path.c_str());
		if (!imgSurface)
		{
			std::cerr << "IMG_Load error: " << path << ": " << SDL_GetError() << std::endl;
			return nullptr;
		}
		
		return new Texture(imgSurface);
	}

	ColorRGB Texture::Sample(const Vector2& uv) const
	{
		//TODO
		//Sample the correct texel for the given uv
		float u = uv.x;
		float v = uv.y;
		
		int x = static_cast<int>(u * m_pSurface->w);
		int y = static_cast<int>(v * m_pSurface->h);

		x = std::clamp(x, 0, m_pSurface->w - 1);
		y = std::clamp(y, 0, m_pSurface->h - 1);

		int pixelIndex = y * m_pSurface->w + x;
		
		uint32_t pixel = m_pSurfacePixels[pixelIndex];
	
		Uint8 r, g, b;
		SDL_GetRGB(pixel, m_pSurface->format, &r, &g, &b);
		
		
		return {float(r) / 255.f, float(g) / 255.f, float(b) / 255.f};
	}
}