#include "pch.h"
#include "RenderManager.h"
#include "WorldPartitionManager.h"
#include "World.h"
#include "Renderer.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "PrimitiveComponent.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "TextRenderComponent.h"
#include "Gizmo/GizmoActor.h"
#include "Grid/GridActor.h"
#include "Octree.h"
#include "BVHierarchy.h"
#include "Occlusion.h"
#include "Frustum.h"
#include "ResourceManager.h"
#include "RHIDevice.h"
#include "StaticMesh.h"
#include "Material.h"
#include "Texture.h"
#include "RenderSettings.h"
#include "EditorEngine.h"
#include "DecalComponent.h"

IMPLEMENT_CLASS(URenderManager)

URenderManager::URenderManager()
{
	Renderer = GEngine.GetRenderer();
}

URenderManager::~URenderManager()
{
}

void URenderManager::BeginFrame()
{
	if (Renderer)
		Renderer->BeginFrame();
}

void URenderManager::EndFrame()
{
	if (Renderer)
		Renderer->EndFrame();
}
