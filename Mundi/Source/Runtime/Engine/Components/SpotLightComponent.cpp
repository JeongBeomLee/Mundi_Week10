#include "pch.h"
#include "SpotLightComponent.h"
#include "ShadowViewProjection.h"
#include "BillboardComponent.h"
#include "Gizmo/GizmoArrowComponent.h"

IMPLEMENT_CLASS(USpotLightComponent)

BEGIN_PROPERTIES(USpotLightComponent)
	MARK_AS_COMPONENT("스포트 라이트", "스포트 라이트 컴포넌트를 추가합니다.")
	ADD_PROPERTY_RANGE(float, InnerConeAngle, "Light", 0.0f, 90.0f, true, "원뿔 내부 각도입니다. 이 각도 안에서는 빛이 최대 밝기로 표시됩니다.")
	ADD_PROPERTY_RANGE(float, OuterConeAngle, "Light", 0.0f, 90.0f, true, "원뿔 외부 각도입니다. 이 각도 밖에서는 빛이 보이지 않습니다.")
END_PROPERTIES()

USpotLightComponent::USpotLightComponent()
{
	InnerConeAngle = 30.0f;
	OuterConeAngle = 45.0f;

	ShadowBias = 0.00001f;
	ShadowSlopeBias = 1.0f;
}

void USpotLightComponent::ValidateConeAngles()
{
	// OuterCone과 InnerCone의 일관성 유지
	// 어느 값이 변경되었는지 추적하여 다른 값을 조정

	// InnerCone이 변경됨
	bool bInnerChanged = (InnerConeAngle != PreviousInnerConeAngle);
	// OuterCone이 변경됨
	bool bOuterChanged = (OuterConeAngle != PreviousOuterConeAngle);

	if (bInnerChanged)
	{
		// InnerCone이 증가하여 OuterCone보다 커진 경우
		if (InnerConeAngle > OuterConeAngle)
		{
			OuterConeAngle = InnerConeAngle;
		}
	}
	else if (bOuterChanged)
	{
		// OuterCone이 감소하여 InnerCone보다 작아진 경우
		if (OuterConeAngle < InnerConeAngle)
		{
			InnerConeAngle = OuterConeAngle;
		}
	}

	// 이전 값 업데이트
	PreviousInnerConeAngle = InnerConeAngle;
	PreviousOuterConeAngle = OuterConeAngle;
}

USpotLightComponent::~USpotLightComponent()
{
}

FVector USpotLightComponent::GetDirection() const
{
	// Z-Up Left-handed 좌표계에서 Forward는 X축
	FQuat Rotation = GetWorldRotation();
	return Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

float USpotLightComponent::GetConeAttenuation(const FVector& WorldPosition) const
{
	FVector LightPosition = GetWorldLocation();
	FVector LightDirection = GetDirection();
	FVector ToPosition = (WorldPosition - LightPosition).GetNormalized();

	// 방향 벡터와의 각도 계산
	float CosAngle = FVector::Dot(LightDirection, ToPosition);
	float Angle = acos(CosAngle) * (180.0f / 3.14159265f); // Radians to Degrees

	// 내부 원뿔 안: 감쇠 없음
	if (Angle <= InnerConeAngle)
	{
		return 1.0f;
	}

	// 외부 원뿔 밖: 빛 없음
	if (Angle >= OuterConeAngle)
	{
		return 0.0f;
	}

	// 내부와 외부 사이: 부드러운 감쇠
	float Range = OuterConeAngle - InnerConeAngle;
	float T = (Angle - InnerConeAngle) / Range;
	return 1.0f - T;
}

FSpotLightInfo USpotLightComponent::GetLightInfo() const
{
	FSpotLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	Info.Position = GetWorldLocation();
	Info.InnerConeAngle = GetInnerConeAngle();
	Info.Direction = GetDirection();
	Info.OuterConeAngle = GetOuterConeAngle();
	Info.AttenuationRadius = GetAttenuationRadius();
	Info.FalloffExponent = GetFalloffExponent(); // Always pass FalloffExponent (used when bUseInverseSquareFalloff = false)
	Info.bUseInverseSquareFalloff = IsUsingInverseSquareFalloff() ? 1u : 0u;
	Info.bCastShadow = GetIsCastShadows() ? 1u : 0u;
	Info.ShadowMapIndex = static_cast<uint32>(GetShadowMapIndex());
	Info.Padding = 0; // 패딩 초기화

	// Calculate Light View-Projection matrix for shadow mapping
	if (Info.bCastShadow && Info.ShadowMapIndex != 0xFFFFFFFF)
	{
		// FShadowViewProjection 헬퍼 사용하여 중복 제거
		FShadowViewProjection ShadowVP = FShadowViewProjection::CreateForSpotLight(
			GetWorldLocation(),
			GetDirection(),
			GetOuterConeAngle() * 2.0f,  // 전체 FOV
			GetAttenuationRadius());

		Info.LightViewProjection = ShadowVP.ViewProjection;
	}
	else
	{
		// Identity matrix if not casting shadows
		Info.LightViewProjection = FMatrix::Identity();
	}

	return Info;
}

void USpotLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 스포트라이트 특화 업데이트 로직

	// Cone 각도 유효성 검사 (UI에서 변경된 경우를 대비)
	ValidateConeAngles();

	GWorld->GetLightManager()->UpdateLight(this);

	// Update direction gizmo to reflect any changes
	UpdateDirectionGizmo();
}

void USpotLightComponent::OnTransformUpdated()
{
	Super::OnTransformUpdated();
	GWorld->GetLightManager()->UpdateLight(this);
}

void USpotLightComponent::OnRegister(UWorld* InWorld)
{
	Super_t::OnRegister(InWorld);
	if (SpriteComponent)
	{
		SpriteComponent->SetTextureName(GDataDir + "/UI/Icons/SpotLight_64x.png");
	}

	// Create Direction Gizmo if not already created
	if (!DirectionGizmo && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);

		// DirectionGizmo->SetCanEverPick(false);

		// Set gizmo mesh (using the same mesh as GizmoActor's arrow)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// Use world-space scale (not screen-constant scale like typical gizmos)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// Set default scale
		DirectionGizmo->SetDefaultScale(FVector(0.5f, 0.3f, 0.3f));

		// Update gizmo properties to match light
		UpdateDirectionGizmo();
	}
	InWorld->GetLightManager()->RegisterLight(this);
}

void USpotLightComponent::OnUnregister()
{
	GWorld->GetLightManager()->DeRegisterLight(this);
}

void USpotLightComponent::UpdateDirectionGizmo()
{
	if (!DirectionGizmo)
		return;

	// Set direction to match light direction (used for hovering/picking, not for rendering)
	FVector LightDir = GetDirection();
	DirectionGizmo->SetDirection(LightDir);

	// Set color to match base light color (without intensity or temperature multipliers)
	const FLinearColor& BaseColor = GetLightColor();
	DirectionGizmo->SetColor(FVector(BaseColor.R, BaseColor.G, BaseColor.B));
}

void USpotLightComponent::RenderDebugVolume(URenderer* Renderer) const
{
	// Cone 각도 유효성 검사 (const 메서드이므로 const_cast 사용)
	const_cast<USpotLightComponent*>(this)->ValidateConeAngles();

	// Unreal Engine 스타일 SpotLight 볼륨 렌더링
	const FVector4 InnerConeColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색 (InnerCone)
	const FVector4 OuterConeColor(1.0f, 0.5f, 0.0f, 1.0f); // 주황색 (OuterCone)

	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 원뿔의 정점(apex)은 라이트의 위치
	const FVector ApexWorld = GetWorldLocation();
	const FVector LightDirection = GetDirection();

	// 볼록한 아크의 가장 먼 지점(LightDirection 방향)까지의 거리가 AttenuationRadius
	const float AttenuationRadius = GetAttenuationRadius();

	// 세그먼트 수
	const int NumRays = 24;      // 방사형 라인 개수
	const int NumArcSegments = 32; // 아크 세그먼트 개수

	// LightDirection에 수직인 두 벡터 구하기
	FVector Up = FVector(0.0f, 0.0f, 1.0f);
	if (std::abs(FVector::Dot(LightDirection, Up)) > 0.99f)
	{
		Up = FVector(0.0f, 1.0f, 0.0f);
	}
	FVector Right = FVector::Cross(LightDirection, Up).GetNormalized();
	FVector ActualUp = FVector::Cross(Right, LightDirection).GetNormalized();

	// --- OuterCone 그리기 ---
	const float OuterAngleRad = DegreesToRadians(OuterConeAngle);

	// 방사형 라인 길이는 AttenuationRadius로 고정
	const float OuterRayLength = AttenuationRadius;

	// 아크의 가장 먼 지점(LightDirection 방향)이 AttenuationRadius가 되도록 ArcRadius 계산
	// ArcRadius × cos(angle) = AttenuationRadius
	// ArcRadius = AttenuationRadius / cos(angle)
	const float OuterArcRadius = (OuterAngleRad < HALF_PI && std::cos(OuterAngleRad) > 0.0001f)
		? AttenuationRadius / std::cos(OuterAngleRad)
		: AttenuationRadius;

	// 밑면 원을 위한 끝점들 저장
	TArray<FVector> OuterEndPoints;
	OuterEndPoints.Reserve(NumRays);

	// 1. 방사형 라인 (Apex에서 원뿔 가장자리로) - 길이 고정
	for (int i = 0; i < NumRays; ++i)
	{
		const float Angle = (static_cast<float>(i) / NumRays) * TWO_PI;
		const FVector RadialDir = (Right * std::cos(Angle) + ActualUp * std::sin(Angle)).GetNormalized();

		// 원뿔 방향으로 회전
		const FVector RayDir = (LightDirection * std::cos(OuterAngleRad) + RadialDir * std::sin(OuterAngleRad)).GetNormalized();
		const FVector RayEnd = ApexWorld + RayDir * OuterRayLength;

		StartPoints.Add(ApexWorld);
		EndPoints.Add(RayEnd);
		Colors.Add(OuterConeColor);

		OuterEndPoints.Add(RayEnd);
	}

	// 밑면 원 그리기 (방사형 라인의 끝점들을 연결)
	for (int i = 0; i < NumRays; ++i)
	{
		StartPoints.Add(OuterEndPoints[i]);
		EndPoints.Add(OuterEndPoints[(i + 1) % NumRays]);
		Colors.Add(OuterConeColor);
	}

	// 2. 언리얼 엔진 스타일 아크 (5+1+5 정점 방식)
	// ActualUp축 기준 아크
	{
		// 가운데 정점 (가장 볼록한 부분) - LightDirection 방향으로 AttenuationRadius 거리
		const FVector CenterPoint = ApexWorld + LightDirection * AttenuationRadius;

		// 양쪽 끝점 (방사형 라인의 끝점과 동일)
		const FVector RayDirLeft = (LightDirection * std::cos(OuterAngleRad) + ActualUp * std::sin(OuterAngleRad)).GetNormalized();
		const FVector RayDirRight = (LightDirection * std::cos(OuterAngleRad) - ActualUp * std::sin(OuterAngleRad)).GetNormalized();
		const FVector LeftEndPoint = ApexWorld + RayDirLeft * OuterRayLength;
		const FVector RightEndPoint = ApexWorld + RayDirRight * OuterRayLength;

		// 11개 정점 생성 (왼쪽 5개 + 중앙 1개 + 오른쪽 5개)
		TArray<FVector> ArcPoints;
		const int NumArcPoints = 11;

		for (int i = 0; i < NumArcPoints; ++i)
		{
			float t = static_cast<float>(i) / (NumArcPoints - 1); // 0.0 ~ 1.0

			FVector Point;
			if (t <= 0.5f)
			{
				// 왼쪽 끝점 -> 중앙 정점 (구면 보간)
				float localT = t * 2.0f; // 0.0 ~ 1.0
				FVector DirLeft = (LeftEndPoint - ApexWorld).GetNormalized();
				FVector DirCenter = (CenterPoint - ApexWorld).GetNormalized();

				// Slerp (구면 선형 보간)
				float cosAngle = FVector::Dot(DirLeft, DirCenter);
				float angle = std::acos(FMath::Clamp(cosAngle, -1.0f, 1.0f));

				if (angle < 0.001f)
				{
					// 각도가 매우 작으면 선형 보간 사용
					Point = FVector::Lerp(LeftEndPoint, CenterPoint, localT);
				}
				else
				{
					float sinAngle = std::sin(angle);
					float ratio1 = std::sin((1.0f - localT) * angle) / sinAngle;
					float ratio2 = std::sin(localT * angle) / sinAngle;
					FVector Dir = (DirLeft * ratio1 + DirCenter * ratio2).GetNormalized();

					// 거리도 보간
					float distance = FMath::Lerp(OuterRayLength, AttenuationRadius, localT);
					Point = ApexWorld + Dir * distance;
				}
			}
			else
			{
				// 중앙 정점 -> 오른쪽 끝점 (구면 보간)
				float localT = (t - 0.5f) * 2.0f; // 0.0 ~ 1.0
				FVector DirCenter = (CenterPoint - ApexWorld).GetNormalized();
				FVector DirRight = (RightEndPoint - ApexWorld).GetNormalized();

				// Slerp (구면 선형 보간)
				float cosAngle = FVector::Dot(DirCenter, DirRight);
				float angle = std::acos(FMath::Clamp(cosAngle, -1.0f, 1.0f));

				if (angle < 0.001f)
				{
					// 각도가 매우 작으면 선형 보간 사용
					Point = FVector::Lerp(CenterPoint, RightEndPoint, localT);
				}
				else
				{
					float sinAngle = std::sin(angle);
					float ratio1 = std::sin((1.0f - localT) * angle) / sinAngle;
					float ratio2 = std::sin(localT * angle) / sinAngle;
					FVector Dir = (DirCenter * ratio1 + DirRight * ratio2).GetNormalized();

					// 거리도 보간
					float distance = FMath::Lerp(AttenuationRadius, OuterRayLength, localT);
					Point = ApexWorld + Dir * distance;
				}
			}

			ArcPoints.Add(Point);
		}

		// 정점들을 선으로 연결
		for (int i = 0; i < NumArcPoints - 1; ++i)
		{
			StartPoints.Add(ArcPoints[i]);
			EndPoints.Add(ArcPoints[i + 1]);
			Colors.Add(OuterConeColor);
		}
	}

	// Right축 기준 아크
	{
		// 가운데 정점 (가장 볼록한 부분) - LightDirection 방향으로 AttenuationRadius 거리
		const FVector CenterPoint = ApexWorld + LightDirection * AttenuationRadius;

		// 양쪽 끝점 (방사형 라인의 끝점과 동일)
		const FVector RayDirLeft = (LightDirection * std::cos(OuterAngleRad) + Right * std::sin(OuterAngleRad)).GetNormalized();
		const FVector RayDirRight = (LightDirection * std::cos(OuterAngleRad) - Right * std::sin(OuterAngleRad)).GetNormalized();
		const FVector LeftEndPoint = ApexWorld + RayDirLeft * OuterRayLength;
		const FVector RightEndPoint = ApexWorld + RayDirRight * OuterRayLength;

		// 11개 정점 생성 (왼쪽 5개 + 중앙 1개 + 오른쪽 5개)
		TArray<FVector> ArcPoints;
		const int NumArcPoints = 11;

		for (int i = 0; i < NumArcPoints; ++i)
		{
			float t = static_cast<float>(i) / (NumArcPoints - 1); // 0.0 ~ 1.0

			FVector Point;
			if (t <= 0.5f)
			{
				// 왼쪽 끝점 -> 중앙 정점 (구면 보간)
				float localT = t * 2.0f; // 0.0 ~ 1.0
				FVector DirLeft = (LeftEndPoint - ApexWorld).GetNormalized();
				FVector DirCenter = (CenterPoint - ApexWorld).GetNormalized();

				// Slerp (구면 선형 보간)
				float cosAngle = FVector::Dot(DirLeft, DirCenter);
				float angle = std::acos(FMath::Clamp(cosAngle, -1.0f, 1.0f));

				if (angle < 0.001f)
				{
					// 각도가 매우 작으면 선형 보간 사용
					Point = FVector::Lerp(LeftEndPoint, CenterPoint, localT);
				}
				else
				{
					float sinAngle = std::sin(angle);
					float ratio1 = std::sin((1.0f - localT) * angle) / sinAngle;
					float ratio2 = std::sin(localT * angle) / sinAngle;
					FVector Dir = (DirLeft * ratio1 + DirCenter * ratio2).GetNormalized();

					// 거리도 보간
					float distance = FMath::Lerp(OuterRayLength, AttenuationRadius, localT);
					Point = ApexWorld + Dir * distance;
				}
			}
			else
			{
				// 중앙 정점 -> 오른쪽 끝점 (구면 보간)
				float localT = (t - 0.5f) * 2.0f; // 0.0 ~ 1.0
				FVector DirCenter = (CenterPoint - ApexWorld).GetNormalized();
				FVector DirRight = (RightEndPoint - ApexWorld).GetNormalized();

				// Slerp (구면 선형 보간)
				float cosAngle = FVector::Dot(DirCenter, DirRight);
				float angle = std::acos(FMath::Clamp(cosAngle, -1.0f, 1.0f));

				if (angle < 0.001f)
				{
					// 각도가 매우 작으면 선형 보간 사용
					Point = FVector::Lerp(CenterPoint, RightEndPoint, localT);
				}
				else
				{
					float sinAngle = std::sin(angle);
					float ratio1 = std::sin((1.0f - localT) * angle) / sinAngle;
					float ratio2 = std::sin(localT * angle) / sinAngle;
					FVector Dir = (DirCenter * ratio1 + DirRight * ratio2).GetNormalized();

					// 거리도 보간
					float distance = FMath::Lerp(AttenuationRadius, OuterRayLength, localT);
					Point = ApexWorld + Dir * distance;
				}
			}

			ArcPoints.Add(Point);
		}

		// 정점들을 선으로 연결
		for (int i = 0; i < NumArcPoints - 1; ++i)
		{
			StartPoints.Add(ArcPoints[i]);
			EndPoints.Add(ArcPoints[i + 1]);
			Colors.Add(OuterConeColor);
		}
	}

	// --- InnerCone 그리기 (선택적) ---
	if (InnerConeAngle > 0.0f && InnerConeAngle <= OuterConeAngle)
	{
		const float InnerAngleRad = DegreesToRadians(InnerConeAngle);

		// 방사형 라인 길이는 AttenuationRadius로 고정
		const float InnerRayLength = AttenuationRadius;

		// 아크의 가장 먼 지점이 AttenuationRadius가 되도록 ArcRadius 계산
		const float InnerArcRadius = (InnerAngleRad < HALF_PI && std::cos(InnerAngleRad) > 0.0001f)
			? AttenuationRadius / std::cos(InnerAngleRad)
			: AttenuationRadius;

		// 밑면 원을 위한 끝점들 저장
		TArray<FVector> InnerEndPoints;
		InnerEndPoints.Reserve(NumRays);

		// 1. 방사형 라인 (Apex에서 원뿔 가장자리로) - 길이 고정
		for (int i = 0; i < NumRays; ++i)
		{
			const float Angle = (static_cast<float>(i) / NumRays) * TWO_PI;
			const FVector RadialDir = (Right * std::cos(Angle) + ActualUp * std::sin(Angle)).GetNormalized();

			const FVector RayDir = (LightDirection * std::cos(InnerAngleRad) + RadialDir * std::sin(InnerAngleRad)).GetNormalized();
			const FVector RayEnd = ApexWorld + RayDir * InnerRayLength;

			StartPoints.Add(ApexWorld);
			EndPoints.Add(RayEnd);
			Colors.Add(InnerConeColor);

			InnerEndPoints.Add(RayEnd);
		}

		// 밑면 원 그리기 (방사형 라인의 끝점들을 연결)
		for (int i = 0; i < NumRays; ++i)
		{
			StartPoints.Add(InnerEndPoints[i]);
			EndPoints.Add(InnerEndPoints[(i + 1) % NumRays]);
			Colors.Add(InnerConeColor);
		}

		// InnerCone은 아크를 그리지 않음
	}

	// 렌더러에 라인 데이터 전달
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void USpotLightComponent::OnSerialized()
{
	Super::OnSerialized();

	ValidateConeAngles();

}

void USpotLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	DirectionGizmo = nullptr;
}
