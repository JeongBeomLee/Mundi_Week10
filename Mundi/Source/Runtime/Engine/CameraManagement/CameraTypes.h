#pragma once

#include "pch.h"

// 카메라 블렌드 함수 타입
enum class EViewTargetBlendFunction : uint8
{
	VTBlend_Linear,          // 선형 보간
	VTBlend_Cubic,           // 큐빅 보간: 3차 보간
	VTBlend_EaseIn,          // 천천히 시작
	VTBlend_EaseOut,         // 천천히 종료
	VTBlend_EaseInOut,       // 천천히 시작하고 종료
	VTBlend_BezierCustom,    // 커스텀 베지어 커브
	VTBlend_MAX
};

// 베지어 커브 제어점 구조체
struct FBezierControlPoints
{
	float P0 = 0.0f;         // 시작점 Y값 (X=0.0)
	float P1 = 0.33f;        // 첫 번째 제어점 Y값 (X=0.33)
	float P2 = 0.66f;        // 두 번째 제어점 Y값 (X=0.66)
	float P3 = 1.0f;         // 끝점 Y값 (X=1.0)

	// 기본 생성자 (선형)
	FBezierControlPoints() = default;

	// 커스텀 생성자
	FBezierControlPoints(float InP0, float InP1, float InP2, float InP3)
		: P0(InP0), P1(InP1), P2(InP2), P3(InP3)
	{
	}

	// 큐빅 베지어 평가 함수 (t: 0~1)
	float Evaluate(float T) const;

	// 프리셋 커브
	static FBezierControlPoints Linear();
	static FBezierControlPoints EaseIn();
	static FBezierControlPoints EaseOut();
	static FBezierControlPoints EaseInOut();
	static FBezierControlPoints Cubic();
};

// 뷰 타겟 블렌드 파라미터
struct FViewTargetTransitionParams
{
	// 블렌드 시간
	float BlendTime = 1.0f;
	float BlendTimeRemaining = 0.0f;

	// 블렌드 함수
	EViewTargetBlendFunction BlendFunction = EViewTargetBlendFunction::VTBlend_Cubic;
	float BlendExp = 2.0f;  // Ease 함수의 지수 (기본 2.0)

	// 시작 ViewTarget 고정 여부
	bool bLockOutgoing = false;

	// 커스텀 베지어 커브 (VTBlend_BezierCustom 사용 시)
	FBezierControlPoints LocationCurve;   // 위치 블렌딩 커브
	FBezierControlPoints RotationCurve;   // 회전 블렌딩 커브
	FBezierControlPoints FOVCurve;        // FOV 블렌딩 커브

	// SpringArm 길이 블렌딩
	bool bBlendSpringArmLength = false;
	float TargetSpringArmLength = 3.0f;
	FBezierControlPoints SpringArmCurve;

	// 기본 생성자
	FViewTargetTransitionParams() = default;

	// 간단한 생성자
	FViewTargetTransitionParams(
		float InBlendTime,
		EViewTargetBlendFunction InBlendFunc = EViewTargetBlendFunction::VTBlend_Cubic,
		float InBlendExp = 2.0f,
		bool bInLockOutgoing = false
	)
		: BlendTime(InBlendTime)
		, BlendTimeRemaining(InBlendTime)
		, BlendFunction(InBlendFunc)
		, BlendExp(InBlendExp)
		, bLockOutgoing(bInLockOutgoing)
	{
		// 기본 커브 설정
		switch (InBlendFunc)
		{
		case EViewTargetBlendFunction::VTBlend_Linear:
			LocationCurve = RotationCurve = FOVCurve = SpringArmCurve = FBezierControlPoints::Linear();
			break;
		case EViewTargetBlendFunction::VTBlend_Cubic:
			LocationCurve = RotationCurve = FOVCurve = SpringArmCurve = FBezierControlPoints::Cubic();
			break;
		case EViewTargetBlendFunction::VTBlend_EaseIn:
			LocationCurve = RotationCurve = FOVCurve = SpringArmCurve = FBezierControlPoints::EaseIn();
			break;
		case EViewTargetBlendFunction::VTBlend_EaseOut:
			LocationCurve = RotationCurve = FOVCurve = SpringArmCurve = FBezierControlPoints::EaseOut();
			break;
		case EViewTargetBlendFunction::VTBlend_EaseInOut:
			LocationCurve = RotationCurve = FOVCurve = SpringArmCurve = FBezierControlPoints::EaseInOut();
			break;
		default:
			LocationCurve = RotationCurve = FOVCurve = SpringArmCurve = FBezierControlPoints::Linear();
			break;
		}
	}

	// 블렌드 알파 계산
	float CalculateBlendAlpha() const;

	// 각 채널별 알파 계산 (커스텀 베지어 사용 시)
	float CalculateLocationAlpha() const;
	float CalculateRotationAlpha() const;
	float CalculateFOVAlpha() const;
	float CalculateSpringArmAlpha() const;

	// 블렌드 진행 중인지 확인
	bool IsBlending() const { return BlendTimeRemaining > 0.0f; }

	// 블렌드 업데이트
	void UpdateBlend(float DeltaTime);

	// 직렬화
	void Serialize(JSON& InOutHandle, bool bIsLoading);
};

// FBezierControlPoints 직렬화 헬퍼 함수
void SerializeBezierControlPoints(JSON& InOutHandle, const char* Key, FBezierControlPoints& Curve, bool bIsLoading);
