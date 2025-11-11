#pragma once
#include "ActorComponent.h"

/**
 * @brief 컴포넌트별 커스텀 DetailPanel UI 렌더러
 * @note PropertyRenderer는 리플렉션 기반 property만 처리
 * @note 이 클래스는 추가 UI (버튼, 시각화, 커스텀 위젯) 담당
 *
 * 설계 원칙:
 * - Single Responsibility: UI 렌더링만
 * - Open-Closed: 새 타입 추가 시 함수 오버로딩만 추가
 * - Non-Invasive: 기존 코드 수정 없이 확장
 */
class UComponentDetailRenderer
{
public:
	/**
	 * @brief 컴포넌트의 커스텀 UI 렌더링 (메인 진입점)
	 * @param Component 대상 컴포넌트 (nullptr 안전)
	 */
	static void RenderCustomUI(UActorComponent* Component);

private:
	// ===== 함수 오버로딩으로 타입별 구현 =====
	static void RenderCustomUIImpl(class USpotLightComponent* Component);
	static void RenderCustomUIImpl(class UDirectionalLightComponent* Component);
	static void RenderCustomUIImpl(class UPointLightComponent* Component);

	// Fallback (매칭 안 되는 타입은 아무것도 안 함)
	static void RenderCustomUIImpl(UActorComponent* Component) {}
};
