#pragma once
#include "Object.h"

class UQuadManager : public UObject
{
public:
	DECLARE_CLASS(UQuadManager, UObject)

	UQuadManager();
	~UQuadManager();

	static UQuadManager* GetInstance()
	{
		static UQuadManager* Instance;
		if (!Instance)
		{
			Instance = NewObject<UQuadManager>();
		}
		return Instance;
	}

};

