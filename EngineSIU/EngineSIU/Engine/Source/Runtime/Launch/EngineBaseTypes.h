#pragma once
#include "HAL/PlatformType.h"


enum class EViewModeIndex : uint8
{
    /**
     * 이 enum 수정 시에는 CompositingShader.hlsl도 수정해야 합니다.
     */
    VMI_Lit_Gouraud = 0,
    VMI_Lit_Lambert,
    VMI_Lit_BlinnPhong,
    VMI_LIT_PBR,
    VMI_Unlit, // Lit 모드들은 이 위에 적어주세요.
    VMI_Wireframe,
    VMI_SceneDepth,
    VMI_WorldNormal,
    VMI_WorldTangent,
    VMI_LightHeatMap,
    VMI_MAX,
};


enum ELevelViewportType : uint8
{
    LVT_Perspective = 0,

    /** Top */
    LVT_OrthoXY = 1,
    /** Bottom */
    LVT_OrthoNegativeXY,
    /** Left */
    LVT_OrthoYZ,
    /** Right */
    LVT_OrthoNegativeYZ,
    /** Front */
    LVT_OrthoXZ,
    /** Back */
    LVT_OrthoNegativeXZ,

    LVT_MAX,
    LVT_None = 255,
};
