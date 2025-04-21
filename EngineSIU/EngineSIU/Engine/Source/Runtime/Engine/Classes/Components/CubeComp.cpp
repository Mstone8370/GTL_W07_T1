#include "CubeComp.h"
#include "Engine/FObjLoader.h"
#include "UObject/ObjectFactory.h"


UCubeComp::UCubeComp()
{
    SetType(StaticClass()->GetName());
    AABB.Max = { 1,1,1 };
    AABB.Min = { -1,-1,-1 };

}

void UCubeComp::InitializeComponent()
{
    Super::InitializeComponent();

    //FManagerOBJ::CreateStaticMesh("Assets/helloBlender.obj");
    //SetStaticMesh(FManagerOBJ::GetStaticMesh(L"helloBlender.obj"));
    // 
    // Begin Test
    FObjManager::CreateStaticMesh("Contents/Reference/Reference.obj");
    SetStaticMesh(FObjManager::GetStaticMesh(L"Reference.obj"));
    // End Test
}

void UCubeComp::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

}
