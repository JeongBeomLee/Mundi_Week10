#pragma once
#include "Object.h"

class UWorld;
class URenderer;
class ACameraActor;
class FViewport;
class FViewportClient;

struct FCandidateDrawable;

// High-level scene rendering orchestrator extracted from UWorld
class URenderManager : public UObject
{
public:
	DECLARE_CLASS(URenderManager, UObject)

	URenderManager();

	// Singleton accessor
	static URenderManager& GetInstance()
	{
		static URenderManager* Instance = nullptr;
		if (!Instance) Instance = NewObject<URenderManager>();
		return *Instance;
	}

	//// Render using camera derived from the viewport's client
	//void Render(UWorld* InWorld, FViewport* Viewport);

	//// Low-level: Renders with explicit camera
	//void RenderViewports(ACameraActor* Camera, FViewport* Viewport);

	// Optional frame hooks if you want to move frame begin/end here later
	void BeginFrame();
	void EndFrame();

	URenderer* GetRenderer() const { return Renderer; }

private:
	URenderer* Renderer = nullptr;

	~URenderManager() override;
};
