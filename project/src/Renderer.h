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

		inline float Remap(float value, float start1, float stop1, float start2, float stop2)
		{
			return start2 + (value - start1) * (stop2 - start2) / (stop1 - start1);
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

		void SetIsNormalMap(bool isNormalMap)
		{
			m_IsNormalMap = isNormalMap;
		}

		bool GetIsNormalMap() const
		{
			return m_IsNormalMap;
		}

		enum class DisplayMode {
			FinalColor,
			DepthBuffer,
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



		static ColorRGB Lambert(const ColorRGB cd, const float kd = 1)
		{
			const ColorRGB rho = kd * cd;
			return rho / PI;
		}

		static ColorRGB Lambert(const ColorRGB cd, const ColorRGB& kd)
		{
			const ColorRGB rho = kd * cd;
			return rho / PI;
		}

	
		static ColorRGB Phong(const ColorRGB ks, const float exp, const Vector3& l, const Vector3& v, const Vector3& n)
		{
			const Vector3 reflect = l - (2 * std::max(Vector3::Dot(n, l), 0.f) * n);
			const float cosAlpha = std::max(Vector3::Dot(reflect, v), 0.f);

			return ks * std::powf(cosAlpha, exp);
		}
		
		
	private:

		ShadingMode m_CurrentShadingMode{ ShadingMode::Combined };
		DisplayMode m_CurrentDisplayMode{ DisplayMode::FinalColor };

		SDL_Window* m_pWindow{};
		bool m_IsFinalColor { true };
		bool m_IsRotating{ true };
		bool m_IsNormalMap{ false };

		Texture* m_DiffuseTexture;
		Texture* m_NormalMapTexture;
		Texture* m_GlossTexture;
		Texture* m_SpecularTexture;

		std::vector<Mesh> m_MeshesWorld;
		Matrix m_MatrixRot;
		

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		int m_Width{};
		int m_Height{};
		float m_YawAngle{};
	};
}
