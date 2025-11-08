#include "pch.h"
#include "FOffscreenViewportClient.h"
#include "ImGui/imgui.h"
#include "CameraActor.h"
#include "Vector.h"
#include "Math.h"

FOffscreenViewportClient::FOffscreenViewportClient()
	: FViewportClient()
{
}

void FOffscreenViewportClient::ProcessImGuiInput(ImGuiIO& IO, bool bIsRightMouseDown)
{
	if (!Camera) return;

	// 우클릭 드래그 중에만 입력 처리
	if (bIsRightMouseDown)
	{
		// 마우스 델타로 카메라 회전
		if (IO.MouseDelta.x != 0.0f || IO.MouseDelta.y != 0.0f)
		{
			RotateCamera(IO.MouseDelta.x, IO.MouseDelta.y);
		}

		// 키보드 입력으로 카메라 이동 (WASDQE)
		FVector MoveDirection(0, 0, 0);

		if (ImGui::IsKeyDown(ImGuiKey_W)) MoveDirection.X += 1.0f;  // 전진
		if (ImGui::IsKeyDown(ImGuiKey_S)) MoveDirection.X -= 1.0f;  // 후진
		if (ImGui::IsKeyDown(ImGuiKey_D)) MoveDirection.Y += 1.0f;  // 우측
		if (ImGui::IsKeyDown(ImGuiKey_A)) MoveDirection.Y -= 1.0f;  // 좌측
		if (ImGui::IsKeyDown(ImGuiKey_E)) MoveDirection.Z += 1.0f;  // 상승
		if (ImGui::IsKeyDown(ImGuiKey_Q)) MoveDirection.Z -= 1.0f;  // 하강

		// 방향 벡터가 있으면 이동
		if (MoveDirection.SizeSquared() > 0.0f)
		{
			MoveDirection.Normalize();
			MoveCamera(MoveDirection, IO.DeltaTime);
		}
	}
}

void FOffscreenViewportClient::RotateCamera(float DeltaX, float DeltaY)
{
	if (!Camera) return;

	// 마우스 감도
	const float MouseSensitivity = 0.1f;

	// Yaw (좌우 회전) - X축 마우스 이동
	float DeltaYaw = DeltaX * MouseSensitivity;

	// Pitch (상하 회전) - Y축 마우스 이동 (반전 제거)
	float DeltaPitch = DeltaY * MouseSensitivity;

	// 현재 카메라 회전 가져오기
	FQuat CurrentRotation = Camera->GetActorRotation();
	FVector CurrentEuler = CurrentRotation.ToEulerZYXDeg();

	// Pitch 클램핑 (-89 ~ 89도로 제한)
	float NewPitch = FMath::Clamp(CurrentEuler.Y + DeltaPitch, -89.0f, 89.0f);
	float NewYaw = CurrentEuler.Z + DeltaYaw;

	// 새 회전 적용
	FQuat NewRotation = FQuat::MakeFromEulerZYX(FVector(CurrentEuler.X, NewPitch, NewYaw));
	Camera->SetActorRotation(NewRotation);
}

void FOffscreenViewportClient::MoveCamera(const FVector& Direction, float DeltaTime)
{
	if (!Camera) return;

	// 이동 속도
	const float MoveSpeed = 150.0f;

	// 카메라의 현재 회전을 기준으로 방향 변환
	FQuat CameraRotation = Camera->GetActorRotation();
	FVector Forward = CameraRotation.GetForwardVector();
	FVector Right = CameraRotation.GetRightVector();
	FVector Up = FVector(0, 0, 1); // 월드 업 벡터

	// 로컬 방향을 월드 방향으로 변환
	FVector WorldDirection =
		Forward * Direction.X +  // 전후 (W/S)
		Right * Direction.Y +    // 좌우 (A/D)
		Up * Direction.Z;        // 상하 (Q/E)

	// 카메라 이동
	FVector NewLocation = Camera->GetActorLocation() + WorldDirection * MoveSpeed * DeltaTime;
	Camera->SetActorLocation(NewLocation);
}
