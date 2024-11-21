#pragma once
#include <cassert>
#include <iostream>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Maths.h"
#include "Timer.h"

namespace dae
{
	struct Camera
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle, float _width, float _height):
			width{ _width },
			height{ _height },
			origin{ _origin },
			fovAngle{ _fovAngle }
		{
		}

		float width;
		float height;
		Vector3 origin{};
		float fovAngle{90.f};
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };

		Vector3 forward{Vector3::UnitZ};
		Vector3 up{Vector3::UnitY};
		Vector3 right{Vector3::UnitX};

		float totalPitch{};
		float totalYaw{};

		Matrix invViewMatrix{};
		Matrix viewMatrix{};
		Matrix projectionMatrix{};

		void Initialize(float _width, float _height, float _fovAngle = 90.f, Vector3 _origin = {0.f,0.f,0.f})
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);

			width = _width;
			height = _height;
			origin = _origin;
		}

		void CalculateViewMatrix()
		{
			auto right = Vector3::Cross(Vector3::UnitY, forward).Normalized();
			auto up = Vector3::Cross(forward, right).Normalized();

			viewMatrix = Matrix::CreateLookAtLH(origin, forward, up);
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
		}

		void CalculateProjectionMatrix()
		{
			projectionMatrix = Matrix::CreatePerspectiveFovLH(fov, width/height, 1.f, 1000.f);
			
			//ProjectionMatrix => Matrix::CreatePerspectiveFovLH(...) [not implemented yet]
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
		}

		void Update(Timer* pTimer)
		{
			const float deltaTime = pTimer->GetElapsed();

			//Camera Update Logic
			Vector3 velocity { 5.f, 5.f, 5.f };
			constexpr float rotationVeloctiy{ 0.1f * PI / 180.0f};

			//Keyboard Input
			const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);


			//Mouse Input
			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);

			if (pKeyboardState[SDL_SCANCODE_W]) origin += forward * velocity.z * deltaTime;
			if (pKeyboardState[SDL_SCANCODE_S]) origin -= forward * velocity.z * deltaTime;
			if (pKeyboardState[SDL_SCANCODE_A]) origin -= right * velocity.x * deltaTime;
			if (pKeyboardState[SDL_SCANCODE_D]) origin += right * velocity.x * deltaTime;


			bool leftButtonPressed = mouseState & SDL_BUTTON(SDL_BUTTON_LEFT);
			bool rightButtonPressed = mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT);

			if (leftButtonPressed)
			{
				origin += forward * mouseY * deltaTime;
				totalPitch += mouseX * rotationVeloctiy;
			}
			if (rightButtonPressed)
			{
				totalPitch += mouseX * rotationVeloctiy;
				totalYaw += mouseY * rotationVeloctiy;

			}
			Matrix finalRotation = finalRotation.CreateRotation(totalYaw, totalPitch, 0.f);
			forward = finalRotation.TransformVector(Vector3::UnitZ);

		
			forward.Normalize();

			//Update Matrices
			CalculateViewMatrix();
			CalculateProjectionMatrix(); //Try to optimize this - should only be called once or when fov/aspectRatio changes
		}
	};
}
