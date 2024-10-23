//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Maths.h"
#include "Texture.h"
#include "Utils.h"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	//m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	//delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	std::vector<Vertex> vertices_nds
	{
		Vertex{{0.f, .5f, 1.f}},
		Vertex{{.5f, -.5f, 1.f}},
		Vertex{{-.5f, -.5f, 1.f}},
	};

	std::vector<Vertex> vertices_screen;
	vertices_screen.resize(vertices_nds.size());
	VertexTransformationFunction(vertices_nds, vertices_screen);
	
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//RENDER LOGIC
	for (int inx = 0; inx < vertices_screen.size(); inx+=3)
	{
		for (int px{}; px < m_Width; ++px)
		{
			for (int py{}; py < m_Height; ++py)
			{
				ColorRGB finalColor{ 1, 1, 1};
				
				auto v0 = vertices_screen[inx].position;
				auto v1 = vertices_screen[inx+1].position;
				auto v2 = vertices_screen[inx+2].position;

				auto P = Vector2(px + 0.5f, py + 0.5f);
				
				auto e1 = v1 - v0;
				auto p1 = P - Vector2(v0.x, v0.y);
				
				auto e2 = v2 - v1;
				auto p2 = P - Vector2(v1.x, v1.y);
				
				auto e3 = v0 - v2;
				auto p3 = P - Vector2(v2.x, v2.y);
				
				if (Vector2::Cross(Vector2(e1.x, e1.y), p1) < 0.f ||
					Vector2::Cross(Vector2(e2.x, e2.y), p2) < 0.f ||
					Vector2::Cross(Vector2(e3.x, e3.y), p3) < 0.f)
				{
					continue;
				}
					
				
				//float gradient = px / static_cast<float>(m_Width);
				//gradient += py / static_cast<float>(m_Width);
				//gradient /= 2.0f;

				//ColorRGB finalColor{ gradient, gradient, gradient };

				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	for (int inx = 0; inx < vertices_in.size(); ++inx)
	{
		vertices_out[inx].position.x = (vertices_in[inx].position.x + 1) / 2.f * m_Width;
		vertices_out[inx].position.y = (1 - vertices_in[inx].position.y) / 2.f * m_Height;
		vertices_out[inx].position.z = vertices_in[inx].position.z;
		vertices_out[inx].color = vertices_in[inx].color;
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
