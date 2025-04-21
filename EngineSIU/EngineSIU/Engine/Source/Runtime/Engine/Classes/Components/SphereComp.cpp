#include "SphereComp.h"
#include "Engine/Source/Runtime/Core/Math/JungleMath.h"
#include "UnrealEd/EditorViewportClient.h"

USphereComp::USphereComp()
{
    SetType(StaticClass()->GetName());
    AABB.Max = {1, 1, 1};
    AABB.Min = {-1, -1, -1};
}

void USphereComp::InitializeComponent()
{
    Super::InitializeComponent();
}

void USphereComp::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
}