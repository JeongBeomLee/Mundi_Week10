#pragma once
#include "Vector.h"

/**
 * @brief 프로퍼티 UI 렌더링 유틸리티 함수 모음
 *
 * PropertyRenderer와 독립적인 재사용 가능한 ImGui 위젯 헬퍼
 */
class UPropertyUtils
{
public:
	/**
	 * @brief 색상 바가 있는 Vector3 입력 위젯 (X=Red, Y=Green, Z=Blue)
	 * @param label 라벨 텍스트
	 * @param value Vector 값 포인터
	 * @param speed 드래그 속도
	 * @param labelWidth 라벨 너비 (기본 80.0f)
	 * @return 값이 변경되었으면 true
	 */
	static bool RenderVector3WithColorBars(const char* label, FVector* value, float speed, float labelWidth = 80.0f);
};
