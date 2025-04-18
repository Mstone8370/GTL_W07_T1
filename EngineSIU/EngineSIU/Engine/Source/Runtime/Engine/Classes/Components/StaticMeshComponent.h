#pragma once
#include "Components/MeshComponent.h"
#include "Mesh/StaticMeshRenderData.h"

#include "Engine/Asset/StaticMeshAsset.h"

class UStaticMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

public:
    UStaticMeshComponent() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    
    void GetProperties(TMap<FString, FString>& OutProperties) const override;
    
    void SetProperties(const TMap<FString, FString>& InProperties) override;

    void SetselectedSubMeshIndex(const int& value) { selectedSubMeshIndex = value; }
    int GetselectedSubMeshIndex() const { return selectedSubMeshIndex; };

    virtual uint32 GetNumMaterials() const override;
    virtual UMaterial* GetMaterial(uint32 ElementIndex) const override;
    virtual uint32 GetMaterialIndex(FName MaterialSlotName) const override;
    virtual TArray<FName> GetMaterialSlotNames() const override;
    virtual void GetUsedMaterials(TArray<UMaterial*>& Out) const override;

    virtual int CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance) override;
    
    UStaticMesh* GetStaticMesh() const { return StaticMesh; }
    void SetStaticMesh(UStaticMesh* value)
    { 
        StaticMesh = value;
        OverrideMaterials.SetNum(value->GetMaterials().Num());
        AABB = FBoundingBox(StaticMesh->GetRenderData()->BoundingBoxMin, StaticMesh->GetRenderData()->BoundingBoxMax);
    }

protected:
    UStaticMesh* StaticMesh = nullptr;
    int selectedSubMeshIndex = -1;
};
