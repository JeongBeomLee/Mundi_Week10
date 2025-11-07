#include "pch.h"
#include "Level.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "PrimitiveComponent.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "DirectionalLightActor.h"
#include "AmbientLightActor.h"
#include "AmbientLightComponent.h"
#include "World.h"
#include "JsonSerializer.h"

static inline FString RemoveObjExtension(const FString& FileName)
{
    const FString Extension = ".obj";
    const uint64 Sep = FileName.find_last_of("/\\");
    const uint64 Start = (Sep == FString::npos) ? 0 : Sep + 1;
    uint64 End = FileName.size();
    if (End >= Extension.size() && FileName.compare(End - Extension.size(), Extension.size(), Extension) == 0)
        End -= Extension.size();
    if (Start <= End) return FileName.substr(Start, End - Start);
    return FileName;
}

std::unique_ptr<ULevel> ULevelService::CreateNewLevel()
{
    std::unique_ptr<ULevel> NewLevel = std::make_unique<ULevel>();
    NewLevel->SpawnDefaultActors();
    return NewLevel;
}

std::unique_ptr<ULevel> ULevelService::CreateDefaultLevel()
{
    std::unique_ptr<ULevel> NewLevel = std::make_unique<ULevel>();
    return NewLevel;
}

//어느 레벨이든 기본적으로 존재하는 엑터(디렉셔널 라이트) 생성
void ULevel::SpawnDefaultActors()
{
    ADirectionalLightActor* DirectionalLightActor = NewObject<ADirectionalLightActor>();
    this->AddActor(DirectionalLightActor);
    DirectionalLightActor->SetActorRotation(FQuat::FromAxisAngle(FVector(1.f, -1.f, 1.f), 30.f));
    AAmbientLightActor* AmbientLightActor = NewObject<AAmbientLightActor>();
    this->AddActor(AmbientLightActor);
    AmbientLightActor->GetLightComponent()->SetIntensity(0.1f);
   
}

void ULevel::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    struct FPerspectiveCameraData
    {
        FVector Location;
        FVector Rotation;
        float FOV;
        float NearClip;
        float FarClip;
    };

    if (bInIsLoading)
    {
        // 카메라 정보
        JSON PerspectiveCameraData;
        if (FJsonSerializer::ReadObject(InOutHandle, "PerspectiveCamera", PerspectiveCameraData))
        {
            // 카메라 정보
            ACameraActor* CamActor = GWorld->GetCameraActor();
            FPerspectiveCameraData CamData;
            if (CamActor)
            {
                // ReadObject 유틸리티 함수로 해당 뷰포트의 JSON 데이터를 안전하게 가져옴
                // 유틸리티 함수를 사용하여 반복적인 검사 없이 간결하게 데이터 파싱
                // 실패 시 각 함수 내부에서 로그를 남기고 기본값을 할당함
                FJsonSerializer::ReadVector(PerspectiveCameraData, "Location", CamData.Location);
                FJsonSerializer::ReadVector(PerspectiveCameraData, "Rotation", CamData.Rotation);
                FJsonSerializer::ReadArrayFloat(PerspectiveCameraData, "FOV", CamData.FOV);
                FJsonSerializer::ReadArrayFloat(PerspectiveCameraData, "NearClip", CamData.NearClip);
                FJsonSerializer::ReadArrayFloat(PerspectiveCameraData, "FarClip", CamData.FarClip);

                CamActor->SetActorLocation(CamData.Location);
                CamActor->SetRotationFromEulerAngles(CamData.Rotation);
                if (auto* CamComp = CamActor->GetCameraComponent())
                {
                    CamComp->SetFOV(CamData.FOV);
                    CamComp->SetClipPlanes(CamData.NearClip, CamData.FarClip);
                }
            }
        }

        // Actors 정보
        JSON ActorListJson;
        if (FJsonSerializer::ReadObject(InOutHandle, "Actors", ActorListJson))
        {
            // ObjectRange()를 사용하여 Primitives 객체의 모든 키-값 쌍을 순회
            for (auto& Pair : ActorListJson.ObjectRange())
            {
                // Pair.first는 ID 문자열, Pair.second는 단일 프리미티브의 JSON 데이터입니다.
                const FString& IdString = Pair.first;
                JSON& ActorDataJson = Pair.second;

                FString TypeString;
                FJsonSerializer::ReadString(ActorDataJson, "Type", TypeString);

                //UClass* NewClass = FActorTypeMapper::TypeToActor(TypeString);
                UClass* NewClass = UClass::FindClass(TypeString);

                UWorld* World = GWorld;

                // 유효성 검사: Class가 유효하고 AActor를 상속했는지 확인
                if (!NewClass || !NewClass->IsChildOf(AActor::StaticClass()))
                {
                    UE_LOG("SpawnActor failed: Invalid class provided.");
                    return;
                }

                // ObjectFactory를 통해 UClass*로부터 객체 인스턴스 생성
                AActor* NewActor = Cast<AActor>(ObjectFactory::NewObject(NewClass));
                if (!NewActor)
                {
                    UE_LOG("SpawnActor failed: ObjectFactory could not create an instance of");
                    return;
                }

                AddActor(NewActor);

                if (NewActor)
                {
                    NewActor->Serialize(bInIsLoading, ActorDataJson);
                }
            }
        }
    }
    else
    {
        // 기본 정보
        InOutHandle["Version"] = 1;
        InOutHandle["NextUUID"] = UObject::PeekNextUUID();

        // 카메라 정보
        const ACameraActor* Camera = GWorld->GetCameraActor();
        FPerspectiveCameraData CamData;
        if (Camera && Camera->GetCameraComponent())
        {
            const UCameraComponent* Cam = Camera->GetCameraComponent();
            CamData.Location = Camera->GetActorLocation();
            CamData.Rotation.X = 0.0f;
            CamData.Rotation.Y = Camera->GetCameraPitch();
            CamData.Rotation.Z = Camera->GetCameraYaw();
            CamData.FOV = Cam->GetFOV();
            CamData.NearClip = Cam->GetNearClip();
            CamData.FarClip = Cam->GetFarClip();
        }

        JSON CamreaJson = json::Object();
        CamreaJson["Location"] = FJsonSerializer::VectorToJson(CamData.Location);
        CamreaJson["Rotation"] = FJsonSerializer::VectorToJson(CamData.Rotation);
        CamreaJson["FarClip"] = FJsonSerializer::FloatToArrayJson(CamData.FarClip);
        CamreaJson["NearClip"] = FJsonSerializer::FloatToArrayJson(CamData.NearClip);
        CamreaJson["FOV"] = FJsonSerializer::FloatToArrayJson(CamData.FOV);

        InOutHandle["PerspectiveCamera"] = CamreaJson;

        // Actors 정보
        JSON ActorListJson = json::Object();
        for (AActor* Actor : Actors)
        {
            JSON ActorJson = json::Object();
            ActorJson["Type"] = Actor->GetClass()->Name;

            Actor->Serialize(bInIsLoading, ActorJson);

            ActorListJson[std::to_string(Actor->UUID)] = ActorJson;
        }
        InOutHandle["Actors"] = ActorListJson;
    }
}
