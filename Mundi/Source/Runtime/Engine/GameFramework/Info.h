#pragma once
#include "Actor.h"

//물리적 실체가 없고 월드의 세팅 데이터를 조작하는 Actor, AActor가 기본적으로 가지는 기본 프로퍼티 변수들을 조작해서 
//replication이나(서버에서 클라이언트로 복사, 동기화) 물리, 렌더링 고려대상에서 제거함으로써 경량화가 가능하기 때문에 계층 구조를 나눈다고 함.
//아직은 아무런 기능도 하지 않음
class AInfo : public AActor
{
public:

	DECLARE_CLASS(AInfo, AActor)

	AInfo() = default;

	DECLARE_DUPLICATE(AInfo)
	void DuplicateSubObjects() override;
private:
	

};