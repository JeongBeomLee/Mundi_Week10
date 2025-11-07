#pragma once
#include "Object.h"

class UPipelineStateManager : public UObject
{
public:
	DECLARE_CLASS(UPipelineStateManager, UObject)
	static UPipelineStateManager& GetInstance();
	void Initialize();
};
