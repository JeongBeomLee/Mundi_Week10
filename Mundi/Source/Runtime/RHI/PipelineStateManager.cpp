#include "pch.h"
#include "PipelineStateManager.h"

UPipelineStateManager& UPipelineStateManager::GetInstance()
{
    static UPipelineStateManager* Instance = nullptr;
    if (Instance == nullptr)
    {
        Instance = NewObject<UPipelineStateManager>();
        Instance->Initialize();
    }
    return *Instance;
}

void UPipelineStateManager::Initialize()
{

}