#pragma once
#include "HAL/PlatformType.h"


enum class EViewModeIndex : uint8
{
    VMI_Lit_Gouraud,
    VMI_Lit_Lambert,
    VMI_Lit_BlinnPhong,
    VMI_Unlit,
    VMI_Wireframe,
    VMI_SceneDepth,
    VMI_WorldNormal,
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
