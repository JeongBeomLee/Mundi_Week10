#include "pch.h"
#include "ComponentDetailRenderer.h"
#include "ImGui/imgui.h"
#include "SpotLightComponent.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"
#include "SkeletalMeshComponent.h"
#include "UIManager.h"
#include "Windows/SSkeletalMeshEditorWindow.h"

void UComponentDetailRenderer::RenderCustomUI(UActorComponent* Component)
{
	if (!Component)
		return;

	// 런타임 타입 체크 → 컴파일 타임 오버로딩 디스패치
	if (auto* SpotLight = Cast<USpotLightComponent>(Component))
		RenderCustomUIImpl(SpotLight);
	else if (auto* DirLight = Cast<UDirectionalLightComponent>(Component))
		RenderCustomUIImpl(DirLight);
	else if (auto* PointLight = Cast<UPointLightComponent>(Component))
		RenderCustomUIImpl(PointLight);
	else if (auto* SkeletalMesh = Cast<USkeletalMeshComponent>(Component))
		RenderCustomUIImpl(SkeletalMesh);
	else
		RenderCustomUIImpl(Component);  // Fallback
}

// ===== SpotLight UI =====
// TODO: 기존 TargetActorTransformWidget.cpp의 코드를 여기로 이동 (리팩토링)
void UComponentDetailRenderer::RenderCustomUIImpl(USpotLightComponent* Component)
{
	// 현재는 비워둠 (나중에 리팩토링 시 구현)
}

// ===== DirectionalLight UI =====
// TODO: 기존 TargetActorTransformWidget.cpp의 코드를 여기로 이동 (리팩토링)
void UComponentDetailRenderer::RenderCustomUIImpl(UDirectionalLightComponent* Component)
{
	// 현재는 비워둠 (나중에 리팩토링 시 구현)
}

// ===== PointLight UI =====
// TODO: 기존 TargetActorTransformWidget.cpp의 코드를 여기로 이동 (리팩토링)
void UComponentDetailRenderer::RenderCustomUIImpl(UPointLightComponent* Component)
{
	// 현재는 비워둠 (나중에 리팩토링 시 구현)
}

// ===== SkeletalMesh UI (신규) =====
void UComponentDetailRenderer::RenderCustomUIImpl(USkeletalMeshComponent* Component)
{
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Text("[Skeletal Mesh Editor]");
	ImGui::Spacing();

	// Bone 정보 표시
	int32 BoneCount = Component->GetBoneCount();
	ImGui::Text("Total Bones: %d", BoneCount);

	if (Component->SelectedBoneIndex >= 0)
	{
		FBone* SelectedBone = Component->GetBone(Component->SelectedBoneIndex);
		if (SelectedBone)
		{
			ImGui::Text("Selected: %s (Index: %d)",
				SelectedBone->Name.c_str(),
				Component->SelectedBoneIndex);
		}
	}
	else
	{
		ImGui::TextDisabled("No bone selected");
	}

	ImGui::Spacing();

	// Editor 열기 버튼
	if (ImGui::Button("Open Skeletal Mesh Editor", ImVec2(200, 30)))
	{
		// UUIManager를 통해 창 찾기 또는 생성 (UCameraBlendEditorWindow 패턴)
		UUIWindow* SkeletalMeshEditorWindow = UUIManager::GetInstance().FindUIWindow("Skeletal Mesh Editor");
		if (!SkeletalMeshEditorWindow)
		{
			// 윈도우가 없으면 생성 후 등록
			SkeletalMeshEditorWindow = new SSkeletalMeshEditorWindow();
			SkeletalMeshEditorWindow->Initialize();
			UUIManager::GetInstance().RegisterUIWindow(SkeletalMeshEditorWindow);
		}

		// SetTargetComponent 호출 (다운캐스트 필요)
		SSkeletalMeshEditorWindow* Editor = static_cast<SSkeletalMeshEditorWindow*>(SkeletalMeshEditorWindow);
		Editor->SetTargetComponent(Component);

		// 윈도우 표시
		Editor->SetWindowState(EUIWindowState::Visible);

		UE_LOG("SkeletalMeshEditor: Window opened for component %p", Component);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Bone hierarchy editor and transform manipulation");
	}
}
