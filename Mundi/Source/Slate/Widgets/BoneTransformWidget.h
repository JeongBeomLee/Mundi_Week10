#pragma once
#include "Widgets/Widget.h"
#include <functional>

class USkeletalMeshComponent;

/**
 * @brief Bone Transform 편집 UI 위젯
 *
 * 설계:
 * - UWidget 상속 (기존 패턴 준수)
 * - Transform 편집 UI + Revert 버튼
 * - 콜백으로 Revert 이벤트 전달
 * - SDetailsWindow 패턴 준수
 *
 * 책임:
 * - 선택된 본의 Local Transform 편집 UI 렌더링 (미리보기 전용)
 * - Position, Rotation, Scale 입력
 * - Revert 버튼 렌더링 및 이벤트 처리
 */
class UBoneTransformWidget : public UWidget
{
public:
	DECLARE_CLASS(UBoneTransformWidget, UWidget)

	UBoneTransformWidget();

	// UWidget 인터페이스
	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

	/**
	 * @brief 외부 데이터 주입 (SDetailsWindow 패턴)
	 * @param Component Preview mesh component (nullptr 안전)
	 * @param BoneIndex 선택된 본 인덱스 포인터 (부모 위젯의 상태)
	 */
	void SetPreviewComponent(USkeletalMeshComponent* Component) { PreviewComponent = Component; }
	void SetSelectedBoneIndex(int32* BoneIndex) { SelectedBoneIndexPtr = BoneIndex; }

	/**
	 * @brief 콜백 설정 (Revert 버튼 클릭 시 호출)
	 * @param OnRevert Revert 버튼 클릭 시 콜백
	 */
	void SetRevertCallback(std::function<void()> OnRevert)
	{
		OnRevertCallback = OnRevert;
	}

	/**
	 * @brief Euler 캐시 리셋 (Revert 시 호출)
	 */
	void ResetEulerCache()
	{
		LastEditedBoneIndex = -1;
	}

private:
	// 외부에서 주입된 데이터 (포인터/참조)
	USkeletalMeshComponent* PreviewComponent = nullptr;
	int32* SelectedBoneIndexPtr = nullptr;

	// 콜백 함수
	std::function<void()> OnRevertCallback;

	// Gimbal lock 방지: Euler 각도 캐시 (FBone 패턴)
	FVector CachedLocalRotationEuler = FVector(0, 0, 0);
	int32 LastEditedBoneIndex = -1;
};
