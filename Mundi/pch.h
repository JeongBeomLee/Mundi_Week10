#pragma once

// Feature Flags
// Uncomment to enable DDS texture caching (faster loading, uses Data/TextureCache/)
#define USE_DDS_CACHE
#define USE_OBJ_CACHE

// Linker
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// DirectXTK
#pragma comment(lib, "DirectXTK.lib")

// lua
#pragma comment(lib, "lua.lib")

// FMOD
#ifdef _DEBUG
#pragma comment(lib, "fmodL_vc.lib")
#else
#pragma comment(lib, "fmod_vc.lib")
#endif

// FBX
#ifdef _DEBUG
#pragma comment(lib, "libfbxsdk-md.lib")
#pragma comment(lib, "libxml2-md.lib") 
#pragma comment(lib, "zlib-md.lib")    
#else
#pragma comment(lib, "libfbxsdk-md.lib")
#pragma comment(lib, "libxml2-md.lib") 
#pragma comment(lib, "zlib-md.lib") 
#endif


// Standard Library (MUST come before UEContainer.h)
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <list>
#include <deque>
#include <string>
#include <array>
#include <algorithm>
#include <functional>
#include <memory>
#include <cmath>
#include <limits>
#include <iostream>
#include <fstream>
#include <utility>
#include <filesystem>
#include <sstream>
#include <iterator>

// Windows & DirectX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <cassert>
#include <ppl.h>

// Core Project Headers
#include "VertexData.h"
#include "UEContainer.h"
#include "Vector.h"
#include "Name.h"
#include "PathUtils.h"
#include "Object.h"
#include "ObjectFactory.h"
#include "ObjectMacros.h"
#include "Enums.h"
#include "Delegate.h"
#include "WeakPtr.h"
#include "GlobalConsole.h"
#include "D3D11RHI.h"
#include "World.h"
#include "ConstantBufferType.h"
#include "LogManager.h"
#include "TimeProfiler.h"

// d3dtk
#include "DirectXTK/SimpleMath.h"

// ImGui
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

// nlohmann
#include "nlohmann/json.hpp"

// lua
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

// FBX
#include "fbxsdk.h"

//Manager
#include "Renderer.h"
#include "InputManager.h"
#include "UIManager.h"
#include "ResourceManager.h"
#include "SoundManager.h"

#include "JsonSerializer.h"

#define RESOURCE UResourceManager::GetInstance()
#define UI UUIManager::GetInstance()
#define INPUT UInputManager::GetInstance()
#define RENDER URenderManager::GetInstance()
#define SLATE USlateManager::GetInstance()

//(월드 별 소유)
//#define PARTITION UWorldPartitionManager::GetInstance()
//#define SELECTION (GEngine.GetDefaultWorld()->GetSelectionManager())

extern TMap<FString, FString> EditorINI;
extern const FString GDataDir;
extern const FString GCacheDir;

//Editor
#include "EditorEngine.h"

//CUR ENGINE MODE
#define _EDITOR

extern UEditorEngine GEngine;

extern UWorld* GWorld;