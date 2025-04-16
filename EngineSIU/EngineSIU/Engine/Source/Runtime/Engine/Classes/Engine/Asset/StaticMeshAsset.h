#pragma once

#include "Hal/PlatformType.h"
#include "Container/Array.h"

struct FStaticMeshVertex
{
    float X, Y, Z;    // Position
    float R, G, B, A; // Color
    float NormalX, NormalY, NormalZ;
    float TangentX, TangentY, TangentZ;
    float U = 0, V = 0;
    uint32 MaterialIndex;
};

struct FStaticMeshRenderData
{
    FWString ObjectName;
    FString DisplayName;

    TArray<FStaticMeshVertex> Vertices;
    TArray<UINT> Indices;

    ID3D11Buffer* VertexBuffer;
    ID3D11Buffer* IndexBuffer;

    TArray<FObjMaterialInfo> Materials;
    TArray<FMaterialSubset> MaterialSubsets;

    FVector BoundingBoxMin;
    FVector BoundingBoxMax;
};
