struct AABBData
{
    float3 Position;
    float Padding1;
    
    float3 Extent;
    float Padding2;
};
cbuffer ConstantBufferDebugAABB : register(b10)
{
    AABBData DataAABB[8];
}

struct SphereData
{
    float3 Position;
    float Radius;
};
cbuffer ConstantBufferDebugSphere : register(b10)
{
    SphereData DataSphere[8];
}

struct ConeData
{
    float3 ApexPosition;
    float InnerRadius;
    
    float OuterRadius;
    float3 Direction;
    
    float Height;
    float3 Padding;
};
cbuffer ConstantBufferDebugCone : register(b10)
{
    ConeData DataCone[100];
}

cbuffer ConstantBufferDebugGrid : register(b10)
{
    row_major matrix InverseViewProj;
}

cbuffer ConstantBufferDebugIcon : register(b10)
{
    float3 IconPosition;
    float IconScale;
}

cbuffer ConstantBufferDebugArrow : register(b10)
{
    float3 ArrowPosition;
    float ArrowScaleXYZ;
    float3 ArrowDirection;
    float ArrowScaleZ;
}
