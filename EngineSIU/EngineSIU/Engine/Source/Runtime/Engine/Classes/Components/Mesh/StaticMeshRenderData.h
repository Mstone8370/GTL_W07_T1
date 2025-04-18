#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"

struct FStaticMeshRenderData;

class UStaticMesh : public UObject
{
    DECLARE_CLASS(UStaticMesh, UObject)

public:
    UStaticMesh() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    const TArray<FStaticMaterial*>& GetMaterials() const { return materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& Out) const;
    FStaticMeshRenderData* GetRenderData() const { return staticMeshRenderData; }

    //ObjectName은 경로까지 포함
    FWString GetObjectName() const;

    void SetData(FStaticMeshRenderData* renderData);

private:
    FStaticMeshRenderData* staticMeshRenderData = nullptr;
    TArray<FStaticMaterial*> materials;
};
