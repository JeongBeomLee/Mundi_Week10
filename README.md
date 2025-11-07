# Mundi 엔진 - Week09 GameJam#3

## 프로젝트 정보
- **Week:** 09 (GameJam #3)
- **저자:** 이정범, 홍신화, 조창근, 김상천
- **주제:** Lua 스크립팅 시스템 + Delegate 기반 Actor 제어

---

## 📋 Week09 주요 구현 내용

### 1. Lua 스크립팅 시스템 (Sol2 기반)

#### 1.1 UScriptManager - Lua 중앙 관리자
- **경로:** `Source/Runtime/LuaScripting/UScriptManager.h/cpp`
- **싱글톤 패턴:** 전역 단일 Lua 상태 관리
- **주요 기능:**
  - C++ 타입을 Lua에 등록 (new_usertype)
  - 스크립트 파일 로드 및 실행
  - Actor별 독립적 Lua 환경(environment) 제공
  - 핫 리로드 지원 (파일 변경 감지 시 자동 리로드)

#### 1.2 C++ ↔ Lua 타입 바인딩
- **등록된 C++ 타입:**
  - `FVector`: X, Y, Z 접근, Add/Sub/Mul 메서드, 연산자 오버로딩 (+, -, *, /)
  - `FQuat`: MakeFromEuler() 생성자
  - `AActor`: GetLocation, SetLocation, GetRotation, SetRotation, GetScale, SetScale, AddWorldLocation 등
  - `USceneComponent`: GetSceneId() 메서드
  - `FName`: ToString() 메서드

#### 1.3 데이터 구조
- **FScript:** 스크립트 메타데이터 저장 구조체
  - `sol::environment Env`: 각 스크립트의 독립적 실행 환경
  - `sol::table Table`: 스크립트 데이터 테이블
  - `FLuaTemplateFunctions`: BeginPlay, Tick, OnOverlap, EndPlay 함수 포인터
  - `LastModifiedTime`: 핫 리로드용 파일 수정 시간
- **FLuaLocalValue:** 스크립트에 전달되는 로컬 데이터
  - `MyActor`: 스크립트가 제어할 대상 Actor 포인터

### 2. Delegate 시스템

#### 2.1 단일 Delegate (TDelegate)
- **기능:**
  - 단일 함수 바인딩 및 실행
  - 람다 함수 지원
  - 멤버 함수 바인딩 (BindDynamic)
  - Bind/Unbind/IsBound/Execute 메서드

#### 2.2 멀티캐스트 Delegate (TMulticastDelegate)
- **기능:**
  - 여러 함수 동시 바인딩
  - Add/AddDynamic로 핸들 반환
  - RemoveDynamic(Handle)로 선택적 제거
  - RemoveAll()로 전체 제거
  - Broadcast로 동시 실행

#### 2.3 Delegate 매크로
- `DECLARE_DELEGATE_OneParam`: 1개 파라미터 delegate 선언
- `DECLARE_MULTICAST_DELEGATE_TwoParams`: 2개 파라미터 multicast delegate 선언
- 다양한 파라미터 개수 지원

### 3. Actor Transform 제어 (Lua)

#### 3.1 Lua 스크립트에서 Actor 조작
```lua
-- 위치 제어
local newPos = FVector.new(100, 200, 300)
MyActor:SetLocation(newPos)

-- 회전 제어
local rotation = FQuat.MakeFromEuler(10, 80, 20)
MyActor:SetRotation(rotation)

-- 스케일 제어
local scale = FVector.new(2, 2, 2)
MyActor:SetScale(scale)

-- 월드 상대 이동
local deltaPos = FVector.new(10, 0, 0)
MyActor:AddWorldLocation(deltaPos)
```

#### 3.2 Lua 기본 함수 (템플릿)
```lua
function BeginPlay()
    -- Actor 시작 시 호출
end

function Tick(deltaTime)
    -- 매 프레임 호출
    -- deltaTime: 이전 프레임부터의 경과 시간(초)
end

function OnOverlap()
    -- 충돌 발생 시 호출
end

function EndPlay()
    -- Actor 종료 시 호출
end
```

#### 3.3 Delegate를 통한 Transform 변경 알림
- Lua에서 Actor 변경 시 Delegate 호출
- C++ 람다가 Broadcast로 변경 사항 수신
- 이벤트 기반 아키텍처 구현

### 4. 핫 리로드 기능

#### 4.1 파일 변경 감지
- `CheckAndHotReloadLuaScript()`: 매 프레임 호출
- 파일 수정 시간(LastModifiedTime) 비교
- 변경 감지 시 자동 리로드

#### 4.2 안전한 리로드
- 기존 상태 백업 (Env, Table, Functions)
- 새로운 스크립트 로드 시도
- 실패 시 백업으로 즉시 롤백
- 게임 실행 중 로직 수정 가능

### 5. 테스트 및 검증

#### 5.1 main.cpp의 테스트 함수들
- **TestDelegate()**: Delegate 시스템 9가지 테스트
  - 람다 바인딩, 멤버 함수 바인딩, Unbind
  - 멀티캐스트, 핸들 제거, Clear
  - 매크로 테스트, 여러 멤버 함수, Operator() 오버로드

- **TestLua()**: Lua 스크립트 로드 테스트
  - template.lua 파일 로드 검증
  - 함수 정의 확인

- **TestLuaWithDelegateTransform()**: 통합 테스트 (3가지)
  1. Lua로 Actor Transform 직접 변경
  2. Delegate로 Transform 변경 알림 수신
  3. Lua 스크립트로 애니메이션 구현 (원형 운동)

#### 5.2 테스트 결과
- 모든 기능 동작 확인
- 메시지 박스로 성공/실패 보고

### 6. 기타 개선사항

#### 6.1 스크립트 구조화
- 경로: `Scripts/` 폴더
- 기본 템플릿: `Scripts/template.lua`
- Actor별 스크립트 자동 생성

#### 6.2 빌드 설정
- **CLAUDE.md 참고:** PowerShell로만 빌드 실행
- MSBuild 명령어 정확히 준수

#### 6.3 코드 스타일
- 모든 주석은 `//` 사용 (NOT `/* */`)
- 한글 주석 강제
- OOP 원칙 준수

---

## 🎮 사용 방법

### Lua 스크립팅 시작
1. Actor 클래스에서 BeginPlay() 시 스크립트 부착:
```cpp
FLuaLocalValue LocalValue;
LocalValue.MyActor = this;
UScriptManager::GetInstance().AttachScriptTo(LocalValue, "my_actor.lua");
```

2. Tick()에서 스크립트의 Lua Tick 호출:
```cpp
auto& ScriptsByOwner = UScriptManager::GetInstance().GetScriptsByOwner();
if (ScriptsByOwner.find(this) != ScriptsByOwner.end()) {
    for (FScript* Script : ScriptsByOwner[this]) {
        if (Script->LuaTemplateFunctions.Tick.valid()) {
            Script->LuaTemplateFunctions.Tick(DeltaTime);
        }
    }
}
```

### Lua 스크립트 작성 (Scripts/my_actor.lua)
```lua
function BeginPlay()
    PrintToConsole("Actor started!")
end

function Tick(deltaTime)
    -- Actor 위치 업데이트
    local pos = MyActor:GetLocation()
    local newPos = FVector.new(pos.X + 10, pos.Y, pos.Z)
    MyActor:SetLocation(newPos)
end

function OnOverlap()
    PrintToConsole("Collision detected!")
end

function EndPlay()
    PrintToConsole("Actor ended!")
end
```

### 핫 리로드 활용
- Engine Tick에서 주기적으로 호출:
```cpp
UScriptManager::GetInstance().CheckAndHotReloadLuaScript();
```
- Lua 파일 저장 시 자동으로 리로드되어 변경사항 즉시 반영

### Delegate 사용
```cpp
// 단일 Delegate
TDelegate<float> OnDamage;
OnDamage.Bind([](float damage) {
    PrintToConsole("Damage: " + std::to_string(damage));
});
OnDamage.Execute(50.0f);

// 멀티캐스트 Delegate
TMulticastDelegate<FVector> OnLocationChanged;
auto handle = OnLocationChanged.Add([](FVector newPos) {
    PrintToConsole(newPos.ToString());
});
OnLocationChanged.Broadcast(FVector(100, 200, 300));
OnLocationChanged.RemoveDynamic(handle);
```

### 테스트 실행
- Visual Studio에서 `TestLuaWithDelegateTransform()` 함수 실행
- 메시지 박스로 결과 확인

---

## 📘 Mundi 엔진 렌더링 기준

> 🚫 **경고: 이 내용은 Mundi 엔진 렌더링 기준의 근본입니다.**
> 삭제하거나 수정하면 엔진 전반의 좌표계 및 버텍스 연산이 깨집니다.
> **반드시 유지하십시오.**

### 기본 좌표계

* **좌표계:** Z-Up, **왼손 좌표계 (Left-Handed)**
* **버텍스 시계 방향 (CW)** 이 **앞면(Face Front)** 으로 간주됩니다.
  > → **DirectX의 기본 설정**을 그대로 따릅니다.

### OBJ 파일 Import 규칙

* OBJ 포맷은 **오른손 좌표계 + CCW(반시계)** 버텍스 순서를 사용한다고 가정합니다.
  > → 블렌더에서 OBJ 포맷으로 Export 시 기본적으로 이렇게 저장되기 때문입니다.
* 따라서 OBJ를 로드할 때, 엔진 내부 좌표계와 일치하도록 자동 변환을 수행합니다.

```cpp
FObjImporter::LoadObjModel(... , bIsRightHanded = true) // 기본값
```

즉, OBJ를 **Right-Handed → Left-Handed**,
**CCW → CW** 방향으로 변환하여 엔진의 렌더링 방식과 동일하게 맞춥니다.

### 블렌더(Blender) Export 설정

* 블렌더에서 모델을 **Z-Up, X-Forward** 설정으로 Export하여
  Mundi 엔진에 Import 시 **동일한 방향을 바라보게** 됩니다.

> 💡 참고:
> 블렌더에서 축 설정을 변경해도 **좌표계나 버텍스 순서 자체는 변하지 않습니다.**
> 단지 **기본 회전 방향만 바뀌므로**, Mundi 엔진에서는 항상 같은 방식으로 Import하면 됩니다.

### 좌표계 정리

| 구분     | Mundi 엔진 내부 표현      | Mundi 엔진이 해석하는 OBJ   | OBJ Import 결과 |
| ------ | ----------------- | ------------------ | ----------------- |
| 좌표계    | Z-Up, Left-Handed | Z-Up, Right-Handed | Z-Up, Left-Handed |
| 버텍스 순서 | CW (시계 방향)        | CCW (반시계 방향)       | CW |
