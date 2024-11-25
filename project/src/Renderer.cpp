// External includes
#include "SDL.h"
#include "SDL_surface.h"
#include <memory>
#include <omp.h>
#include <algorithm>
#include <limits>
#include <vector>
#include <cmath>

// Project includes
#include "Renderer.h"
#include "Maths.h"
#include "Texture.h"
#include "Utils.h"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
    m_pWindow(pWindow)
{
    // Initialize
    SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

    m_Texture = Texture::LoadFromFile("resources/tuktuk.png");

    // Create Buffers
    m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
    m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
    m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

    m_pDepthBufferPixels = new float[m_Width * m_Height];

    auto& meshRef = m_MeshesWorld.emplace_back();
    Utils::ParseOBJ("resources/tuktuk.obj", meshRef.vertices, meshRef.indices);
    meshRef.primitiveTopology = PrimitiveTopology::TriangleList;
    m_MeshesWorld.push_back(meshRef);

    // Initialize Camera
    m_Camera.Initialize(m_Width, m_Height, 60.f, { .0f, 5.f , -30.f });

    // Set number of threads for OpenMP
    omp_set_num_threads(4); // Adjust as needed
}

Renderer::~Renderer()
{
    delete[] m_pDepthBufferPixels;
    delete m_Texture;
}

void Renderer::Update(Timer* pTimer)
{
    m_Camera.Update(pTimer);
}

void Renderer::Render()
{
    //@START: Reset depth buffer and clear screen
    std::fill(m_pDepthBufferPixels, m_pDepthBufferPixels + (m_Width * m_Height), std::numeric_limits<float>::max());

    SDL_Color clearColor = { 0, 0, 0, 255 };
    Uint32 color = SDL_MapRGB(m_pBackBuffer->format, clearColor.r, clearColor.g, clearColor.b);
    SDL_FillRect(m_pBackBuffer, nullptr, color);

    // Lock BackBuffer
    SDL_LockSurface(m_pBackBuffer);

    // RENDER LOGIC
    for (Mesh& mesh : m_MeshesWorld) {
        VertexTransformationFunction(mesh);

        bool isTriangleList = (mesh.primitiveTopology == PrimitiveTopology::TriangleList);

        // Parallelize over triangles
#pragma omp parallel for
        for (int inx = 0; inx < mesh.indices.size() - 2; inx += (isTriangleList ? 3 : 1)) {
            auto t0 = mesh.indices[inx];
            auto t1 = mesh.indices[inx + 1];
            auto t2 = mesh.indices[inx + 2];

            // Handle Triangle Strip
            if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip) {
                if (t0 == t1 || t1 == t2 || t2 == t0) continue;
                if (inx % 2 != 0) std::swap(t1, t2);
            }

            // Vertex positions
            auto v0 = mesh.vertices_out[t0].position;
            auto v1 = mesh.vertices_out[t1].position;
            auto v2 = mesh.vertices_out[t2].position;

            // Backface culling
            Vector3 edge0 = v1 - v0;
            Vector3 edge1 = v2 - v0;
            Vector3 normal = Vector3::Cross(edge0, edge1);
            if (normal.z <= 0) continue;

            // Transform coordinates to screen space
            v0.x *= m_Width;
            v1.x *= m_Width;
            v2.x *= m_Width;
            v0.y *= m_Height;
            v1.y *= m_Height;
            v2.y *= m_Height;

            // Compute bounding box of the triangle
            int minX = std::max(0, static_cast<int>(std::floor(std::min({ v0.x, v1.x, v2.x }))));
            int maxX = std::min(m_Width, static_cast<int>(std::ceil(std::max({ v0.x, v1.x, v2.x }))));
            int minY = std::max(0, static_cast<int>(std::floor(std::min({ v0.y, v1.y, v2.y }))));
            int maxY = std::min(m_Height, static_cast<int>(std::ceil(std::max({ v0.y, v1.y, v2.y }))));

            // Edge vectors
            auto e0 = v2 - v1;
            auto e1 = v0 - v2;
            auto e2 = v1 - v0;

            Vector2 edge0_2D(e0.x, e0.y);
            Vector2 edge1_2D(e1.x, e1.y);
            Vector2 edge2_2D(e2.x, e2.y);

            float wProduct = v0.w * v1.w * v2.w;

            if (v0.w < 0 || v1.w < 0 || v2.w < 0) continue;

            // Parallelize over rows of pixels (py)
#pragma omp parallel for
            for (int py = minY; py < maxY; ++py) {
                for (int px = minX; px < maxX; ++px) {
                    auto P = Vector2(px + 0.5f, py + 0.5f);

                    auto p0 = P - Vector2(v1.x, v1.y);
                    auto p1 = P - Vector2(v2.x, v2.y);
                    auto p2 = P - Vector2(v0.x, v0.y);

                    auto weightP0 = Vector2::Cross(edge0_2D, p0);
                    auto weightP1 = Vector2::Cross(edge1_2D, p1);
                    auto weightP2 = Vector2::Cross(edge2_2D, p2);


                    if (weightP0 < 0 || weightP1 < 0 || weightP2 < 0) continue;

                    auto totalArea = weightP0 + weightP1 + weightP2;
                    float reciprocalTotalArea = 1.0f / totalArea;

                    float interpolationScale0 = weightP0 * reciprocalTotalArea;
                    float interpolationScale1 = weightP1 * reciprocalTotalArea;
                    float interpolationScale2 = weightP2 * reciprocalTotalArea;

                    float zBufferValue = v0.z * v1.z * v2.z / (v1.z * v2.z * interpolationScale0 +
                        v0.z * v2.z * interpolationScale1 +
                        v0.z * v1.z * interpolationScale2);

                    int pixelIndex = px + (py * m_Width);
                    if (zBufferValue >= m_pDepthBufferPixels[pixelIndex]) continue;

                    m_pDepthBufferPixels[pixelIndex] = zBufferValue;

                    float interpolatedDepth = wProduct / (v1.w * v2.w * interpolationScale0 +
                        v0.w * v2.w * interpolationScale1 +
                        v0.w * v1.w * interpolationScale2);
                    if (interpolatedDepth <= 0) continue;

                    // Texture sampling
                    Vector2 uv = (mesh.vertices[t0].uv * v1.w * v2.w * interpolationScale0 +
                        mesh.vertices[t1].uv * v0.w * v2.w * interpolationScale1 +
                        mesh.vertices[t2].uv * v0.w * v1.w * interpolationScale2) *
                        interpolatedDepth / wProduct;
                   // Vector2 uv = (mesh.vertices[t0].uv * v1.w * v2.w * interpolationScale0 +
                   //     mesh.vertices[t1].uv * v0.w * v2.w * interpolationScale1 +
                   //     mesh.vertices[t2].uv * v0.w * v1.w * interpolationScale2) *
                   //     (1.0f / wProduct);

                    ColorRGB finalColor = m_IsFinalColor ? m_Texture->Sample(uv) : ColorRGB(zBufferValue, zBufferValue, zBufferValue);
                    finalColor.MaxToOne();

                    m_pBackBufferPixels[pixelIndex] = SDL_MapRGB(m_pBackBuffer->format,
                        static_cast<uint8_t>(finalColor.r * 255),
                        static_cast<uint8_t>(finalColor.g * 255),
                        static_cast<uint8_t>(finalColor.b * 255));
                }
            }
        }
    }

    //@END
    // Update SDL Surface
    SDL_UnlockSurface(m_pBackBuffer);
    SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
    SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(Mesh& mesh) const
{
    // Precompute transformation matrix
    auto overallMatrix = mesh.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix;

    // Resize vertices_out to match input vertices
    mesh.vertices_out.resize(mesh.vertices.size());

    // Transform vertices in parallel
#pragma omp parallel for
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        Vector4 viewSpacePosition = overallMatrix.TransformPoint(mesh.vertices[i].position.ToVector4());

        Vector4 projectionSpacePosition = viewSpacePosition / viewSpacePosition.w;
       

        projectionSpacePosition.x = projectionSpacePosition.x * 0.5f + 0.5f;
        projectionSpacePosition.y = (1.0f - projectionSpacePosition.y) * 0.5f;

        mesh.vertices_out[i].position = projectionSpacePosition;
        mesh.vertices_out[i].color = mesh.vertices[i].color;
    }
}

bool Renderer::SaveBufferToImage() const
{
    return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
