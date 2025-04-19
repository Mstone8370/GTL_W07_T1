
#include "WorldBillboardRenderPass.h"

#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "Components/BillboardComponent.h"

FWorldBillboardRenderPass::FWorldBillboardRenderPass()
{
    ResourceType = EResourceType::ERT_Scene;
}

void FWorldBillboardRenderPass::PrepareRenderArr()
{
    BillboardComps.Empty();
    for (const auto Component : TObjectRange<UBillboardComponent>())
    {
        if (Component->GetWorld() == GEngine->ActiveWorld && !Component->bIsEditorBillboard)
        {
            BillboardComps.Add(Component);
        }
    }
}
