#include "FireballActor.h"
#include "Engine/FObjLoader.h"

#include "Components/Light/PointLightComponent.h"

#include "Components/ProjectileMovementComponent.h"

#include "Components/SphereComp.h"

AFireballActor::AFireballActor()
{
    FObjManager::CreateStaticMesh("Contents/Sphere.obj");


    SphereComp = AddComponent<USphereComp>();
    
    SphereComp->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/Sphere.obj"));
  
    PointLightComponent = AddComponent<UPointLightComponent>();
    

    PointLightComponent->SetAttenuation(0.5f);
    
    PointLightComponent->SetSpecularColor(FLinearColor::Red);
    
    PointLightComponent->SetDiffuseColor(FLinearColor::Red);
    
    ProjectileMovementComponent = AddComponent<UProjectileMovementComponent>();
    PointLightComponent->AttachToComponent(RootComponent);

    ProjectileMovementComponent->SetGravity(0);
    ProjectileMovementComponent->SetVelocity(FVector(100, 0, 0));
    ProjectileMovementComponent->SetInitialSpeed(100);
    ProjectileMovementComponent->SetMaxSpeed(100);
    ProjectileMovementComponent->SetLifetime(10);
}

AFireballActor::~AFireballActor()
{
}

void AFireballActor::BeginPlay()
{
}
