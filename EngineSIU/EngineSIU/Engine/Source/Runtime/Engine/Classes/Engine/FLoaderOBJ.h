#pragma once

#include "Define.h"
#include "EngineLoop.h"
#include "Container/Map.h"
#include "HAL/PlatformType.h"
#include "Serialization/Serializer.h"

class UStaticMesh;
struct FManagerOBJ;

struct FLoaderOBJ
{
    // Obj Parsing (*.obj to FObjInfo)
    static bool ParseOBJ(const FString& ObjFilePath, FObjInfo& OutObjInfo);

    // Material Parsing (*.obj to MaterialInfo)
    static bool ParseMaterial(FObjInfo& OutObjInfo, OBJ::FStaticMesh& OutFStaticMesh);

    // Convert the Raw data to Cooked data (FStaticMeshRenderData)
    static bool ConvertToStaticMesh(const FObjInfo& RawData, OBJ::FStaticMesh& OutStaticMesh);

    static bool CreateTextureFromFile(const FWString& Filename);

    static void ComputeBoundingBox(const TArray<FStaticMeshVertex>& InVertices, FVector& OutMinVector, FVector& OutMaxVector);

private:
    static void CalculateTangent(FStaticMeshVertex& PivotVertex, const FStaticMeshVertex& Vertex1, const FStaticMeshVertex& Vertex2);
};

struct FManagerOBJ
{
public:
    static OBJ::FStaticMesh* LoadObjStaticMeshAsset(const FString& PathFileName);

    static void CombineMaterialIndex(OBJ::FStaticMesh& OutFStaticMesh);

    static bool SaveStaticMeshToBinary(const FWString& FilePath, const OBJ::FStaticMesh& StaticMesh);

    static bool LoadStaticMeshFromBinary(const FWString& FilePath, OBJ::FStaticMesh& OutStaticMesh);

    static UMaterial* CreateMaterial(FObjMaterialInfo materialInfo);

    static TMap<FString, UMaterial*>& GetMaterials() { return materialMap; }

    static UMaterial* GetMaterial(FString name);

    static int GetMaterialNum() { return materialMap.Num(); }

    static UStaticMesh* CreateStaticMesh(const FString& filePath);

    static const TMap<FWString, UStaticMesh*>& GetStaticMeshes() { return StaticMeshMap; }

    static UStaticMesh* GetStaticMesh(FWString name);

    static int GetStaticMeshNum() { return StaticMeshMap.Num(); }

private:
    inline static TMap<FString, OBJ::FStaticMesh*> ObjStaticMeshMap;
    inline static TMap<FWString, UStaticMesh*> StaticMeshMap;
    inline static TMap<FString, UMaterial*> materialMap;
};
