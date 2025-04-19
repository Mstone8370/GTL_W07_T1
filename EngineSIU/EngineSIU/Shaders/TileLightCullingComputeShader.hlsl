// 상수 정의
#define TILE_SIZE 32
#define MAX_LIGHTS_PER_TILE 2048
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32)
#define THREAD_GROUP_SIZE 8
#define NUM_SLICES 32    // 타일 내 depth를 32개의 슬라이스로 분할 (시그래프 Harada 값과 동일)

cbuffer TileLightCullSettings : register(b0)
{
    uint2 ScreenSize; // 화면 해상도
    uint2 TileSize; // 한 타일의 크기 (예: 16x16)

    float NearZ; // 카메라 near plane
    float FarZ; // 카메라 far plane

    row_major matrix View; // View 행렬
    row_major matrix Projection; // Projection 행렬
    row_major matrix InverseProjection; // Projection^-1, 뷰스페이스 복원용

    uint NumLights; // 총 라이트 수
    uint Enable25DCulling; // 1이면 2.5D 컬링 사용
}
struct Sphere
{
    float3 c; // Center point.
    float r; // Radius.
};
struct Plane
{
    float3 N; // Plane normal.
    float d; // Distance to origin.
};
struct Frustum
{
    Plane planes[4]; // left, right, top, bottom frustum planes.
};

Plane ComputePlane(float3 p0, float3 p1, float3 p2)
{
    Plane plane;

    float3 v0 = p1 - p0;
    float3 v2 = p2 - p0;

    plane.N = normalize(cross(v0, v2));

	// Compute the distance to the origin using p0.
    plane.d = dot(plane.N, p0);

    return plane;
}

// Convert clip space coordinates to view space
float4 ClipToView(float4 clip)
{
	// View space position.
    float4 view = mul(clip, InverseProjection);
	// Perspective projection.
    view = view / view.w;

    return view;
}
// Convert screen space coordinates to view space.
float4 ScreenToView(float4 screen, float2 dim_rcp)
{
	// Convert to normalized texture coordinates // Convert to clip space
    float2 texCoord = screen.xy * dim_rcp;
    float4 clip = float4(float2(texCoord.x, 1.0f - texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);
    return ClipToView(clip);
}

bool SphereInsidePlane(Sphere sphere, Plane plane)
{
    return dot(plane.N, sphere.c) - plane.d < -sphere.r;
}
bool SphereInsideFrustum(Sphere sphere, Frustum frustum, float zNear, float zFar) // this can only be used in view space
{
    bool result = true;
    result = ((sphere.c.z + sphere.r < zNear || sphere.c.z - sphere.r > zFar) ? false : result);
    result = ((SphereInsidePlane(sphere, frustum.planes[0])) ? false : result);
    result = ((SphereInsidePlane(sphere, frustum.planes[1])) ? false : result);
    result = ((SphereInsidePlane(sphere, frustum.planes[2])) ? false : result);
    result = ((SphereInsidePlane(sphere, frustum.planes[3])) ? false : result);

    return result;
}

inline uint2 unflatten2D(uint idx, uint2 dim)
{
    return uint2(idx % dim.x, idx / dim.x);
}

struct FLightGPU
{
    float3 Position;
    float Radius;
    float3 Direction; // 사용하지 않음
    uint isPointLight;
};

StructuredBuffer<FLightGPU> LightBuffer : register(t0); // CPU에서 전달된 PointLight들
Texture2D<float> gDepthTexture : register(t1);          // Depth Texture

RWStructuredBuffer<uint> TileLightMask : register(u0); // 타일별 조명 마스크
RWTexture2D<float4> DebugHeatmap : register(u3); // 디버깅용 히트맵


// Group Shared 메모리 - Depth Masking 누적에 쓰입니다~
groupshared uint tileDepthMask;

// Group(타일) 단위로 오브젝트의 minZ, maxZ 값 저장
groupshared uint groupMinZ; // float max (≈ 3.4e+38);
groupshared uint groupMaxZ;  // float min (0);
groupshared uint hitCount;

// dispatchID = groupID * [numthreads] + threadID

// no difference between [1,1,1] - 그러나 DepthMap Texturing할 때 한 픽셀의 값 읽어오려면 TILE_SIZE만큼 나눠야 효율적
[numthreads(TILE_SIZE, TILE_SIZE, 1)] 
void mainCS(uint3 groupID : SV_GroupID, uint3 dispatchID : SV_DispatchThreadID, uint3 threadID : SV_GroupThreadID)
{
    uint2 tileCoord = groupID.xy;
    uint2 pixel = tileCoord * TILE_SIZE + threadID.xy;
    
    uint2 screenTileSize = TileSize;  // TILE_SIZE
    uint2 screenSize = ScreenSize;
    
    float minZ, maxZ;
    // --- 1. 타일 내 각 픽셀의 Depth를 샘플링하고, 해당 슬라이스 인덱스의 비트를 그룹 공유 변수에 누적
    // 초기화: 그룹의 첫 번째 스레드가 tileDepthMask를 0으로 초기화
    if (threadID.x == 0 && threadID.y == 0)
    {
        tileDepthMask = 0;
        groupMinZ = 0x7f7fffff;
        groupMaxZ = 0x00000000; // 그룹공유 변수는 초기화 허용X, 쓰레기값 발생 가능
        
    }
    hitCount = 0;
    GroupMemoryBarrierWithGroupSync();
    float depthSample = 1.0f;
    float linearZ = FarZ;
    // (1) 만약 Enable25DCulling 옵션이 켜져 있다면, 해당 타일 내의 depth mask 구성
    if (Enable25DCulling != 0 && all(pixel < ScreenSize))
    {
        // 픽셀별 depth 샘플링
        depthSample = gDepthTexture[pixel];
        if (depthSample < 1.0f)
        {
            linearZ = (NearZ * FarZ) / (FarZ - depthSample * (FarZ - NearZ));
            uint linZ_uint = uint(linearZ);
            InterlockedMin(groupMinZ, linZ_uint);
            InterlockedMax(groupMaxZ, linZ_uint);
        }
    }
        
        
    GroupMemoryBarrierWithGroupSync();
    
    if (Enable25DCulling != 0 && groupMaxZ > groupMinZ)
    {
    
        minZ = float(groupMinZ);
        maxZ = float(groupMaxZ);        
    }
        
        
        //float sliceNormZ = saturate((linearZ - minZ) / max(1e-5, (maxZ - minZ)));
        //int sliceIndex = clamp((int) floor(sliceNormZ * NUM_SLICES), 0, NUM_SLICES - 1);
        //uint sliceBit = (1u << sliceIndex);
        //InterlockedOr(tileDepthMask, sliceBit);
        //GroupMemoryBarrierWithGroupSync();
    
    if (Enable25DCulling != 0 && depthSample < 1.0f)
    {
    
        float rangeZ = maxZ - minZ;
        if (rangeZ < 1e-3)
        {
            minZ -= 0.5f;
            maxZ += 0.5f;
            rangeZ = maxZ - minZ;
        }

        float sliceNormZ = saturate((linearZ - minZ) / rangeZ);
        int sliceIndex = clamp((int) floor(sliceNormZ * NUM_SLICES), 0, NUM_SLICES - 1);
        uint sliceBit = (1u << sliceIndex);
        InterlockedOr(tileDepthMask, sliceBit);
    }
    GroupMemoryBarrierWithGroupSync();
        
        // 깊이값 변환: gDepthTexture가 보통 [0,1] 범위의 비선형 값이면 선형화
        //float linearZ = (depthSample == 1.0f)
        //    ? FarZ
        //    : (NearZ * FarZ) / (FarZ - depthSample * (FarZ - NearZ));
        //float depthNormalized = saturate((linearZ - NearZ) / (FarZ - NearZ));

        //// 해당 구간의 depth slice 인덱스 계산함
        //int sliceIndex = (int) floor(depthNormalized * NUM_SLICES); 
        //sliceIndex = clamp(sliceIndex, 0, NUM_SLICES - 1);
        //uint sliceBit = 1u << sliceIndex;
        //InterlockedOr(tileDepthMask, sliceBit);
        //GroupMemoryBarrierWithGroupSync();          // 동기화 (32x32픽셀: 스레드가 각 픽셀 맡음)
    

    // 뷰 공간 프러스텀 계산
    float2 dim_rcp = 1.0 / float2(screenSize);
    float tilePixelWidth = screenTileSize.x;
    float tilePixelHeight = screenTileSize.y;

    float2 tileMin = tileCoord * screenTileSize; // GroupID 기준 tile Min, Max
    float2 tileMax = tileMin + screenTileSize;

    // 8개 코너의 clip space 점을 inverse projection하여 view space로 변환
    float3 corners[8];
    float nearZ = NearZ;
    float farZ = FarZ;

    // tile clip corner 좌표 → view 변환
    float3 viewCorners[8];
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        float2 uv = float2((i & 1) ? tileMax.x : tileMin.x, (i & 2) ? tileMax.y : tileMin.y);
        // i=0: (tileMin.x, tileMin.y) → 왼쪽 상단
        // i=1: (tileMax.x, tileMin.y) → 오른쪽 상단
        // i=2: (tileMin.x, tileMax.y) → 왼쪽 하단
        // i=3: (tileMax.x, tileMax.y) → 오른쪽 하단
        uv /= screenSize;
        uv.y = 1.0 - uv.y; // y축 반전해야 카메라 위로 들어올렸을 때 사각형은 아래로 감
        float4 clipNear = float4(uv * 2.0 - 1.0, nearZ, 1.0);
        float4 clipFar =  float4(uv * 2.0 - 1.0, farZ , 1.0);

        float4 viewNear = mul(clipNear, InverseProjection);
        float4 viewFar =  mul(clipFar , InverseProjection);
        viewCorners[i + 0] = viewNear.xyz / viewNear.w;
        viewCorners[i + 4] = viewFar.xyz / viewFar.w;
    }

    // 프러스텀 4개 면 생성
    Frustum frustum;
    frustum.planes[0] = ComputePlane(viewCorners[0], viewCorners[2], viewCorners[6]); // left
    frustum.planes[1] = ComputePlane(viewCorners[3], viewCorners[1], viewCorners[7]); // right
    frustum.planes[2] = ComputePlane(viewCorners[1], viewCorners[0], viewCorners[5]); // top
    frustum.planes[3] = ComputePlane(viewCorners[2], viewCorners[3], viewCorners[6]); // bottom

    // 타일 인덱스
    uint flatTileIndex = tileCoord.y * (ScreenSize.x / screenTileSize.x) + tileCoord.x;

    // 마스킹 초기화
    uint mask = 0;


    // 2.5D 컬링이 활성화 -> 각 라이트의 view space 깊이 범위를 구해 depth mask와 교차 검사
      
    if (threadID.x == 0 && threadID.y == 0)
    {
        [loop]
        for (uint i = 0; i < NumLights; ++i)
        {
            FLightGPU light = LightBuffer[i];
            Sphere s;
            s.c = mul(float4(light.Position, 1), View).xyz;
            s.r = light.Radius;
        
            if (tileDepthMask == 0)
                continue;
        
            bool insideFrustum = SphereInsideFrustum(s, frustum, NearZ, FarZ);
            if (insideFrustum == false)
            {
                continue;
            }


            bool depthOverlap = true; // 2.5D 컬링이 비활성화면 무조건 true
            if (Enable25DCulling != 0)
            {
            // 빛의 Bounding Sphere의 view-space z값 범위 계산
                float3 posVS = mul(float4(light.Position, 1), View).xyz;
                float s_minDepth = posVS.z - s.r;
                float s_maxDepth = posVS.z + s.r;
            // 0419 수정 : 그룹기준으로 슬라이스 계산
            
                if (s_maxDepth < minZ || s_minDepth > maxZ)
                {
                    depthOverlap = false; // 절대 겹칠 수 없음
                }
                else
                {
                // 조금이라도 겹치면 [minZ, maxZ] 범위로 0,1 saturate
                    float normMin = saturate((s_minDepth - minZ) / max(1e-5, maxZ - minZ));
                    float normMax = saturate((s_maxDepth - minZ) / max(1e-5, maxZ - minZ));
                
                    int sphereSliceMin = (int) floor(normMin * NUM_SLICES); // light 포함 X -> 내림
                    int sphereSliceMax = (int) ceil(normMax * NUM_SLICES); // light 포함 X -> 올림
                    sphereSliceMin = clamp(sphereSliceMin, 0, NUM_SLICES - 1); // 0~31 인덱스로 클램프
                    sphereSliceMax = clamp(sphereSliceMax, 0, NUM_SLICES - 1);
                    uint sphereMask = 0;
                    for (int j = sphereSliceMin; j <= sphereSliceMax; ++j)
                    {
                        sphereMask |= (1u << j);
                    }
            
            // 깊이 영역이 겹치지 않으면, 해당 라이트는 2.5D 기준에서 컬링됨
                    depthOverlap = (sphereMask & tileDepthMask) != 0;
                }
            }
        
            if (depthOverlap)
            {
                uint bucketIdx = i / 32;
                uint bitIdx = i % 32;
                InterlockedOr(TileLightMask[flatTileIndex * SHADER_ENTITY_TILE_BUCKET_COUNT + bucketIdx], 1 << bitIdx);
                InterlockedAdd(hitCount, 1);
            //hitCount++;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();
    
    // thread 0에서만 실행
    if (threadID.x == 0 && threadID.y == 0)
    {
        float4 result = float4(0, 0, 0, 0); // 기본: 아무것도 안 보임

        if (hitCount > 0) // 히트된 라이트가 있는 경우만 시각화
        {
            const float3 heatmap[] =
            {
                float3(0, 0, 0),
            float3(0, 0, 1),
            float3(0, 1, 1),
            float3(0, 1, 0),
            float3(1, 1, 0),
            float3(1, 0, 0),
            };
            const float maxHeat = 50.0f;
            float l = saturate(hitCount / maxHeat) * 5;
            float3 c1 = heatmap[floor(l)];
            float3 c2 = heatmap[ceil(l)];
            float3 color = lerp(c1, c2, frac(l));
            result = float4(color, 0.8f);
        }

    // 타일 전체에 결과 색상 출력
        for (uint i = 0; i < TILE_SIZE * TILE_SIZE; ++i)
        {
            uint2 local = unflatten2D(i, uint2(TILE_SIZE, TILE_SIZE));
            uint2 pixel = tileCoord * TILE_SIZE + local;

            if (all(pixel < screenSize))
            {
                DebugHeatmap[pixel] = result;
            }
        }
    }
    
    //// heatmap 시각화
    //const float3 heatmap[] = {
    //    float3(0, 0, 0),
    //    float3(0, 0, 1),
    //    float3(0, 1, 1),
    //    float3(0, 1, 0),
    //    float3(1, 1, 0),
    //    float3(1, 0, 0),
    //};
    //const float maxHeat = 50.0f;
    //float l = saturate(hitCount / maxHeat) * 5;
    //float3 c1 = heatmap[floor(l)];
    //float3 c2 = heatmap[ceil(l)];
    //float3 color = lerp(c1, c2, frac(l));
    //float4 result = float4(color, 0.8f);

    //// 타일 내부 모든 픽셀에 출력
    //for (uint i = 0; i < TILE_SIZE * TILE_SIZE; ++i)
    //{
    //    uint2 local = unflatten2D(i, uint2(TILE_SIZE, TILE_SIZE));
    //    // pixel = 타일 내부에서 0부터 TILE_SIZE−1까지의 오프셋
    //    // tileCoord * TILE_SIZE = 타일의 왼쪽 상단 좌표
    //    uint2 pixel = tileCoord * TILE_SIZE + local;
    //    if (all(pixel < screenSize))
    //    {
    //        DebugHeatmap[pixel] = result;
    //    }
    //}
    //for (uint i = 0; i < screenSize.x * screenSize.y; ++i)
    //{
    //    uint2 pixel = unflatten2D(i, screenSize);
    //    DebugHeatmap[pixel] = float4(heatmap[3].xyz, 1.0f);
    //}

    // SV_DispatchThreadID는 전체 화면상의 픽셀 좌표(전역 좌표)를 나타냅니다.
    
}
