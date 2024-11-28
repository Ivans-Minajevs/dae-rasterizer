#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include "Camera.h"
#include "DataTypes.h"

struct SDL_Window;
struct SDL_Surface;

namespace dae
{
	class Texture;
	struct Mesh;
	struct Vertex;
	class Timer;
	class Scene;

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(Timer* pTimer);
		void Render();

		bool SaveBufferToImage() const;

		void VertexTransformationFunction(Mesh& mesh) const;
		void PixelShading(const Vertex_Out& v);

		void ClipTriangle(const Vertex_Out& v0, const Vertex_Out& v1, const Vertex_Out& v2,
			std::vector<Vertex_Out>& clippedVertices, std::vector<uint32_t>& clippedIndices);
		void ClipPolygonAgainstPlane(std::vector<Vertex_Out>& inputVertices,
			std::vector<Vertex_Out>& outputVertices,
			const Vector4& plane);
		Vertex_Out IntersectEdgeWithPlane(const Vertex_Out& v0, const Vertex_Out& v1,
			float v0PlaneValue, float v1PlaneValue);

		float PlaneValue(const Vector4& vertex, const Vector4& plane) {
			return vertex.x * plane.x + vertex.y * plane.y + vertex.z * plane.z + vertex.w * plane.w;
		}


		void PixelShading(Vertex_Out& v);

		void SetIsFinalColor(bool isFinalColor)
		{
			m_IsFinalColor = isFinalColor;
		}

		bool GetIsFinalColor() const
		{
			return m_IsFinalColor;
		}

		void SetIsRotating(bool isRotating)
		{
			m_IsRotating = isRotating;
		}

		bool GetIsRotating() const
		{
			return m_IsRotating;
		}

		enum class DisplayMode {
			FinalColor,
			DepthBuffer,
			NormalMap,
			ShadingMode
		};

		enum class ShadingMode
		{
			ObservedArea,
			Diffuse,
			Specular,
			Combined
		};

		void CycleShadingMode()
		{
			switch (m_CurrentShadingMode)
			{
			case ShadingMode::Combined:
				m_CurrentShadingMode = ShadingMode::ObservedArea;
				break;
			case ShadingMode::ObservedArea:
				m_CurrentShadingMode = ShadingMode::Diffuse;
				break;
			case ShadingMode::Diffuse:
				m_CurrentShadingMode = ShadingMode::Specular;
				break;
			case ShadingMode::Specular:
				m_CurrentShadingMode = ShadingMode::Combined;
				break;
			}
		}

		void SetDisplayMode(DisplayMode displayMode)
		{
			m_CurrentDisplayMode = displayMode;
		}

		DisplayMode GetDisplayMode() const
		{
			return m_CurrentDisplayMode;
		}
		
	private:

		ShadingMode m_CurrentShadingMode{ ShadingMode::Combined };
		DisplayMode m_CurrentDisplayMode{ DisplayMode::FinalColor };

		SDL_Window* m_pWindow{};
		bool m_IsFinalColor { true };
		bool m_IsRotating{ true };

		Texture* m_Texture;
		std::vector<Mesh> m_MeshesWorld;
		Matrix m_MatrixRot;

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		int m_Width{};
		int m_Height{};
	};
}
