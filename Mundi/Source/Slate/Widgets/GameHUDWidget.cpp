#include "pch.h"
#include "GameHUDWidget.h"
#include "GameStateBase.h"
#include "RunnerGameMode.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "D3D11RHI.h"
#include <sstream>
#include <iomanip>

IMPLEMENT_CLASS(UGameHUDWidget)

UGameHUDWidget::UGameHUDWidget()
	: GameState(nullptr)
	, GameMode(nullptr)
	, GameStateChangedHandle(0)
	, ScoreChangedHandle(0)
	, TimerUpdatedHandle(0)
	, PlayerHealthDecreasedHandle(0)
	, CachedScore(0)
	, CachedElapsedTime(0.0f)
	, CachedGameStateText("Not Started")
	, CachedPlayerHealth(PLAYER_HEALTH)
	, ViewportX(0.0f)
	, ViewportY(0.0f)
	, ViewportWidth(1920.0f)
	, ViewportHeight(1080.0f)
	, HeartIconTexture(nullptr)
	, HeartIconWidth(0)
	, HeartIconHeight(0)
{
}

void UGameHUDWidget::Initialize()
{
	// 하트 아이콘 로드
	LoadHeartIcon();
}

void UGameHUDWidget::Update()
{
	// 델리게이트를 통해 자동으로 업데이트되므로 여기서는 아무것도 하지 않음
	// 필요시 추가 로직 작성 가능
}

void UGameHUDWidget::RenderWidget()
{
	// PIE 모드가 아니면 렌더링하지 않음
	if (!GWorld || !GWorld->bPie)
		return;

	// GameState 확인
	bool bIsGameOver = false;
	bool bIsVictory = false;
	bool bIsNotStarted = false;
	if (GameState.IsValid())
	{
		EGameState State = GameState.Get()->GetGameState();
		bIsGameOver = (State == EGameState::GameOver);
		bIsVictory = (State == EGameState::Victory);
		bIsNotStarted = (State == EGameState::NotStarted);
	}

	// 게임 상태에 따라 적절한 렌더링 함수 호출
	if (bIsNotStarted)
	{
		RenderNotStartedState();
	}
	else if (bIsGameOver || bIsVictory)
	{
		RenderGameOverState(bIsVictory);
	}
	else
	{
		RenderPlayingState();
	}
}

void UGameHUDWidget::SetGameState(AGameStateBase* InGameState)
{
	// 기존 델리게이트 바인딩 해제
	UnbindDelegates();

	if (InGameState)
	{
		GameState = TWeakPtr<AGameStateBase>(InGameState);

		// 델리게이트 바인딩 (핸들 저장)
		GameStateChangedHandle = InGameState->OnGameStateChanged.AddDynamic(this, &UGameHUDWidget::OnGameStateChanged_Handler);
		ScoreChangedHandle = InGameState->OnScoreChanged.AddDynamic(this, &UGameHUDWidget::OnScoreChanged_Handler);
		TimerUpdatedHandle = InGameState->OnTimerUpdated.AddDynamic(this, &UGameHUDWidget::OnTimerUpdated_Handler);

		// 초기 데이터 캐시
		CachedScore = InGameState->GetScore();
		CachedElapsedTime = InGameState->GetElapsedTime();
		CachedGameStateText = GetGameStateText();
	}
	else
	{
		GameState.Reset();

		// 초기값으로 리셋
		CachedScore = 0;
		CachedElapsedTime = 0.0f;
		CachedGameStateText = "No GameState";
	}
}

void UGameHUDWidget::SetGameMode(ARunnerGameMode* InGameMode)
{
	// 기존 GameMode 델리게이트 해제
	if (GameMode.IsValid())
	{
		ARunnerGameMode* Mode = GameMode.Get();
		Mode->OnPlayerHealthDecreased.RemoveDynamic(PlayerHealthDecreasedHandle);
	}

	if (InGameMode)
	{
		GameMode = TWeakPtr<ARunnerGameMode>(InGameMode);

		// 체력 감소 델리게이트 바인딩
		PlayerHealthDecreasedHandle = InGameMode->OnPlayerHealthDecreased.AddDynamic(this, &UGameHUDWidget::OnPlayerHealthDecreased_Handler);
	}
	else
	{
		GameMode.Reset();
	}
}

void UGameHUDWidget::SetViewportBounds(float X, float Y, float Width, float Height)
{
	ViewportX = X;
	ViewportY = Y;
	ViewportWidth = Width;
	ViewportHeight = Height;
}

void UGameHUDWidget::UnbindDelegates()
{
	if (GameState.IsValid())
	{
		AGameStateBase* State = GameState.Get();
		// 이 위젯의 바인딩만 제거 (다른 위젯의 바인딩은 유지)
		State->OnGameStateChanged.RemoveDynamic(GameStateChangedHandle);
		State->OnScoreChanged.RemoveDynamic(ScoreChangedHandle);
		State->OnTimerUpdated.RemoveDynamic(TimerUpdatedHandle);
	}

	if (GameMode.IsValid())
	{
		ARunnerGameMode* Mode = GameMode.Get();
		Mode->OnPlayerHealthDecreased.RemoveDynamic(PlayerHealthDecreasedHandle);
	}
}

void UGameHUDWidget::OnGameStateChanged_Handler(EGameState OldState, EGameState NewState)
{
	// 게임 상태 텍스트 업데이트
	CachedGameStateText = GetGameStateText();
	UE_LOG("GameHUDWidget: GameState changed from %d to %d", static_cast<int>(OldState), static_cast<int>(NewState));
}

void UGameHUDWidget::OnScoreChanged_Handler(int32 OldScore, int32 NewScore)
{
	// 스코어 캐시 업데이트
	CachedScore = NewScore;
	UE_LOG("GameHUDWidget: Score changed from %d to %d", OldScore, NewScore);
}

void UGameHUDWidget::OnTimerUpdated_Handler(float ElapsedTime)
{
	// 타이머 캐시 업데이트
	CachedElapsedTime = ElapsedTime;
}

void UGameHUDWidget::OnPlayerHealthDecreased_Handler(int32 CurrentHealth)
{
	// 체력 캐시 업데이트
	CachedPlayerHealth = CurrentHealth;
	UE_LOG("GameHUDWidget: Player health changed to %d", CurrentHealth);
}

FString UGameHUDWidget::FormatTime(float Seconds) const
{
	int32 Minutes = static_cast<int32>(Seconds) / 60;
	int32 Secs = static_cast<int32>(Seconds) % 60;

	std::ostringstream oss;
	oss << std::setfill('0') << std::setw(2) << Minutes << ":" << std::setw(2) << Secs;
	return oss.str();
}

FString UGameHUDWidget::GetGameStateText() const
{
	if (!GameState.IsValid())
		return "No GameState";

	EGameState State = GameState.Get()->GetGameState();
	switch (State)
	{
	case EGameState::NotStarted:
		return "Not Started";
	case EGameState::Playing:
		return "Playing";
	case EGameState::Paused:
		return "Paused";
	case EGameState::GameOver:
		return "Game Over";
	case EGameState::Victory:
		return "Victory!";
	default:
		return "Unknown";
	}
}

ImVec4 UGameHUDWidget::GetGameStateColor() const
{
	if (!GameState.IsValid())
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 흰색

	EGameState State = GameState.Get()->GetGameState();
	switch (State)
	{
	case EGameState::NotStarted:
		return ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // 회색
	case EGameState::Playing:
		return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // 녹색
	case EGameState::Paused:
		return ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // 노란색
	case EGameState::GameOver:
		return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // 빨간색
	case EGameState::Victory:
		return ImVec4(0.0f, 0.8f, 1.0f, 1.0f); // 하늘색
	default:
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 흰색
	}
}

void UGameHUDWidget::LoadHeartIcon()
{
	// ResourceManager를 통해 하트 아이콘 텍스처 로드
	UResourceManager& ResMgr = UResourceManager::GetInstance();
	UTexture* HeartTexture = ResMgr.Load<UTexture>("Data/Icon/heart.png");
	UTexture* EmptyHeartTexture = ResMgr.Load<UTexture>("Data/Icon/empty_heart.png");

	if (HeartTexture && HeartTexture->GetShaderResourceView())
	{
		HeartIconTexture = (void*)HeartTexture->GetShaderResourceView();
		EmptyHeartIconTexture = (void*)EmptyHeartTexture->GetShaderResourceView();
		HeartIconWidth = HeartTexture->GetWidth();
		HeartIconHeight = HeartTexture->GetHeight();
		UE_LOG("GameHUDWidget: Heart icon loaded successfully (%dx%d)", HeartIconWidth, HeartIconHeight);
	}
	else
	{
		UE_LOG("GameHUDWidget: Failed to load heart icon from Data/Icon/heart.png");
		HeartIconTexture = nullptr;
		HeartIconWidth = 0;
		HeartIconHeight = 0;
	}
}

void UGameHUDWidget::RenderHeartIcons()
{
	if (!HeartIconTexture)
		return;

	ImGuiIO& io = ImGui::GetIO();

	// 하트 아이콘 설정
	const float HeartSize = 50.0f; // 하트 크기
	const float HeartSpacing = 8.0f; // 하트 간격
	const float Padding = 20.0f; // 화면 가장자리에서의 패딩

	// 게임 화면 영역(Viewport) 기준 우측 상단 위치 계산
	const float TotalWidth = (HeartSize + HeartSpacing) * 3 - HeartSpacing + HeartSpacing + HeartSpacing;
	const float StartX = ViewportX + ViewportWidth - TotalWidth - Padding;
	const float StartY = ViewportY + Padding + 25.f;

	// 투명 배경의 윈도우 생성
	ImGui::SetNextWindowPos(ImVec2(StartX, StartY));
	ImGui::SetNextWindowSize(ImVec2(TotalWidth, HeartSize + HeartSpacing));
	ImGui::SetNextWindowBgAlpha(0.0f); // 완전 투명

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBackground; // 테두리 제거

	ImGui::Begin("##HeartIcons", nullptr, flags);

	// 하트 아이콘을 오른쪽부터 왼쪽으로 그리기
	for (int i = 0; i < PLAYER_HEALTH; ++i)
	{
		if (i > 0)
		{
			ImGui::SameLine(0.0f, HeartSpacing);
		}

		bool bShouldShow = i < CachedPlayerHealth;

		if (bShouldShow)
		{
			ImGui::Image(HeartIconTexture, ImVec2(HeartSize, HeartSize));
		}
		else
		{
			ImGui::Image(EmptyHeartIconTexture, ImVec2(HeartSize, HeartSize));
		}
	}

	ImGui::End();
}

void UGameHUDWidget::RenderTitleScreen()
{
	ImGuiIO& io = ImGui::GetIO();
	float time = static_cast<float>(ImGui::GetTime());

	// 우주 배경 효과 렌더링
	ImGui::SetNextWindowPos(ImVec2(ViewportX, ViewportY));
	ImGui::SetNextWindowSize(ImVec2(ViewportWidth, ViewportHeight));
	ImGui::SetNextWindowBgAlpha(0.0f);

	ImGuiWindowFlags spaceFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoBackground;

	if (ImGui::Begin("##TitleSpaceBackground", nullptr, spaceFlags))
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// 별 파티클 효과 (100개의 별)
		for (int i = 0; i < 100; ++i)
		{
			// 각 별마다 고유한 위치와 속도
			float starSpeed = 0.5f + (i % 5) * 0.3f; // 시차 효과 (가까운 별은 빠르게, 먼 별은 느리게)
			float x = ViewportX + fmodf(i * 173.7f + time * starSpeed * 20.0f, ViewportWidth);
			float y = ViewportY + fmodf(i * 271.3f, ViewportHeight);

			// 별 크기 (먼 별은 작게, 가까운 별은 크게)
			float baseSize = (i % 3 == 0) ? 2.5f : ((i % 2 == 0) ? 1.5f : 1.0f);
			float twinkle = sinf(time * 3.0f + i * 0.5f); // 반짝임 효과
			float size = baseSize + twinkle * 0.5f;

			// 별 밝기 (반짝임)
			float brightness = 0.5f + 0.5f * sinf(time * 2.0f + i * 0.3f);

			// 별 색상 (흰색, 파란색, 약간의 노란색 별)
			ImVec4 starColor;
			if (i % 7 == 0)
			{
				// 파란 별 (멀리 있는 별)
				starColor = ImVec4(0.7f, 0.8f, 1.0f, brightness * 0.8f);
			}
			else if (i % 11 == 0)
			{
				// 노란 별 (가까운 별)
				starColor = ImVec4(1.0f, 1.0f, 0.8f, brightness * 0.9f);
			}
			else
			{
				// 흰 별 (일반)
				starColor = ImVec4(1.0f, 1.0f, 1.0f, brightness * 0.7f);
			}

			// 별 그리기 (밝은 별은 글로우 효과)
			if (baseSize > 2.0f && brightness > 0.7f)
			{
				// 큰 별에 글로우 추가
				ImVec4 glowColor = starColor;
				glowColor.w *= 0.3f;
				drawList->AddCircleFilled(ImVec2(x, y), size + 2.0f, ImGui::ColorConvertFloat4ToU32(glowColor));
			}

			drawList->AddCircleFilled(ImVec2(x, y), size, ImGui::ColorConvertFloat4ToU32(starColor));
		}

		// 희미한 성운 효과 (더 큰 원으로 배경 분위기)
		for (int i = 0; i < 5; ++i)
		{
			float nebulaX = ViewportX + fmodf(i * 457.3f + time * 5.0f, ViewportWidth);
			float nebulaY = ViewportY + fmodf(i * 689.1f, ViewportHeight);
			float nebulaSize = 40.0f + sinf(time + i) * 10.0f;
			float nebulaAlpha = 0.03f + 0.02f * sinf(time * 0.5f + i);

			ImVec4 nebulaColor;
			if (i % 2 == 0)
			{
				// 보라색 성운
				nebulaColor = ImVec4(0.4f, 0.2f, 0.6f, nebulaAlpha);
			}
			else
			{
				// 파란색 성운
				nebulaColor = ImVec4(0.2f, 0.3f, 0.6f, nebulaAlpha);
			}

			drawList->AddCircleFilled(ImVec2(nebulaX, nebulaY), nebulaSize, ImGui::ColorConvertFloat4ToU32(nebulaColor));
		}
	}
	ImGui::End();

	// 타이틀 텍스트 렌더링 윈도우
	ImGui::SetNextWindowPos(ImVec2(ViewportX, ViewportY));
	ImGui::SetNextWindowSize(ImVec2(ViewportWidth, ViewportHeight));
	ImGui::SetNextWindowBgAlpha(0.0f); // 완전 투명

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoSavedSettings;

	ImGui::Begin("##TitleScreen", nullptr, flags);

	// 상단 여백
	ImGui::Dummy(ImVec2(0, ViewportHeight * 0.25f));

	// "INFINITY RUNNER" 타이틀 (큰 글씨, 무지개색 반짝임)
	const char* titleText = "INFINITY RUNNER";
	ImFont* font = ImGui::GetFont();
	float originalScale = font->Scale;
	time = static_cast<float>(ImGui::GetTime());

	// 큰 폰트 스케일 (3.0배)
	font->Scale = 3.0f;
	ImGui::PushFont(font);

	float titleWidth = ImGui::CalcTextSize(titleText).x;
	float startX = (ViewportWidth - titleWidth) * 0.5f;
	ImVec2 titlePos = ImGui::GetCursorScreenPos();
	titlePos.x = startX;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	float fontSize = ImGui::GetFontSize();

	// 각 글자를 무지개색으로 그리기
	float xOffset = 0.0f;
	size_t textLen = strlen(titleText);
	for (size_t i = 0; i < textLen; ++i)
	{
		char charStr[2] = { titleText[i], '\0' };
		ImVec2 charSize = ImGui::CalcTextSize(charStr);

		// 무지개색 계산 (HSV to RGB)
		float hue = fmodf(time * 0.5f + i * 0.05f, 1.0f); // 시간에 따라 변화 + 글자마다 오프셋
		float saturation = 1.0f;
		float value = 0.8f + 0.2f * sinf(time * 3.0f + i * 0.3f); // 반짝임 효과
		ImVec4 color;
		ImGui::ColorConvertHSVtoRGB(hue, saturation, value, color.x, color.y, color.z);
		color.w = 1.0f;

		ImU32 col = ImGui::ColorConvertFloat4ToU32(color);

		// 글로우 효과 (외곽선)
		for (float offsetX = -1.5f; offsetX <= 1.5f; offsetX += 1.5f)
		{
			for (float offsetY = -1.5f; offsetY <= 1.5f; offsetY += 1.5f)
			{
				if (offsetX == 0.0f && offsetY == 0.0f) continue;
				ImVec4 glowColor = ImVec4(color.x * 0.5f, color.y * 0.5f, color.z * 0.5f, 0.6f);
				ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glowColor);
				drawList->AddText(font, fontSize,
					ImVec2(titlePos.x + xOffset + offsetX, titlePos.y + offsetY),
					glowCol, charStr, nullptr);
			}
		}

		// 메인 글자
		drawList->AddText(font, fontSize,
			ImVec2(titlePos.x + xOffset, titlePos.y),
			col, charStr, nullptr);

		xOffset += charSize.x;
	}

	ImGui::Dummy(ImVec2(titleWidth, fontSize));
	ImGui::PopFont();
	font->Scale = originalScale;

	// 타이틀과 부제목 사이 간격
	ImGui::Dummy(ImVec2(0, 30));

	// "- SPACE TO START -" 부제목 (중간 글씨, 무지개색 반짝임)
	const char* subtitleText = "- SPACE TO START -";

	// 중간 폰트 스케일 (1.8배)
	font->Scale = 1.8f;
	ImGui::PushFont(font);

	float subtitleWidth = ImGui::CalcTextSize(subtitleText).x;
	float subtitleStartX = (ViewportWidth - subtitleWidth) * 0.5f;
	ImVec2 subtitlePos = ImGui::GetCursorScreenPos();
	subtitlePos.x = subtitleStartX;

	fontSize = ImGui::GetFontSize();

	// 각 글자를 무지개색으로 그리기
	xOffset = 0.0f;
	textLen = strlen(subtitleText);
	for (size_t i = 0; i < textLen; ++i)
	{
		char charStr[2] = { subtitleText[i], '\0' };
		ImVec2 charSize = ImGui::CalcTextSize(charStr);

		// 무지개색 계산 (타이틀과 반대 방향으로 회전)
		float hue = fmodf(time * 0.5f - i * 0.05f + 0.5f, 1.0f);
		float saturation = 0.9f;
		float value = 0.7f + 0.3f * sinf(time * 2.5f - i * 0.2f); // 반짝임 효과
		ImVec4 color;
		ImGui::ColorConvertHSVtoRGB(hue, saturation, value, color.x, color.y, color.z);
		color.w = 1.0f;

		ImU32 col = ImGui::ColorConvertFloat4ToU32(color);

		// 글로우 효과
		for (float offsetX = -1.0f; offsetX <= 1.0f; offsetX += 1.0f)
		{
			for (float offsetY = -1.0f; offsetY <= 1.0f; offsetY += 1.0f)
			{
				if (offsetX == 0.0f && offsetY == 0.0f) continue;
				ImVec4 glowColor = ImVec4(color.x * 0.4f, color.y * 0.4f, color.z * 0.4f, 0.5f);
				ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glowColor);
				drawList->AddText(font, fontSize,
					ImVec2(subtitlePos.x + xOffset + offsetX, subtitlePos.y + offsetY),
					glowCol, charStr, nullptr);
			}
		}

		// 메인 글자
		drawList->AddText(font, fontSize,
			ImVec2(subtitlePos.x + xOffset, subtitlePos.y),
			col, charStr, nullptr);

		xOffset += charSize.x;
	}

	ImGui::Dummy(ImVec2(subtitleWidth, fontSize));
	ImGui::PopFont();
	font->Scale = originalScale;

	ImGui::End();
}

void UGameHUDWidget::RenderNotStartedState()
{
	// 타이틀 화면 렌더링
	RenderTitleScreen();

	// 스페이스바를 누르면 게임 시작
	if (ImGui::IsKeyPressed(ImGuiKey_Space))
	{
		if (GameMode.IsValid())
		{
			ARunnerGameMode* Mode = GameMode.Get();
			Mode->StartGame();
			UE_LOG("GameHUDWidget: Game started via Space key");
		}
	}
}

void UGameHUDWidget::RenderGameOverState(bool bIsVictory)
{
	float time = static_cast<float>(ImGui::GetTime());

	// 멋진 배경 오버레이 (그라데이션 + 파티클 효과)
	ImGui::SetNextWindowPos(ImVec2(ViewportX, ViewportY));
	ImGui::SetNextWindowSize(ImVec2(ViewportWidth, ViewportHeight));
	ImGui::SetNextWindowBgAlpha(0.0f);

	ImGuiWindowFlags overlayFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoBackground;

	if (ImGui::Begin("##GameOverOverlay", nullptr, overlayFlags))
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 overlayMin = ImVec2(ViewportX, ViewportY);
		ImVec2 overlayMax = ImVec2(ViewportX + ViewportWidth, ViewportY + ViewportHeight);

		// 그라데이션 배경
		ImU32 topColor, bottomColor;
		if (bIsVictory)
		{
			// Victory: 어두운 파랑에서 금색으로
			topColor = IM_COL32(10, 10, 30, 220);
			bottomColor = IM_COL32(40, 30, 10, 220);
		}
		else
		{
			// Game Over: 패배한 느낌의 짙은 어둠 (거의 검정에 약간의 빨강)
			topColor = IM_COL32(20, 5, 5, 235);
			bottomColor = IM_COL32(5, 0, 0, 235);
		}
		drawList->AddRectFilledMultiColor(overlayMin, overlayMax, topColor, topColor, bottomColor, bottomColor);

		// 파티클 효과 (떠다니는 작은 점들)
		for (int i = 0; i < 50; ++i)
		{
			// 각 파티클마다 고유한 움직임
			float particleTime = time * 0.3f + i * 0.5f;
			float x = ViewportX + fmodf(i * 137.5f + time * 50.0f, ViewportWidth);
			float y = ViewportY + fmodf(i * 219.3f + sinf(particleTime) * 100.0f, ViewportHeight);
			float size = 2.0f + sinf(time * 2.0f + i) * 1.5f;
			float alpha = 0.3f + 0.3f * sinf(time * 3.0f + i * 0.7f);

			ImVec4 particleColor;
			if (bIsVictory)
			{
				// Victory: 금색 파티클
				particleColor = ImVec4(1.0f, 0.9f, 0.3f, alpha);
			}
			else
			{
				// Game Over: 어두운 피빛 파티클 (패배감)
				particleColor = ImVec4(0.6f, 0.1f, 0.05f, alpha * 0.8f);
			}

			drawList->AddCircleFilled(ImVec2(x, y), size, ImGui::ColorConvertFloat4ToU32(particleColor));
		}
	}
	ImGui::End();

	// 뷰포트 중앙에 팝업 창 (고정 크기)
	const float popupWidth = 600.0f;
	const float popupHeight = 450.0f;

	ImGui::SetNextWindowPos(ImVec2(ViewportX + (ViewportWidth - popupWidth) * 0.5f, ViewportY + (ViewportHeight - popupHeight) * 0.5f));
	ImGui::SetNextWindowSize(ImVec2(popupWidth, popupHeight));

	// 배경색 설정
	ImVec4 bgColor;
	if (bIsVictory)
	{
		// Victory: 밝은 골드/크림색 배경
		bgColor = ImVec4(0.95f, 0.92f, 0.85f, 0.98f);
	}
	else
	{
		// Game Over: 패배한 느낌의 어두운 배경 (거의 검정에 약간의 빨강)
		bgColor = ImVec4(0.08f, 0.05f, 0.05f, 0.98f);
	}

	ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);

	ImGuiWindowFlags popupFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar; // 스크롤바 제거

	if (ImGui::Begin("##GameEndPopup", nullptr, popupFlags))
	{
		// 멋진 테두리 그리기
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImDrawList* borderDrawList = ImGui::GetWindowDrawList();

		// 외곽 글로우 효과
		for (float thickness = 8.0f; thickness > 0.0f; thickness -= 2.0f)
		{
			float alpha = (8.0f - thickness) / 8.0f * 0.4f;
			ImVec4 glowColor;

			if (bIsVictory)
			{
				// Victory: 황금빛 글로우
				float hue = fmodf(time * 0.2f, 1.0f);
				ImGui::ColorConvertHSVtoRGB(hue, 0.7f, 1.0f, glowColor.x, glowColor.y, glowColor.z);
				glowColor.w = alpha;
			}
			else
			{
				// Game Over: 빨간빛 글로우
				float pulse = 0.5f + 0.5f * sinf(time * 3.0f);
				glowColor = ImVec4(1.0f, 0.2f + pulse * 0.3f, 0.0f, alpha);
			}

			borderDrawList->AddRect(
				ImVec2(windowPos.x - thickness, windowPos.y - thickness),
				ImVec2(windowPos.x + windowSize.x + thickness, windowPos.y + windowSize.y + thickness),
				ImGui::ColorConvertFloat4ToU32(glowColor),
				8.0f, // 둥근 모서리
				0,
				thickness
			);
		}

		// 메인 테두리
		ImVec4 borderColor;
		if (bIsVictory)
		{
			// Victory: 밝은 금색 테두리
			float hue = fmodf(time * 0.2f, 1.0f);
			ImGui::ColorConvertHSVtoRGB(hue, 0.8f, 1.0f, borderColor.x, borderColor.y, borderColor.z);
			borderColor.w = 1.0f;
		}
		else
		{
			// Game Over: 밝은 빨강 테두리
			float pulse = 0.5f + 0.5f * sinf(time * 3.0f);
			borderColor = ImVec4(1.0f, 0.2f + pulse * 0.4f, 0.1f, 1.0f);
		}

		borderDrawList->AddRect(
			windowPos,
			ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
			ImGui::ColorConvertFloat4ToU32(borderColor),
			8.0f, // 둥근 모서리
			0,
			3.0f // 테두리 두께
		);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 25));

		ImGui::Dummy(ImVec2(0, 40)); // 상단 여백

		// 타이틀 메시지 (애니메이션 효과)
		const char* titleText = bIsVictory ? "VICTORY!" : "GAME OVER";
		ImFont* font = ImGui::GetFont();
		float originalScale = font->Scale;
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// 타이틀 크기 및 펄스 효과
		float titleScale = 3.0f + 0.15f * sinf(time * 4.0f);
		font->Scale = titleScale;
		ImGui::PushFont(font);

		float titleWidth = ImGui::CalcTextSize(titleText).x;
		ImVec2 titlePos = ImGui::GetCursorScreenPos();
		titlePos.x = (popupWidth - titleWidth) * 0.5f + ImGui::GetWindowPos().x;

		// 흔들림 효과
		float shakeX = sinf(time * 15.0f) * 2.0f;
		float shakeY = cosf(time * 12.0f) * 1.5f;
		titlePos.x += shakeX;
		titlePos.y += shakeY;

		float fontSize = ImGui::GetFontSize();
		size_t textLen = strlen(titleText);

		// 각 글자를 애니메이션으로 그리기
		float xOffset = 0.0f;
		for (size_t i = 0; i < textLen; ++i)
		{
			char charStr[2] = { titleText[i], '\0' };
			ImVec2 charSize = ImGui::CalcTextSize(charStr);

			ImVec4 color;
			if (bIsVictory)
			{
				// Victory: 무지개색 + 금색
				float hue = fmodf(time * 0.3f + i * 0.08f, 1.0f);
				float saturation = 0.8f;
				float value = 0.9f + 0.1f * sinf(time * 5.0f + i * 0.5f);
				ImGui::ColorConvertHSVtoRGB(hue, saturation, value, color.x, color.y, color.z);
				color.w = 1.0f;
			}
			else
			{
				// Game Over: 빨강-주황 사이 펄스
				float pulse = 0.5f + 0.5f * sinf(time * 3.0f + i * 0.4f);
				color = ImVec4(1.0f, 0.2f + pulse * 0.3f, 0.0f, 1.0f);
			}

			ImU32 col = ImGui::ColorConvertFloat4ToU32(color);

			// 강한 글로우 효과
			for (float radius = 3.0f; radius > 0.0f; radius -= 1.0f)
			{
				for (float angle = 0.0f; angle < 6.28f; angle += 0.785f)
				{
					float offsetX = cosf(angle) * radius;
					float offsetY = sinf(angle) * radius;
					ImVec4 glowColor = ImVec4(color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, 0.4f / radius);
					ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glowColor);
					drawList->AddText(font, fontSize,
						ImVec2(titlePos.x + xOffset + offsetX, titlePos.y + offsetY),
						glowCol, charStr, nullptr);
				}
			}

			// 메인 글자
			drawList->AddText(font, fontSize,
				ImVec2(titlePos.x + xOffset, titlePos.y),
				col, charStr, nullptr);

			xOffset += charSize.x;
		}

		ImGui::Dummy(ImVec2(titleWidth, fontSize));
		ImGui::PopFont();
		font->Scale = originalScale;

		ImGui::Dummy(ImVec2(0, 20));

		// 최종 스코어 표시 (글로우 효과)
		font->Scale = 1.8f;
		ImGui::PushFont(font);
		FString scoreText = "Final Score: " + std::to_string(CachedScore);

		float scoreWidth = ImGui::CalcTextSize(scoreText.c_str()).x;
		ImVec2 scorePos = ImGui::GetCursorScreenPos();
		scorePos.x = (popupWidth - scoreWidth) * 0.5f + ImGui::GetWindowPos().x;
		fontSize = ImGui::GetFontSize();

		// 글로우
		ImVec4 scoreColor = bIsVictory ? ImVec4(0.6f, 0.45f, 0.1f, 1.0f) : ImVec4(1.0f, 0.85f, 0.85f, 1.0f);
		for (float offsetX = -1.0f; offsetX <= 1.0f; offsetX += 1.0f)
		{
			for (float offsetY = -1.0f; offsetY <= 1.0f; offsetY += 1.0f)
			{
				if (offsetX == 0.0f && offsetY == 0.0f) continue;
				ImVec4 glowColor = ImVec4(scoreColor.x * 0.5f, scoreColor.y * 0.5f, scoreColor.z * 0.5f, 0.5f);
				ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glowColor);
				drawList->AddText(font, fontSize,
					ImVec2(scorePos.x + offsetX, scorePos.y + offsetY),
					glowCol, scoreText.c_str(), nullptr);
			}
		}

		ImU32 scoreCol = ImGui::ColorConvertFloat4ToU32(scoreColor);
		drawList->AddText(font, fontSize, scorePos, scoreCol, scoreText.c_str(), nullptr);
		ImGui::Dummy(ImVec2(scoreWidth, fontSize));
		ImGui::PopFont();
		font->Scale = originalScale;

		// 최종 시간 표시 (글로우 효과)
		font->Scale = 1.8f;
		ImGui::PushFont(font);
		FString timeText = "Time: " + FormatTime(CachedElapsedTime);

		float timeWidth = ImGui::CalcTextSize(timeText.c_str()).x;
		ImVec2 timePos = ImGui::GetCursorScreenPos();
		timePos.x = (popupWidth - timeWidth) * 0.5f + ImGui::GetWindowPos().x;
		fontSize = ImGui::GetFontSize();

		// 글로우
		for (float offsetX = -1.0f; offsetX <= 1.0f; offsetX += 1.0f)
		{
			for (float offsetY = -1.0f; offsetY <= 1.0f; offsetY += 1.0f)
			{
				if (offsetX == 0.0f && offsetY == 0.0f) continue;
				ImVec4 glowColor = ImVec4(scoreColor.x * 0.5f, scoreColor.y * 0.5f, scoreColor.z * 0.5f, 0.5f);
				ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glowColor);
				drawList->AddText(font, fontSize,
					ImVec2(timePos.x + offsetX, timePos.y + offsetY),
					glowCol, timeText.c_str(), nullptr);
			}
		}

		drawList->AddText(font, fontSize, timePos, scoreCol, timeText.c_str(), nullptr);
		ImGui::Dummy(ImVec2(timeWidth, fontSize));
		ImGui::PopFont();
		font->Scale = originalScale;

		ImGui::Dummy(ImVec2(0, 25));

		// 재시작 안내 메시지 (깜빡임 효과)
		const char* restartText = "Press R to Play Again";
		float blinkAlpha = 0.5f + 0.5f * sinf(time * 4.0f);

		font->Scale = 1.2f;
		ImGui::PushFont(font);

		float restartWidth = ImGui::CalcTextSize(restartText).x;
		ImVec2 restartPos = ImGui::GetCursorScreenPos();
		restartPos.x = (popupWidth - restartWidth) * 0.5f + ImGui::GetWindowPos().x;
		fontSize = ImGui::GetFontSize();

		ImVec4 restartColor;
		if (bIsVictory)
		{
			// Victory: 어두운 회색
			restartColor = ImVec4(0.3f, 0.3f, 0.3f, blinkAlpha);
		}
		else
		{
			// Game Over: 매우 밝은 회색 (어두운 배경에 잘 보이도록)
			restartColor = ImVec4(0.95f, 0.95f, 0.95f, blinkAlpha);
		}
		ImU32 restartCol = ImGui::ColorConvertFloat4ToU32(restartColor);

		drawList->AddText(font, fontSize, restartPos, restartCol, restartText, nullptr);
		ImGui::Dummy(ImVec2(restartWidth, fontSize));
		ImGui::PopFont();
		font->Scale = originalScale;

		ImGui::PopStyleVar();
	}
	ImGui::End();
	ImGui::PopStyleColor(); // 배경색 복원

	// 게임 오버/승리 상태에서 R 키 입력 체크
	if (ImGui::IsKeyPressed(ImGuiKey_R))
	{
		if (GameMode.IsValid())
		{
			ARunnerGameMode* Mode = GameMode.Get();
			Mode->RestartGame();
		}
	}
}

void UGameHUDWidget::RenderPlayingState()
{
	float time = static_cast<float>(ImGui::GetTime());

	// 우주 배경 효과 렌더링
	ImGui::SetNextWindowPos(ImVec2(ViewportX, ViewportY));
	ImGui::SetNextWindowSize(ImVec2(ViewportWidth, ViewportHeight));
	ImGui::SetNextWindowBgAlpha(0.0f);

	ImGuiWindowFlags spaceFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoBackground;

	if (ImGui::Begin("##SpaceBackground", nullptr, spaceFlags))
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// 별 파티클 효과 (100개의 별)
		for (int i = 0; i < 100; ++i)
		{
			// 각 별마다 고유한 위치와 속도
			float starSpeed = 0.5f + (i % 5) * 0.3f; // 시차 효과 (가까운 별은 빠르게, 먼 별은 느리게)
			float x = ViewportX + fmodf(i * 173.7f + time * starSpeed * 20.0f, ViewportWidth);
			float y = ViewportY + fmodf(i * 271.3f, ViewportHeight);

			// 별 크기 (먼 별은 작게, 가까운 별은 크게)
			float baseSize = (i % 3 == 0) ? 2.5f : ((i % 2 == 0) ? 1.5f : 1.0f);
			float twinkle = sinf(time * 3.0f + i * 0.5f); // 반짝임 효과
			float size = baseSize + twinkle * 0.5f;

			// 별 밝기 (반짝임)
			float brightness = 0.5f + 0.5f * sinf(time * 2.0f + i * 0.3f);

			// 별 색상 (흰색, 파란색, 약간의 노란색 별)
			ImVec4 starColor;
			if (i % 7 == 0)
			{
				// 파란 별 (멀리 있는 별)
				starColor = ImVec4(0.7f, 0.8f, 1.0f, brightness * 0.8f);
			}
			else if (i % 11 == 0)
			{
				// 노란 별 (가까운 별)
				starColor = ImVec4(1.0f, 1.0f, 0.8f, brightness * 0.9f);
			}
			else
			{
				// 흰 별 (일반)
				starColor = ImVec4(1.0f, 1.0f, 1.0f, brightness * 0.7f);
			}

			// 별 그리기 (밝은 별은 글로우 효과)
			if (baseSize > 2.0f && brightness > 0.7f)
			{
				// 큰 별에 글로우 추가
				ImVec4 glowColor = starColor;
				glowColor.w *= 0.3f;
				drawList->AddCircleFilled(ImVec2(x, y), size + 2.0f, ImGui::ColorConvertFloat4ToU32(glowColor));
			}

			drawList->AddCircleFilled(ImVec2(x, y), size, ImGui::ColorConvertFloat4ToU32(starColor));
		}

		// 희미한 성운 효과 (더 큰 원으로 배경 분위기)
		for (int i = 0; i < 5; ++i)
		{
			float nebulaX = ViewportX + fmodf(i * 457.3f + time * 5.0f, ViewportWidth);
			float nebulaY = ViewportY + fmodf(i * 689.1f, ViewportHeight);
			float nebulaSize = 40.0f + sinf(time + i) * 10.0f;
			float nebulaAlpha = 0.03f + 0.02f * sinf(time * 0.5f + i);

			ImVec4 nebulaColor;
			if (i % 2 == 0)
			{
				// 보라색 성운
				nebulaColor = ImVec4(0.4f, 0.2f, 0.6f, nebulaAlpha);
			}
			else
			{
				// 파란색 성운
				nebulaColor = ImVec4(0.2f, 0.3f, 0.6f, nebulaAlpha);
			}

			drawList->AddCircleFilled(ImVec2(nebulaX, nebulaY), nebulaSize, ImGui::ColorConvertFloat4ToU32(nebulaColor));
		}
	}
	ImGui::End();

	// 일반 HUD (좌상단, 배경 없음)
	const float padding = 20.0f;
	const float windowWidth = 250.0f;
	const float windowHeight = 100.0f;

	ImGui::SetNextWindowPos(ImVec2(ViewportX + padding, ViewportY + padding + 25.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f); // 완전 투명

	// 윈도우 플래그: 배경 제거
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBackground; // 배경 및 테두리 제거

	if (ImGui::Begin("##GameHUD", nullptr, windowFlags))
	{
		ImFont* font = ImGui::GetFont();
		float originalScale = font->Scale;
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// 스코어 표시 (크게, 글로우 효과)
		font->Scale = 2.3f;
		ImGui::PushFont(font);
		FString scoreText = "Score: " + std::to_string(CachedScore);

		ImVec2 scorePos = ImGui::GetCursorScreenPos();
		float fontSize = ImGui::GetFontSize();

		// 무지개색 + 반짝임 효과
		float hue = fmodf(time * 0.3f, 1.0f);
		float pulse = 0.85f + 0.15f * sinf(time * 2.0f);
		ImVec4 scoreColor;
		ImGui::ColorConvertHSVtoRGB(hue, 0.6f, pulse, scoreColor.x, scoreColor.y, scoreColor.z);
		scoreColor.w = 1.0f;

		// 강한 글로우 효과 (3단계)
		for (float radius = 4.0f; radius > 0.0f; radius -= 1.5f)
		{
			for (float angle = 0.0f; angle < 6.28f; angle += 0.785f)
			{
				float offsetX = cosf(angle) * radius;
				float offsetY = sinf(angle) * radius;
				ImVec4 glowColor = ImVec4(scoreColor.x * 0.7f, scoreColor.y * 0.7f, scoreColor.z * 0.7f, 0.3f / radius);
				ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glowColor);
				drawList->AddText(font, fontSize,
					ImVec2(scorePos.x + offsetX, scorePos.y + offsetY),
					glowCol, scoreText.c_str(), nullptr);
			}
		}

		// 메인 텍스트
		ImU32 scoreCol = ImGui::ColorConvertFloat4ToU32(scoreColor);
		drawList->AddText(font, fontSize, scorePos, scoreCol, scoreText.c_str(), nullptr);
		ImGui::Dummy(ImGui::CalcTextSize(scoreText.c_str()));
		ImGui::PopFont();
		font->Scale = originalScale;

		ImGui::Dummy(ImVec2(0, 5)); // 간격

		// 타이머 표시 (크게, 글로우 효과, MM:SS 형식)
		font->Scale = 2.3f;
		ImGui::PushFont(font);
		FString timeText = "Time: " + FormatTime(CachedElapsedTime);

		ImVec2 timePos = ImGui::GetCursorScreenPos();
		fontSize = ImGui::GetFontSize();

		// 시안색 + 반짝임 효과
		float timePulse = 0.8f + 0.2f * sinf(time * 2.5f + 1.0f);
		ImVec4 timeColor = ImVec4(0.3f, 0.9f, 1.0f, 1.0f); // 시안색
		timeColor.x *= timePulse;
		timeColor.y *= timePulse;
		timeColor.z *= timePulse;

		// 강한 글로우 효과
		for (float radius = 4.0f; radius > 0.0f; radius -= 1.5f)
		{
			for (float angle = 0.0f; angle < 6.28f; angle += 0.785f)
			{
				float offsetX = cosf(angle) * radius;
				float offsetY = sinf(angle) * radius;
				ImVec4 glowColor = ImVec4(timeColor.x * 0.7f, timeColor.y * 0.7f, timeColor.z * 0.7f, 0.3f / radius);
				ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(glowColor);
				drawList->AddText(font, fontSize,
					ImVec2(timePos.x + offsetX, timePos.y + offsetY),
					glowCol, timeText.c_str(), nullptr);
			}
		}

		// 메인 텍스트
		ImU32 timeCol = ImGui::ColorConvertFloat4ToU32(timeColor);
		drawList->AddText(font, fontSize, timePos, timeCol, timeText.c_str(), nullptr);
		ImGui::Dummy(ImGui::CalcTextSize(timeText.c_str()));
		ImGui::PopFont();
		font->Scale = originalScale;
	}
	ImGui::End();

	// 하트 아이콘 렌더링 (우측 상단)
	RenderHeartIcons();

	// 제작자 정보 표시 (좌하단)
	const float creditPadding = 20.0f;
	const float creditWidth = 400.0f;
	const float creditHeight = 100.0f;

	ImGui::SetNextWindowPos(ImVec2(ViewportX + creditPadding, ViewportY + ViewportHeight - creditHeight - creditPadding), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(creditWidth, creditHeight), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f); // 완전 투명

	ImGuiWindowFlags creditFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBackground; // 배경 제거

	if (ImGui::Begin("##Credits", nullptr, creditFlags))
	{
		ImFont* font = ImGui::GetFont();
		float originalScale = font->Scale;
		ImDrawList* creditDrawList = ImGui::GetWindowDrawList();

		// "Created by" 레이블 (작게, 글로우)
		font->Scale = 1.1f;
		ImGui::PushFont(font);

		const char* labelText = "Created by";
		ImVec2 labelPos = ImGui::GetCursorScreenPos();
		float labelFontSize = ImGui::GetFontSize();

		// 은은한 글로우
		float labelAlpha = 0.6f + 0.2f * sinf(time * 1.5f);
		ImVec4 labelColor = ImVec4(0.7f, 0.8f, 1.0f, labelAlpha); // 밝은 하늘색

		for (float offsetX = -1.0f; offsetX <= 1.0f; offsetX += 1.0f)
		{
			for (float offsetY = -1.0f; offsetY <= 1.0f; offsetY += 1.0f)
			{
				if (offsetX == 0.0f && offsetY == 0.0f) continue;
				ImVec4 glowColor = ImVec4(labelColor.x * 0.5f, labelColor.y * 0.5f, labelColor.z * 0.5f, 0.3f);
				creditDrawList->AddText(font, labelFontSize,
					ImVec2(labelPos.x + offsetX, labelPos.y + offsetY),
					ImGui::ColorConvertFloat4ToU32(glowColor), labelText, nullptr);
			}
		}

		creditDrawList->AddText(font, labelFontSize, labelPos,
			ImGui::ColorConvertFloat4ToU32(labelColor), labelText, nullptr);
		ImGui::Dummy(ImGui::CalcTextSize(labelText));
		ImGui::PopFont();
		font->Scale = originalScale;

		ImGui::Dummy(ImVec2(0, 5));

		// 제작자 이름들 (각각 다른 색상으로 빛나게)
		font->Scale = 1.5f;
		ImGui::PushFont(font);

		const char* creators[] = { "홍신화", "조창근", "이정범", "김상천" };
		float xOffset = 0.0f;

		for (int i = 0; i < 4; ++i)
		{
			ImVec2 namePos = ImGui::GetCursorScreenPos();
			namePos.x += xOffset;
			float nameFontSize = ImGui::GetFontSize();

			// 각 이름마다 다른 색상
			float hue = (i / 4.0f + time * 0.1f);
			hue = fmodf(hue, 1.0f);
			float namePulse = 0.9f + 0.1f * sinf(time * 2.0f + i * 0.5f);
			ImVec4 nameColor;
			ImGui::ColorConvertHSVtoRGB(hue, 0.5f, namePulse, nameColor.x, nameColor.y, nameColor.z);
			nameColor.w = 1.0f;

			// 글로우 효과
			for (float radius = 2.0f; radius > 0.0f; radius -= 1.0f)
			{
				for (float angle = 0.0f; angle < 6.28f; angle += 1.57f)
				{
					float offsetX = cosf(angle) * radius;
					float offsetY = sinf(angle) * radius;
					ImVec4 glowColor = ImVec4(nameColor.x * 0.6f, nameColor.y * 0.6f, nameColor.z * 0.6f, 0.4f / radius);
					creditDrawList->AddText(font, nameFontSize,
						ImVec2(namePos.x + offsetX, namePos.y + offsetY),
						ImGui::ColorConvertFloat4ToU32(glowColor), creators[i], nullptr);
				}
			}

			// 메인 이름
			creditDrawList->AddText(font, nameFontSize, namePos,
				ImGui::ColorConvertFloat4ToU32(nameColor), creators[i], nullptr);

			float nameWidth = ImGui::CalcTextSize(creators[i]).x;
			xOffset += nameWidth + 15.0f; // 이름 사이 간격
		}

		ImGui::Dummy(ImVec2(xOffset, ImGui::GetFontSize()));
		ImGui::PopFont();
		font->Scale = originalScale;
	}
	ImGui::End();

	// P 키 입력으로 Pause/Resume 토글
	if (ImGui::IsKeyPressed(ImGuiKey_P))
	{
		if (GameMode.IsValid() && GameState.IsValid())
		{
			ARunnerGameMode* Mode = GameMode.Get();
			EGameState CurrentState = GameState.Get()->GetGameState();

			if (CurrentState == EGameState::Playing)
			{
				Mode->PauseGame();
				UE_LOG("GameHUDWidget: Game paused via P key");
			}
			else if (CurrentState == EGameState::Paused)
			{
				Mode->ResumeGame();
				UE_LOG("GameHUDWidget: Game resumed via P key");
			}
		}
	}
}
