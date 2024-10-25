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

	m_pDepthBufferPixels = new float[m_Width * m_Height];
	

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	for (int i = 0; i < m_Width * m_Height; ++i) {
		m_pDepthBufferPixels[i] = std::numeric_limits<float>::max();
	}
	
	SDL_Color clearColor = {100, 100, 100, 255};
	Uint32 color = SDL_MapRGB(m_pBackBuffer->format, clearColor.r, clearColor.g, clearColor.b);
	SDL_FillRect(m_pBackBuffer, nullptr, color);
	
	std::vector<Vertex> vertices_world
	{
		{{0.f, 2.f, 0.f}, {1, 0, 0}},
		{{1.5f, -1.f, 0.f},{1, 0, 0}},
		{{-1.5f, -1.f, 0.f}, {1, 0, 0}},
		
		{{0.f, 4.f, 2.f}, {1, 0, 0}},
		{{3.f, -2.f, 2.f},{0, 1, 0}},
		{{-3.f, -2.f, 2.f}, {0, 0, 1}}
	};

	std::vector<Vertex> vertices_screen;
	vertices_screen.resize(vertices_world.size());
	VertexTransformationFunction(vertices_world, vertices_screen);
	
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//RENDER LOGIC
	for (int inx = 0; inx < vertices_screen.size(); inx+=3)
	{
		for (int px{}; px < m_Width; ++px)
		{
			for (int py{}; py < m_Height; ++py)
			{
				ColorRGB finalColor{ 0, 0, 0};

				auto v0 = vertices_screen[inx].position;
				auto v1 = vertices_screen[inx+1].position;
				auto v2 = vertices_screen[inx+2].position;
				
				auto P = Vector2(px + 0.5f, py + 0.5f);
				
				auto e0 = v2 - v1;
				auto p0 = P - Vector2(v1.x, v1.y);
				
				auto e1 = v0 - v2;
				auto p1 = P - Vector2(v2.x, v2.y);
				
				auto e2 = v1 - v0;
				auto p2 = P - Vector2(v0.x, v0.y);

				auto weightP0 = Vector2::Cross(Vector2(e0.x, e0.y), p0);
				auto weightP1 = Vector2::Cross(Vector2(e1.x, e1.y), p1);
				auto weightP2 = Vector2::Cross(Vector2(e2.x, e2.y), p2);
				if (weightP0 >= 0 &&
					weightP1 >= 0 &&
					weightP2 >= 0)
				{
					// Calculate total area
					auto totalArea = weightP0 + weightP1 + weightP2;
					
					// Calculate barycentric coordinates
					float interpolationScale0 = weightP0 / totalArea;
					float interpolationScale1 = weightP1 / totalArea;
					float interpolationScale2 = weightP2 / totalArea;

					// Interpolate depth
					float interpolatedDepth = v0.z * interpolationScale0 +
											  v1.z * interpolationScale1 +
											  v2.z * interpolationScale2;
					// Depth test
					int pixelIndex = px + (py * m_Width);
					if (interpolatedDepth < m_pDepthBufferPixels[pixelIndex])
					{
						m_pDepthBufferPixels[pixelIndex] = interpolatedDepth;
						finalColor = vertices_screen[inx].color * interpolationScale0  +
									 vertices_screen[inx+1].color * interpolationScale1 +
									 vertices_screen[inx+2].color * interpolationScale2;
						finalColor.MaxToOne();

						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));
					}
					
				}
					
				
				//float gradient = px / static_cast<float>(m_Width);
				//gradient += py / static_cast<float>(m_Width);
				//gradient /= 2.0f;

				//ColorRGB finalColor{ gradient, gradient, gradient };

				//Update Color in Buffer
				
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
		auto viewSpaceMatrix = m_Camera.viewMatrix.TransformPoint(vertices_in[inx].position);

		Vector3 projectionSpaceMatrix;
		projectionSpaceMatrix.x = viewSpaceMatrix.x / viewSpaceMatrix.z;
		projectionSpaceMatrix.y = viewSpaceMatrix.y / viewSpaceMatrix.z;
		projectionSpaceMatrix.z = viewSpaceMatrix.z;
		
		projectionSpaceMatrix.x = projectionSpaceMatrix.x / ((float(m_Width)/m_Height) * m_Camera.fov);
		projectionSpaceMatrix.y = projectionSpaceMatrix.y / m_Camera.fov;
		
		vertices_out[inx].position.x = (projectionSpaceMatrix.x + 1) / 2.f * m_Width;
		vertices_out[inx].position.y = (1 - projectionSpaceMatrix.y) / 2.f * m_Height;
		vertices_out[inx].position.z = projectionSpaceMatrix.z;
		vertices_out[inx].color = vertices_in[inx].color;
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
