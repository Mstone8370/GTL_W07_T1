#include "Cube.h"
#include "Components/StaticMeshComponent.h"

#include "Engine/FObjLoader.h"

#include "GameFramework/Actor.h"

ACube::ACube()
{
    // Begin Test
    //StaticMeshComponent->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/helloBlender.obj"));
    StaticMeshComponent->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/Reference/Reference.obj"));
    // End Test
}

void ACube::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    //SetActorRotation(GetActorRotation() + FRotator(0, 0, 1));

}
