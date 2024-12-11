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

    m_DiffuseTexture = Texture::LoadFromFile("resources/vehicle_diffuse.png");
    m_GlossTexture = Texture::LoadFromFile("resources/vehicle_gloss.png");
    m_NormalMapTexture = Texture::LoadFromFile("resources/vehicle_normal.png");
    m_SpecularTexture = Texture::LoadFromFile("resources/vehicle_specular.png");
    //m_Texture = Texture::LoadFromFile("resources/jinx.png");

    // Create Buffers
    m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
    m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
    m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

    m_pDepthBufferPixels = new float[m_Width * m_Height];

    //auto& meshRef = m_MeshesWorld.emplace_back();
    Mesh meshRef{};
    Utils::ParseOBJ("resources/vehicle.obj", meshRef.vertices, meshRef.indices);
  
    meshRef.primitiveTopology = PrimitiveTopology::TriangleList;
    m_MeshesWorld.emplace_back(meshRef);

    // Initialize Camera
    m_Camera.Initialize(m_Width, m_Height, 45.f, { 0.f, 5.f, -64.f });

    // Number of threads for OpenMP
    omp_set_num_threads(omp_get_max_threads());
}

Renderer::~Renderer()
{
    delete[] m_pDepthBufferPixels;
    delete m_DiffuseTexture;
    delete m_GlossTexture;
    delete m_NormalMapTexture;
    delete m_SpecularTexture;
}

void Renderer::Update(Timer* pTimer)
{

    m_Camera.Update(pTimer);


    if (m_IsRotating)
    {
       
        m_MatrixRot *= Matrix::CreateRotationY(pTimer->GetElapsed());
    }
   
}

void Renderer::Render()
{
    // Reset depth buffer and clear screen
    std::fill(m_pDepthBufferPixels, m_pDepthBufferPixels + (m_Width * m_Height), std::numeric_limits<float>::max());

    // Clear screen with black color
    SDL_Color clearColor = { 100, 100, 100, 255 };
    Uint32 color = SDL_MapRGB(m_pBackBuffer->format, clearColor.r, clearColor.g, clearColor.b);
    SDL_FillRect(m_pBackBuffer, nullptr, color);

    // Lock the back buffer before drawing
    SDL_LockSurface(m_pBackBuffer);

    // RENDER LOGIC
    for (Mesh& mesh : m_MeshesWorld) {
        // Apply transformations
        VertexTransformationFunction(mesh);

        bool isTriangleList = (mesh.primitiveTopology == PrimitiveTopology::TriangleList);

        // Parallelize over triangles
#pragma omp parallel for
        for (int inx = 0; inx < mesh.indices.size(); inx += (isTriangleList ? 3 : 1)) {
            
            auto t0 = mesh.indices[inx];
            auto t1 = mesh.indices[inx + 1];
            auto t2 = mesh.indices[inx + 2];

            ////Perform clipping 
            //std::vector<Vertex_Out> clippedVertices;
            //std::vector<uint32_t> clippedIndices;
            //ClipTriangle(mesh.vertices_out[t0], mesh.vertices_out[t1], mesh.vertices_out[t2], clippedVertices, clippedIndices);
            //
            //if (clippedVertices.size() < 3) continue; // If there are not enough vertices left after clipping, skip this triangle

            // Skip degenerate triangles
            if (t0 == t1 || t1 == t2 || t2 == t0) continue;

            // Vertex positions
            auto v0 = mesh.vertices_out[t0].position;
            auto v1 = mesh.vertices_out[t1].position;
            auto v2 = mesh.vertices_out[t2].position;

            // Skip if any vertex is behind the camera (w < 0)
            if (v0.w < 0 || v1.w < 0 || v2.w < 0) continue;
            
            if ((v0.x < -1  || v0.x > 1) || (v1.x < -1 || v1.x > 1) || (v2.x < -1 || v2.x > 1)
                || ((v0.y < -1 || v0.y > 1) || (v1.y < -1 || v1.y > 1) || (v2.y < -1 || v2.y > 1))
                || ((v0.z < 0 || v0.z > 1) || (v1.z < 0 || v1.z > 1) || (v2.z < 0 || v2.z > 1))) continue;


            // Backface culling (skip if the triangle is facing away from the camera)
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

            // Edge vectors for barycentric coordinates
            auto e0 = v2 - v1;
            auto e1 = v0 - v2;
            auto e2 = v1 - v0;

            Vector2 edge0_2D(e0.x, e0.y);
            Vector2 edge1_2D(e1.x, e1.y);
            Vector2 edge2_2D(e2.x, e2.y);

            float wProduct = v0.w * v1.w * v2.w;

            

// Parallelize over rows of pixels (py)
#pragma omp parallel for
            for (int py = minY; py < maxY; ++py) {
                for (int px = minX; px < maxX; ++px) {
                    ColorRGB finalColor;
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

                    // Compute z-buffer value for depth testing
                    float zBufferValue = 1.f / (1.f / v0.z * interpolationScale0 +
                        1.f / v1.z * interpolationScale1 +
                        1.f / v2.z * interpolationScale2);

                    if (zBufferValue < 0 || zBufferValue > 1) continue;

        

                    int pixelIndex = px + (py * m_Width);
                    if (zBufferValue >= m_pDepthBufferPixels[pixelIndex]) continue;
                    //PixelShading(mesh.vertices_out);

                    m_pDepthBufferPixels[pixelIndex] = zBufferValue;

                    // Interpolated depth for final color calculation
                    float interpolatedDepth = wProduct / (v1.w * v2.w * interpolationScale0 +
                        v0.w * v2.w * interpolationScale1 +
                        v0.w * v1.w * interpolationScale2);
                    if (interpolatedDepth <= 0) continue;

                    // Texture sampling
                    Vertex_Out pixelVertex;

                    pixelVertex.position = (mesh.vertices[t0].position.ToPoint4() + mesh.vertices[t1].position.ToPoint4() + mesh.vertices[t2].position.ToPoint4()) / 3.f;
                    pixelVertex.position.z = zBufferValue;
                    pixelVertex.position.w = interpolatedDepth;
                    

                    pixelVertex.uv = Vector2::Interpolate(mesh.vertices_out[t0].uv, mesh.vertices_out[t1].uv, mesh.vertices_out[t2].uv,
                        v0.w, v1.w, v2.w, interpolationScale0, interpolationScale1, interpolationScale2, interpolatedDepth, wProduct);

                    pixelVertex.normal = Vector3::Interpolate(mesh.vertices_out[t0].normal, mesh.vertices_out[t1].normal, mesh.vertices_out[t2].normal,
                        v0.w, v1.w, v2.w, interpolationScale0, interpolationScale1, interpolationScale2, interpolatedDepth, wProduct);
                    pixelVertex.normal.Normalize();


                    pixelVertex.tangent = Vector3::Interpolate(mesh.vertices_out[t0].tangent, mesh.vertices_out[t1].tangent, mesh.vertices_out[t2].tangent,
                        v0.w, v1.w, v2.w, interpolationScale0, interpolationScale1, interpolationScale2, interpolatedDepth, wProduct);
                    pixelVertex.tangent.Normalize();

                    pixelVertex.viewDirection = Vector3::Interpolate(mesh.vertices_out[t0].viewDirection, mesh.vertices_out[t1].viewDirection, mesh.vertices_out[t2].viewDirection,
                        v0.w, v1.w, v2.w, interpolationScale0, interpolationScale1, interpolationScale2, interpolatedDepth, wProduct);
                    pixelVertex.viewDirection.Normalize();

                    pixelVertex.color = colors::Black;

                    // If texture mapping is enabled, sample the texture
                    if (m_CurrentDisplayMode == DisplayMode::FinalColor)
                    {
                        finalColor = m_DiffuseTexture->Sample(pixelVertex.uv);
                    }
                    if (m_CurrentDisplayMode == DisplayMode::DepthBuffer)
                    {
                        auto clampedValue = Remap(zBufferValue, 0.8f, 1.f, 0.f, 1.f);
                        finalColor = ColorRGB(clampedValue, clampedValue, clampedValue);
                    }
                    if (m_CurrentDisplayMode == DisplayMode::ShadingMode)
                    {
                        PixelShading(pixelVertex);
                        finalColor = pixelVertex.color;
                    }
                    
                    finalColor.MaxToOne();

                    m_pBackBufferPixels[pixelIndex] = SDL_MapRGB(m_pBackBuffer->format,
                        static_cast<uint8_t>(finalColor.r * 255.f),
                        static_cast<uint8_t>(finalColor.g * 255.f),
                        static_cast<uint8_t>(finalColor.b * 255.f));
                }
            }
        }
    }
    // Unlock after rendering
    SDL_UnlockSurface(m_pBackBuffer);

    // Copy the back buffer to the front buffer for display
    SDL_BlitSurface(m_pBackBuffer, nullptr, m_pFrontBuffer, nullptr);
    SDL_UpdateWindowSurface(m_pWindow);
}



void Renderer::VertexTransformationFunction(Mesh& mesh) const
{

    // Precompute transformation matrix
  
    auto rotatedWorldMatrix = m_MatrixRot * mesh.worldMatrix;
    auto overallMatrix = rotatedWorldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix;

    // Resize vertices_out to match input vertices
    mesh.vertices_out.resize(mesh.vertices.size());
    
    // Transform vertices in parallel
#pragma omp parallel for
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        mesh.vertices_out[i].normal = rotatedWorldMatrix.TransformVector(mesh.vertices[i].normal).Normalized();

        mesh.vertices_out[i].tangent = rotatedWorldMatrix.TransformVector(mesh.vertices[i].tangent).Normalized();
       
      
        auto rotatedWorldPosition = rotatedWorldMatrix.TransformPoint(mesh.vertices[i].position);
        mesh.vertices_out[i].viewDirection = rotatedWorldPosition - m_Camera.origin;
        mesh.vertices_out[i].viewDirection.Normalize();

        Vector4 viewSpacePosition = overallMatrix.TransformPoint(mesh.vertices[i].position.ToVector4());

        

        Vector4 projectionSpacePosition = viewSpacePosition / viewSpacePosition.w;
       

        projectionSpacePosition.x = projectionSpacePosition.x * 0.5f + 0.5f;
        projectionSpacePosition.y = (1.0f - projectionSpacePosition.y) * 0.5f;

        mesh.vertices_out[i].position = projectionSpacePosition;
        mesh.vertices_out[i].color = mesh.vertices[i].color;
        mesh.vertices_out[i].uv = mesh.vertices[i].uv;
    }
}

void Renderer::PixelShading(Vertex_Out& v)
{
   // ColorRGB tempColor{ ColorRGB(0.f, 0.f, 0.f)};

    Vector3 lightDirection = { .577f, -.577f,  .577f }; 
    constexpr float lightIntensity = 7.f;
    constexpr float shininess = 25.f;
    constexpr ColorRGB ambient = { .025f,.025f,.025f };
   
    if (m_IsNormalMap)
    {
        Vector3 binormal = Vector3::Cross(v.normal, v.tangent);
        Matrix tangentSpaceAxis = Matrix{ v.tangent, binormal, v.normal, Vector3::Zero};

        ColorRGB normalMapSample = m_NormalMapTexture->Sample(v.uv);
        v.normal = (v.tangent * (2.f * normalMapSample.r - 1.f) + binormal * (2.f * normalMapSample.g - 1.f) + v.normal * (2.f * normalMapSample.b - 1.f)).Normalized();
    }
 
    float cosOfAngle{ Vector3::Dot(v.normal, -lightDirection)};

    if (cosOfAngle < 0.f) return;

    ColorRGB observedArea = { cosOfAngle, cosOfAngle, cosOfAngle };
    
    ColorRGB diffuse = Lambert(m_DiffuseTexture->Sample(v.uv));

    ColorRGB gloss = m_GlossTexture->Sample(v.uv);
    float exp = gloss.r * shininess;

    ColorRGB specular = Phong(m_SpecularTexture->Sample(v.uv), exp, -lightDirection, v.viewDirection, v.normal);

    switch (m_CurrentShadingMode)
    {
    case ShadingMode::ObservedArea:
        v.color += observedArea;
        break;

    case ShadingMode::Diffuse:
        v.color += diffuse * observedArea * lightIntensity;
        break;

    case ShadingMode::Specular:
        v.color += specular;
        break;

    case ShadingMode::Combined:
        v.color += ambient + specular + diffuse * observedArea * lightIntensity;
        break;
    }
    
}

void Renderer::ClipTriangle(const Vertex_Out& v0, const Vertex_Out& v1, const Vertex_Out& v2,
    std::vector<Vertex_Out>& clippedVertices, std::vector<uint32_t>& clippedIndices)
{
    // Start with the input triangle vertices
    clippedVertices.clear();
    clippedVertices.push_back(v0);
    clippedVertices.push_back(v1);
    clippedVertices.push_back(v2);

    std::vector<Vector4> planes = {
        Vector4(1, 0, 0, 1),   // Left Plane: x + 0.5 >= 0
        Vector4(-1, 0, 0, 1),  // Right Plane: -x + 0.5 >= 0
        Vector4(0, 1, 0, 1),   // Bottom Plane: y + 0.5 >= 0
        Vector4(0, -1, 0, 1),  // Top Plane: -y + 0.5 >= 0
        Vector4(0, 0, 1, 1),   // Near Plane: z + 0.5 >= 0
        Vector4(0, 0, -1, 1)   // Far Plane: -z + 0.5 >= 0
    };

    // Sequentially clip the triangle against each plane
    for (const auto& plane : planes)
    {
        std::vector<Vertex_Out> outputVertices;
        outputVertices.reserve(clippedVertices.size());

        // Clip the current polygon against the plane
        ClipPolygonAgainstPlane(clippedVertices, outputVertices, plane);

        // If after clipping, there are fewer than 3 vertices, discard the triangle
        if (outputVertices.size() < 3) return;

        clippedVertices = std::move(outputVertices);
    }

    // Generate indices for the clipped triangle
    uint32_t baseIndex = static_cast<uint32_t>(clippedVertices.size()) - 3;
    for (size_t i = 1; i < clippedVertices.size() - 1; ++i)
    {
        clippedIndices.push_back(baseIndex);
        clippedIndices.push_back(baseIndex + static_cast<uint32_t>(i));
        clippedIndices.push_back(baseIndex + static_cast<uint32_t>(i + 1));
    }
}

void Renderer::ClipPolygonAgainstPlane(std::vector<Vertex_Out>& inputVertices,
    std::vector<Vertex_Out>& outputVertices,
    const Vector4& plane)
{
    if (inputVertices.empty()) return;

    size_t vertexCount = inputVertices.size();
    outputVertices.clear();
    outputVertices.reserve(vertexCount);

    // Process each edge of the polygon
    for (size_t i = 0; i < vertexCount; ++i)
    {
        const Vertex_Out& currentVertex = inputVertices[i];
        const Vertex_Out& nextVertex = inputVertices[(i + 1) % vertexCount];

        float currentValue = PlaneValue(currentVertex.position, plane);
        float nextValue = PlaneValue(nextVertex.position, plane);

        // If the current vertex is inside the plane
        if (currentValue >= 0)
        {
            outputVertices.push_back(currentVertex);
        }

        // If the edge crosses the plane, compute intersection
        if ((currentValue >= 0) != (nextValue >= 0))
        {
            Vertex_Out intersection = IntersectEdgeWithPlane(currentVertex, nextVertex, currentValue, nextValue);
            outputVertices.push_back(intersection);
        }
    }
}

Vertex_Out Renderer::IntersectEdgeWithPlane(const Vertex_Out& v0, const Vertex_Out& v1,
    float v0PlaneValue, float v1PlaneValue)
{
    float denominator = v0PlaneValue - v1PlaneValue;

    if (fabs(denominator) < 1e-6) return v0;  // Avoid division by zero

    float t = v0PlaneValue / (v0PlaneValue - v1PlaneValue);

    if (t < 0 || t > 1) return v0;  // Disregard invalid intersections

    Vertex_Out intersection;
    intersection.position = v0.position + (t * (v1.position - v0.position)).ToVector4();

    // Interpolate other attributes (color, UV, normal)
    intersection.color = v0.color + t * (v1.color - v0.color);
    intersection.uv = v0.uv + t * (v1.uv - v0.uv);
    intersection.normal = v0.normal + t * (v1.normal - v0.normal);
    intersection.tangent = v0.tangent + t * (v1.tangent - v0.tangent);

    return intersection;
}




bool Renderer::SaveBufferToImage() const
{
    return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
