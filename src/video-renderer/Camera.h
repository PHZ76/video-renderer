#pragma once
#include <Windows.h>
#include <DirectXMath.h>

namespace nv {
	class Camera {
	public:
		float cameraPosX;
		float cameraPosY;
		float cameraPosZ;
		float cameraRollX;
		float cameraRollY;
		float cameraRollZ;

		float viewWidth;
		float viewHeight;
		float nearZ;
		float farZ;

		Camera();
		DirectX::XMMATRIX GetMatrix();
		
		void MoveForward(float step);
		void MoveBack(float step);
		void MoveLeft(float step);
		void MoveRight(float step);

		void TurnLeftRight(float step);
		void TurnUpDown(float step);

		void Reset();
		void ToBack();
	private:
		void Move(DirectX::XMFLOAT3 translation, float speed);
	};
}