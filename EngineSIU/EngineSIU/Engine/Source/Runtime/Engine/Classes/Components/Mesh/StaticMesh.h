#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"


class UStaticMesh : public UObject
{
    DECLARE_CLASS(UStaticMesh, UObject)

public:
    UStaticMesh() = default;
    virtual ~UStaticMesh() override;

    virtual UObject* Duplicate(UObject* InOuter) override;

    const TArray<FStaticMaterial*>& GetMaterials() const { return materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& Out) const;
    OBJ::FStaticMesh* GetRenderData() const { return staticMeshRenderData; }

    //ObjectName은 경로까지 포함
    FWString GetOjbectName() const
    {
        return staticMeshRenderData->ObjectName;
    }

    void SetData(OBJ::FStaticMesh* renderData);

private:
    OBJ::FStaticMesh* staticMeshRenderData = nullptr;
    TArray<FStaticMaterial*> materials;
};
