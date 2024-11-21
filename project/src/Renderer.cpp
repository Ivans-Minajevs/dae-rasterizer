//External includes
#include "SDL.h"
#include "SDL_surface.h"
#include <memory>
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
	m_Camera.Initialize( m_Width, m_Height, 60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render() const
{
	std::unique_ptr<Texture> texture{ Texture::LoadFromFile("resources/uv_grid_2.png") };
	//@START
	for (int i = 0; i < m_Width * m_Height; ++i) {
		m_pDepthBufferPixels[i] = std::numeric_limits<float>::max();
	}
	
	SDL_Color clearColor = {0, 0, 0, 255};
	Uint32 color = SDL_MapRGB(m_pBackBuffer->format, clearColor.r, clearColor.g, clearColor.b);
	SDL_FillRect(m_pBackBuffer, nullptr, color);

	
	std::vector<Mesh> meshes_world = {
		Mesh {
			{
				Vertex{ { -3, 3, -2 }, {}, {0.f, 0.f}},
				Vertex{	{ 0, 3, -2 }, {}, {.5f, 0.f}},
				Vertex{ { 3, 3, -2 }, {}, {1.f, 0.f}},
				Vertex{ { -3, 0, -2 }, {}, {0.f, .5f}},
				Vertex{ { 0, 0, -2 }, {}, {.5f, .5f}},
				Vertex{ { 3, 0, -2 }, {}, {1.f, .5f}},
				Vertex{ { -3, -3, -2 }, {}, {0.f, 1.f}},
				Vertex{ { 0, -3, -2 }, {}, {.5f, 1.f}},
				Vertex{ { 3, -3, -2 }, {}, {1.f, 1.f}}
			},
			{ 3, 0, 4, 1, 5, 2,
				2, 6,
				6, 3, 7, 4, 8, 5 },
			PrimitiveTopology::TriangleStrip
		}
	};

	//std::vector<Mesh> meshes_world = {
	//	Mesh {
	//			{
	//				Vertex{ { -3, 3, -2 }},
	//				Vertex{	{ 0, 3, -2 }},
	//				Vertex{ { 3, 3, -2 }},
	//				Vertex{ { -3, 0, -2 }},
	//				Vertex{ { 0, 0, -2 }},
	//				Vertex{ { 3, 0, -2 }},
	//				Vertex{ { -3, -3, -2 }},
	//				Vertex{ { 0, -3, -2 }},
	//				Vertex{ { 3, -3, -2 }}
	//			},
	//			{
	//					3, 0, 1,   1, 4, 3,   4, 1, 2,
	//					2, 5, 4,   6, 3, 4,   4, 7, 6,
	//					7, 4, 5,   5, 8, 7},
	//			PrimitiveTopology::TriangleList
	//		}
	//};
	
	
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//RENDER LOGIC
	for (Mesh& mesh: meshes_world)
	{
		VertexTransformationFunction(mesh);
		
		bool isTriangleList = (mesh.primitiveTopology == PrimitiveTopology::TriangleList);
		for (int inx = 0; inx < mesh.indices.size() - 2; inx += (isTriangleList ? 3 : 1))
		{

			auto t0 = mesh.indices[inx];
			auto t1 = mesh.indices[inx+1];
			auto t2 = mesh.indices[inx+2];
			if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip)
			{
				if (t0 == t1 || t1 == t2 || t2 == t0)
				{
					continue;
				}
				if (inx % 2 != 0)
				{
					std::swap(t1, t2);
				}
			}
			
			auto v0 = mesh.vertices_out[t0].position;
			auto v1 = mesh.vertices_out[t1].position;
			auto v2 = mesh.vertices_out[t2].position;

			if ((v0.x >= -1 && v0.x <= 1) || (v1.x >= -1 && v1.x <= 1) || (v2.x >= -1 && v2.x <= 1)
				&& (	(v0.y >= -1 && v0.y <= 1) || (v1.y >= -1 && v1.y <= 1) || (v2.y >= -1 && v2.y <= 1)))
			{
				v0.x *= m_Width;
				v1.x *= m_Width;
				v2.x *= m_Width;

				v0.y *= m_Height;
				v1.y *= m_Height;
				v2.y *= m_Height;
			}
			else
			{
				continue;
			}
			
			int minX = std::max(0, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}))));
			int maxX = std::min(m_Width, static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x}))));
			int minY = std::max(0, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}))));
			int maxY = std::min(m_Height, static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y}))));
			
			auto e0 = v2 - v1;
			auto e1 = v0 - v2;
			auto e2 = v1 - v0;
			
			for (int px = minX; px < maxX; ++px)
			{
				for (int py = minY; py < maxY; ++py)
				{
					ColorRGB finalColor{ 0, 0, 0};
					
					auto P = Vector2(px + 0.5f, py + 0.5f);
					
					auto p0 = P - Vector2(v1.x, v1.y);
					auto p1 = P - Vector2(v2.x, v2.y);
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
						float zBufferValue = 1.f / (1.f / v0.z * interpolationScale0 +
													   1.f / v1.z * interpolationScale1 +
													   1.f / v2.z * interpolationScale2);
						// Depth test
						int pixelIndex = px + (py * m_Width);
						if (zBufferValue < m_pDepthBufferPixels[pixelIndex])
						{
							m_pDepthBufferPixels[pixelIndex] = zBufferValue;
							float interpolatedDepth = 1.f / (1.f / v0.w * interpolationScale0 +
													   1.f / v1.w * interpolationScale1 +
													   1.f / v2.w * interpolationScale2);
							if (interpolatedDepth <= 0) {
								continue;
							}

							float u =	 (mesh.vertices[t0].uv.x / v0.w * interpolationScale0 +
										 mesh.vertices[t1].uv.x / v1.w * interpolationScale1 +
										 mesh.vertices[t2].uv.x / v2.w * interpolationScale2) * interpolatedDepth;

							float v =    (mesh.vertices[t0].uv.y / v0.w * interpolationScale0  +
										 mesh.vertices[t1].uv.y / v1.w * interpolationScale1 +
										 mesh.vertices[t2].uv.y / v2.w * interpolationScale2) * interpolatedDepth;
							
							//finalColor = mesh.vertices_out[mesh.indices[inx]].color * interpolationScale0  +
							//			 mesh.vertices_out[mesh.indices[inx+1]].color * interpolationScale1 +
							//			 mesh.vertices_out[mesh.indices[inx+2]].color * interpolationScale2;

							Vector2 uv = Vector2(u, v);

							if (m_IsFinalColor)
							{
								finalColor = texture.get()->Sample(uv);
								finalColor.MaxToOne();
							}
							else
							{
								
								finalColor = ColorRGB(interpolatedDepth, interpolatedDepth, interpolatedDepth);
								finalColor.MaxToOne();
							}
							
							if (m_IsFinalColor)
							{
								m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
									static_cast<uint8_t>(finalColor.r * 255),
									static_cast<uint8_t>(finalColor.g * 255),
									static_cast<uint8_t>(finalColor.b * 255));
							} 
							else
							{
								m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
									static_cast<uint8_t>(finalColor.r * 255),
									static_cast<uint8_t>(0.985f * 255),
									static_cast<uint8_t>(1.f * 255));
							}
							
							
							//if (m_IsFinalColor)
							//{
							//	
							//}
							//else
							//{
							//	m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							//	static_cast<uint8_t>(interpolatedDepth * 255),
							//	static_cast<uint8_t>(0.985f * 255),
							//	static_cast<uint8_t>(1.f * 255));
							//}
							//
						}
					}
				}
			}
		}
	}
	

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(Mesh& mesh) const
{
	// Ensure vertices_out is resized to match the input vertices
	mesh.vertices_out.resize(mesh.vertices.size());

	// Transform each vertex from world space to screen space
	for (size_t i = 0; i < mesh.vertices.size(); ++i)
	{

		auto transformMatrix = mesh.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix;
		//Matric worldViewProjection = mesh.worldMatrix * m_Camera.viewMatrix * 
		// Transform vertex position using the world and view matrices
		//Vector4 viewSpacePosition = m_Camera.projectionMatrix.TransformPoint((m_Camera.viewMatrix.TransformPoint(mesh.worldMatrix.TransformPoint(mesh.vertices[i].position)).ToVector4()));
		Vector4 viewSpacePosition = transformMatrix.TransformPoint(mesh.vertices[i].position.ToVector4());

		if (viewSpacePosition.w <= 0) continue;
		
		// Project the position onto the screen
		Vector4 projectionSpacePosition;
		projectionSpacePosition.x = viewSpacePosition.x / viewSpacePosition.w;

		projectionSpacePosition.y = viewSpacePosition.y / viewSpacePosition.w;
	
		projectionSpacePosition.z = viewSpacePosition.z / viewSpacePosition.w;

		projectionSpacePosition.w = viewSpacePosition.w;
		

		// Normalize to screen coordinates
		projectionSpacePosition.x = projectionSpacePosition.x  * 0.5f + 0.5f;
		projectionSpacePosition.y = (1.0f - projectionSpacePosition.y) * 0.5f;

		// Store the transformed position in vertices_out
		mesh.vertices_out[i].position.x = projectionSpacePosition.x;
		mesh.vertices_out[i].position.y = projectionSpacePosition.y;
		mesh.vertices_out[i].position.z = projectionSpacePosition.z;
		mesh.vertices_out[i].position.w = projectionSpacePosition.w;
		mesh.vertices_out[i].color = mesh.vertices[i].color;
	}
}


bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
