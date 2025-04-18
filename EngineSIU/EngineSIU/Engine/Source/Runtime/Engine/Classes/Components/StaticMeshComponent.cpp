#include "Components/StaticMeshComponent.h"

#include "Engine/FObjLoader.h"
#include "Launch/EngineLoop.h"
#include "UObject/Casts.h"
#include "UObject/ObjectFactory.h"

#include "GameFramework/Actor.h"

UObject* UStaticMeshComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));

    NewComponent->StaticMesh = StaticMesh;
    NewComponent->selectedSubMeshIndex = selectedSubMeshIndex;

    return NewComponent;
}

void UStaticMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    
    //StaticMesh 경로 저장
    UStaticMesh* CurrentMesh = GetStaticMesh(); 
    if (CurrentMesh != nullptr) {

        // 1. std::wstring 경로 얻기
        std::wstring PathWString = CurrentMesh->GetObjectName(); // 이 함수가 std::wstring 반환 가정

        // 2. std::wstring을 FString으로 변환
        FString PathFString(PathWString.c_str()); // c_str()로 const wchar_t* 얻어서 FString 생성
       // PathFString = CurrentMesh->ConvertToRelativePathFromAssets(PathFString);

        FWString PathWString2 = PathFString.ToWideString();

        
        OutProperties.Add(TEXT("StaticMeshPath"), PathFString);
    } else
    {
        OutProperties.Add(TEXT("StaticMeshPath"), TEXT("None")); // 메시 없음 명시
    }
}

void UStaticMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;

    
    // --- StaticMesh 설정 ---
    TempStr = InProperties.Find(TEXT("StaticMeshPath"));
    if (TempStr) // 키가 존재하는지 확인
    {
        if (*TempStr != TEXT("None")) // 값이 "None"이 아닌지 확인
        {
            // 경로 문자열로 UStaticMesh 에셋 로드 시도
           
            if (UStaticMesh* MeshToSet = FObjManager::CreateStaticMesh(*TempStr))
            {
                SetStaticMesh(MeshToSet); // 성공 시 메시 설정
                UE_LOG(LogLevel::Display, TEXT("Set StaticMesh '%s' for %s"), **TempStr, *GetName());
            }
            else
            {
                // 로드 실패 시 경고 로그
                UE_LOG(LogLevel::Warning, TEXT("Could not load StaticMesh '%s' for %s"), **TempStr, *GetName());
                SetStaticMesh(nullptr); // 안전하게 nullptr로 설정
            }
        }
        else // 값이 "None"이면
        {
            SetStaticMesh(nullptr); // 명시적으로 메시 없음 설정
            UE_LOG(LogLevel::Display, TEXT("Set StaticMesh to None for %s"), *GetName());
        }
    }
    else // 키 자체가 없으면
    {
        // 키가 없는 경우 어떻게 처리할지 결정 (기본값 유지? nullptr 설정?)
        // 여기서는 기본값을 유지하거나, 안전하게 nullptr로 설정할 수 있습니다.
        // SetStaticMesh(nullptr); // 또는 아무것도 안 함
        UE_LOG(LogLevel::Display, TEXT("StaticMeshPath key not found for %s, mesh unchanged."), *GetName());
    }
}

uint32 UStaticMeshComponent::GetNumMaterials() const
{
    if (StaticMesh == nullptr) return 0;

    return StaticMesh->GetMaterials().Num();
}

UMaterial* UStaticMeshComponent::GetMaterial(uint32 ElementIndex) const
{
    if (StaticMesh != nullptr)
    {
        if (OverrideMaterials[ElementIndex] != nullptr)
        {
            return OverrideMaterials[ElementIndex];
        }
    
        if (StaticMesh->GetMaterials().IsValidIndex(ElementIndex))
        {
            return StaticMesh->GetMaterials()[ElementIndex]->Material;
        }
    }
    return nullptr;
}

uint32 UStaticMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
    if (StaticMesh == nullptr) return -1;

    return StaticMesh->GetMaterialIndex(MaterialSlotName);
}

TArray<FName> UStaticMeshComponent::GetMaterialSlotNames() const
{
    TArray<FName> MaterialNames;
    if (StaticMesh == nullptr) return MaterialNames;

    for (const FStaticMaterial* Material : StaticMesh->GetMaterials())
    {
        MaterialNames.Emplace(Material->MaterialSlotName);
    }

    return MaterialNames;
}

void UStaticMeshComponent::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    if (StaticMesh == nullptr) return;
    StaticMesh->GetUsedMaterials(Out);
    for (int materialIndex = 0; materialIndex < GetNumMaterials(); materialIndex++)
    {
        if (OverrideMaterials[materialIndex] != nullptr)
        {
            Out[materialIndex] = OverrideMaterials[materialIndex];
        }
    }
}

int UStaticMeshComponent::CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance)
{
    if (StaticMesh == nullptr || !AABB.Intersect(rayOrigin, rayDirection, pfNearHitDistance))
    {
        return 0;
    }
    
    int32 IntersectionCount = 0;

    FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();

    const FStaticMeshVertex* Vertices = RenderData->Vertices.GetData();
    const int32 VertexNum = RenderData->Vertices.Num();
    const UINT* Indices = RenderData->Indices.GetData();
    const int IndexNum = RenderData->Indices.Num();

    if (!Vertices) return 0;
    BYTE* Positions = reinterpret_cast<BYTE*>(RenderData->Vertices.GetData());

    int32 PrimitivesNum = (!Indices) ? (VertexNum / 3) : (IndexNum / 3);
    float NearHitDistance = FLT_MAX;
    for (int i = 0; i < PrimitivesNum; i++)
    {
        int32 Idx0;
        int32 Idx1;
        int32 Idx2;
        if (!Indices)
        {
            Idx0 = i * 3;
            Idx1 = i * 3 + 1;
            Idx2 = i * 3 + 2;
        }
        else
        {
            Idx0 = Indices[i * 3];
            Idx2 = Indices[i * 3 + 1];
            Idx1 = Indices[i * 3 + 2];
        }

        // 각 삼각형의 버텍스 위치를 FVector로 불러옵니다.
        uint32 stride = sizeof(FStaticMeshVertex);
        FVector v0 = *reinterpret_cast<FVector*>(Positions + Idx0 * stride);
        FVector v1 = *reinterpret_cast<FVector*>(Positions + Idx1 * stride);
        FVector v2 = *reinterpret_cast<FVector*>(Positions + Idx2 * stride);

        float HitDistance;
        if (IntersectRayTriangle(rayOrigin, rayDirection, v0, v1, v2, HitDistance)) {
            if (HitDistance < NearHitDistance) {
                pfNearHitDistance = NearHitDistance = HitDistance;
            }
            IntersectionCount++;
        }

    }
    return IntersectionCount;
}
