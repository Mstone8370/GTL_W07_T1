#include "PropertyEditorPanel.h"

#include "World/World.h"
#include "Actors/Player.h"
#include "Components/Light/LightComponent.h"
#include "Components/Light/PointLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Components/Light/AmbientLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextComponent.h"
#include "Engine/EditorEngine.h"
#include "Engine/FObjLoader.h"
#include "UnrealEd/ImGuiWidget.h"
#include "UObject/Casts.h"
#include "UObject/ObjectFactory.h"
#include "Engine/Engine.h"
#include "Components/HeightFogComponent.h"
#include "Components/ProjectileMovementComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/AssetManager.h"
#include "LevelEditor/SLevelEditor.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UObject/UObjectIterator.h"

void PropertyEditorPanel::Render()
{
    /* Pre Setup */
    float PanelWidth = (Width) * 0.2f - 6.0f;
    float PanelHeight = (Height) * 0.65f;

    float PanelPosX = (Width) * 0.8f + 5.0f;
    float PanelPosY = (Height) * 0.3f + 15.0f;

    ImVec2 MinSize(140, 370);
    ImVec2 MaxSize(FLT_MAX, 900);

    /* Min, Max Size */
    ImGui::SetNextWindowSizeConstraints(MinSize, MaxSize);

    /* Panel Position */
    ImGui::SetNextWindowPos(ImVec2(PanelPosX, PanelPosY), ImGuiCond_Always);

    /* Panel Size */
    ImGui::SetNextWindowSize(ImVec2(PanelWidth, PanelHeight), ImGuiCond_Always);

    /* Panel Flags */
    ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    /* Render Start */
    ImGui::Begin("Detail", nullptr, PanelFlags);

    RenderDetailPanel();
    
    ImGui::End();
}

void PropertyEditorPanel::RenderDetailPanel()
{
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (!Engine)
    {
        return;
    }
    
    AEditorPlayer* Player = Engine->GetEditorPlayer();
    AActor* PickedActor = Engine->GetSelectedActor();
    if (!PickedActor)
    {
        return;
    }
    
    ImGui::SetItemDefaultFocus();
    // TreeNode 배경색을 변경 (기본 상태)
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        Location = PickedActor->GetActorLocation();
        Rotation = PickedActor->GetActorRotation();
        Scale = PickedActor->GetActorScale();

        FImGuiWidget::DrawVec3Control("Location", Location, 0, 85);
        ImGui::Spacing();

        FImGuiWidget::DrawRot3Control("Rotation", Rotation, 0, 85);
        ImGui::Spacing();

        FImGuiWidget::DrawVec3Control("Scale", Scale, 0, 85);
        ImGui::Spacing();

        PickedActor->SetActorLocation(Location);
        PickedActor->SetActorRotation(Rotation);
        PickedActor->SetActorScale(Scale);

        std::string coordiButtonLabel;
        if (Player->GetCoordMode() == ECoordMode::CDM_WORLD)
            coordiButtonLabel = "World";
        else if (Player->GetCoordMode() == ECoordMode::CDM_LOCAL)
            coordiButtonLabel = "Local";

        if (ImGui::Button(coordiButtonLabel.c_str(), ImVec2(ImGui::GetWindowContentRegionMax().x * 0.9f, 32)))
        {
            Player->AddCoordiMode();
        }
        ImGui::TreePop(); // 트리 닫기
    }
    ImGui::PopStyleColor();

    if (ImGui::Button("Duplicate"))
    {
        AActor* NewActor = Engine->ActiveWorld->DuplicateActor(Engine->GetSelectedActor());
        Engine->SelectActor(NewActor);
    }

    //// TODO: 추후에 RTTI를 이용해서 프로퍼티 출력하기
    if (UPointLightComponent* PointLightComp = PickedActor->GetComponentByClass<UPointLightComponent>())
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        if (ImGui::TreeNodeEx("PointLight Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawColorProperty(
                "Light Color",
                [&]() { return PointLightComp->GetLightColor(); },
                [&](FLinearColor c) { PointLightComp->SetLightColor(c); }
            );

            float Intensity = PointLightComp->GetIntensity();
            if (ImGui::SliderFloat("Intensity", &Intensity, 0.0f, 160.0f, "%.1f"))
            {
                PointLightComp->SetIntensity(Intensity);
            }

            float Radius = PointLightComp->GetRadius();
            if (ImGui::SliderFloat("Radius", &Radius, 0.01f, 200.f, "%.1f"))
            {
                PointLightComp->SetRadius(Radius);
            }

            bool bCastShadow = PointLightComp->IsCastShadows();
            if (ImGui::Checkbox("Cast Shadow", &bCastShadow))
            {
                PointLightComp->SetCastShadows(bCastShadow);
            }
            
            FString Dir[6]= {"X+","X-","Z+","Z-", "Y+","Y-"};
            for (int Face = 0; Face < 6; ++Face)
            {
                ImGui::PushID(Face);

                ImGui::Text("Face %s", *Dir[Face]);
                // UV 좌표를 뒤집어(depth 텍스처 상 Y축 반전) 제대로 보이게
                ImGui::Image(
                    reinterpret_cast<ImTextureID>(PointLightComp->faceSRVs[Face]),
                    ImVec2(512,512)
                );   // UV1
                ImGui::PopID();
                char label[64];
                sprintf_s(label, "Override face %d camera with light's perspective", Face);
                
                // faceIn
                FEditorViewportClient* ActiveViewport = GEngineLoop.GetLevelEditor()->GetActiveViewportClient().get();
                if (ImGui::Button(label))
                {
                    if (Face != 0 and Face != 1)
                    {
                        ActiveViewport->PerspectiveCamera.SetRotation(PointLightComp->dirs[Face] * 89.0f);
                    }
                    else if (Face == 0)
                    {
                        ActiveViewport->PerspectiveCamera.SetRotation(FVector(0.0f, 0.0f, 0.0f));
                    }
                    else if (Face == 1)
                    {
                        ActiveViewport->PerspectiveCamera.SetRotation(FVector(0.0f, 0.0f, 180.0f));
                    }
                    
                    ActiveViewport->PerspectiveCamera.SetLocation(PointLightComp->GetWorldLocation() +  ActiveViewport->PerspectiveCamera.GetForwardVector()); 
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopStyleColor();
    }

    if (USpotLightComponent* SpotLightComp = PickedActor->GetComponentByClass<USpotLightComponent>())
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        if (ImGui::TreeNodeEx("SpotLight Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawColorProperty("Light Color",
                [&]() { return SpotLightComp->GetLightColor(); },
                [&](FLinearColor c) { SpotLightComp->SetLightColor(c); });

            float Intensity = SpotLightComp->GetIntensity();
            if (ImGui::SliderFloat("Intensity", &Intensity, 0.0f, 160.0f, "%.1f"))
                SpotLightComp->SetIntensity(Intensity);

            float Radius = SpotLightComp->GetRadius();
            if (ImGui::SliderFloat("Radius", &Radius, 0.01f, 200.f, "%.1f")) {
                SpotLightComp->SetRadius(Radius);
            }

            LightDirection = SpotLightComp->GetDirection();
            FImGuiWidget::DrawVec3Control("Direction", LightDirection, 0, 85);
            
            float InnerDegree = SpotLightComp->GetInnerDegree();
            if (ImGui::SliderFloat("InnerDegree", &InnerDegree, 0.01f, 80.f, "%.1f")) {
                SpotLightComp->SetInnerDegree(InnerDegree);
            }

            float OuterDegree = SpotLightComp->GetOuterDegree();
            if (ImGui::SliderFloat("OuterDegree", &OuterDegree, 0.01f, 80.f, "%.1f")) {
                SpotLightComp->SetOuterDegree(OuterDegree);
            }
            bool bCastShadow = SpotLightComp->IsCastShadows();
            
            if (ImGui::Checkbox("Cast Shadow", &bCastShadow)) {
                SpotLightComp->SetCastShadows(bCastShadow);
            }
            
            const auto& SM = SpotLightComp->GetShadowDepthMap();
            ID3D11ShaderResourceView* depthSRV = SM.SRV;
            ID3D11Texture2D* tex = SM.Texture2D;
            if (depthSRV && tex)
            {
                D3D11_TEXTURE2D_DESC desc = {};
                tex->GetDesc(&desc);
                float w = (float)desc.Width;
                float h = (float)desc.Height;

                ImGui::Separator();
                ImGui::Text("Shadow Depth Map:");
                ImGui::Image(
                    (ImTextureID)depthSRV,
                    ImVec2(200, 200),
                    ImVec2(0, 1),
                    ImVec2(1, 0)
                );
            }
            // faceIn
            FEditorViewportClient* ActiveViewport = GEngineLoop.GetLevelEditor()->GetActiveViewportClient().get();
            if (ImGui::Button("Override camera with light's perspective"))
            {
                FRotator ViewRotator = SpotLightComp->GetWorldRotation();
                FVector ForwardVector = ViewRotator.ToVector();
                ActiveViewport->PerspectiveCamera.SetRotation(FVector(ViewRotator.Roll, ViewRotator.Pitch * -1.f, ViewRotator.Yaw));
                ActiveViewport->PerspectiveCamera.SetLocation(SpotLightComp->GetWorldLocation() + ForwardVector * 1.5f); 
            }
            ImGui::TreePop();
        }

        ImGui::PopStyleColor();
    }

    if (UDirectionalLightComponent* DirectionalLightComp = PickedActor->GetComponentByClass<UDirectionalLightComponent>())
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        if (ImGui::TreeNodeEx("DirectionalLight Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawColorProperty("Light Color",
                [&]() { return DirectionalLightComp->GetLightColor(); },
                [&](FLinearColor c) { DirectionalLightComp->SetLightColor(c); });

            float Intensity = DirectionalLightComp->GetIntensity();
            if (ImGui::SliderFloat("Intensity", &Intensity, 0.0f, 150.0f, "%.1f"))
                DirectionalLightComp->SetIntensity(Intensity);

            LightDirection = DirectionalLightComp->GetDirection();
            FImGuiWidget::DrawVec3Control("Direction", LightDirection, 0, 85);

            bool bCastShadow = DirectionalLightComp->IsCastShadows();
            
            if (ImGui::Checkbox("Cast Shadow", &bCastShadow)) {
                DirectionalLightComp->SetCastShadows(bCastShadow);
            }
            
            ImGui::Image((ImTextureID)DirectionalLightComp->GetShadowDepthMap().SRV, ImVec2(256, 256));
            ImGui::TreePop();
        }

        ImGui::PopStyleColor();
    }

    if (UAmbientLightComponent* AmbientLightComp = PickedActor->GetComponentByClass<UAmbientLightComponent>())
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        if (ImGui::TreeNodeEx("AmbientLight Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawColorProperty("Light Color",
                [&]() { return AmbientLightComp->GetLightColor(); },
                [&](FLinearColor c) { AmbientLightComp->SetLightColor(c); });
            ImGui::TreePop();
        }

        ImGui::PopStyleColor();
    }
    
    if (UProjectileMovementComponent* ProjectileComp = PickedActor->GetComponentByClass<UProjectileMovementComponent>())
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        if (ImGui::TreeNodeEx("Projectile Movement Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            float InitialSpeed = ProjectileComp->GetInitialSpeed();
            if (ImGui::InputFloat("InitialSpeed", &InitialSpeed, 0.f, 10000.0f, "%.1f"))
                ProjectileComp->SetInitialSpeed(InitialSpeed);

            float MaxSpeed = ProjectileComp->GetMaxSpeed();
            if (ImGui::InputFloat("MaxSpeed", &MaxSpeed, 0.f, 10000.0f, "%.1f"))
                ProjectileComp->SetMaxSpeed(MaxSpeed);

            float Gravity = ProjectileComp->GetGravity();
            if (ImGui::InputFloat("Gravity", &Gravity, 0.f, 10000.f, "%.1f"))
                ProjectileComp->SetGravity(Gravity); 
            
            float ProjectileLifetime = ProjectileComp->GetLifetime();
            if (ImGui::InputFloat("Lifetime", &ProjectileLifetime, 0.f, 10000.f, "%.1f"))
                ProjectileComp->SetLifetime(ProjectileLifetime);

            FVector currentVelocity = ProjectileComp->GetVelocity();

            float velocity[3] = { currentVelocity.X, currentVelocity.Y, currentVelocity.Z };

            if (ImGui::InputFloat3("Velocity", velocity, "%.1f")) {
                ProjectileComp->SetVelocity(FVector(velocity[0], velocity[1], velocity[2]));
            }
            
            ImGui::TreePop();
        }

        ImGui::PopStyleColor();
    }
    
    if (UTextComponent* TextComp = Cast<UTextComponent>(PickedActor->GetRootComponent()))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        if (ImGui::TreeNodeEx("Text Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
        {
            if (TextComp) {
                TextComp->SetTexture(L"Assets/Texture/font.png");
                TextComp->SetRowColumnCount(106, 106);
                FWString wText = TextComp->GetText();
                int len = WideCharToMultiByte(CP_UTF8, 0, wText.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string u8Text(len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wText.c_str(), -1, u8Text.data(), len, nullptr, nullptr);

                static char buf[256];
                strcpy_s(buf, u8Text.c_str());

                ImGui::Text("Text: ", buf);
                ImGui::SameLine();
                ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
                if (ImGui::InputText("##Text", buf, 256, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    TextComp->ClearText();
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
                    FWString newWText(wlen, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, buf, -1, newWText.data(), wlen);
                    TextComp->SetText(newWText.c_str());
                }
                ImGui::PopItemFlag();
            }
            ImGui::TreePop();
        }
        ImGui::PopStyleColor();
    }

    if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PickedActor->GetRootComponent()))
    {
        RenderForStaticMesh(StaticMeshComponent);
        RenderForMaterial(StaticMeshComponent);
    }

    if (UHeightFogComponent* FogComponent = Cast<UHeightFogComponent>(PickedActor->GetRootComponent()))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        if (ImGui::TreeNodeEx("Exponential Height Fog", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
        {
            FLinearColor currColor = FogComponent->GetFogColor();

            float r = currColor.R;
            float g = currColor.G;
            float b = currColor.B;
            float a = currColor.A;
            float h, s, v;
            float lightColor[4] = { r, g, b, a };

            // Fog Color
            if (ImGui::ColorPicker4("##Fog Color", lightColor,
                ImGuiColorEditFlags_DisplayRGB |
                ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_Float))
            {
                r = lightColor[0];
                g = lightColor[1];
                b = lightColor[2];
                a = lightColor[3];
                FogComponent->SetFogColor(FLinearColor(r, g, b, a));
            }
            RGBToHSV(r, g, b, h, s, v);
            // RGB/HSV
            bool changedRGB = false;
            bool changedHSV = false;

            // RGB
            ImGui::PushItemWidth(50.0f);
            if (ImGui::DragFloat("R##R", &r, 0.001f, 0.f, 1.f)) changedRGB = true;
            ImGui::SameLine();
            if (ImGui::DragFloat("G##G", &g, 0.001f, 0.f, 1.f)) changedRGB = true;
            ImGui::SameLine();
            if (ImGui::DragFloat("B##B", &b, 0.001f, 0.f, 1.f)) changedRGB = true;
            ImGui::Spacing();

            // HSV
            if (ImGui::DragFloat("H##H", &h, 0.1f, 0.f, 360)) changedHSV = true;
            ImGui::SameLine();
            if (ImGui::DragFloat("S##S", &s, 0.001f, 0.f, 1)) changedHSV = true;
            ImGui::SameLine();
            if (ImGui::DragFloat("V##V", &v, 0.001f, 0.f, 1)) changedHSV = true;
            ImGui::PopItemWidth();
            ImGui::Spacing();

            if (changedRGB && !changedHSV)
            {
                // RGB -> HSV
                RGBToHSV(r, g, b, h, s, v);
                FogComponent->SetFogColor(FLinearColor(r, g, b, a));
            }
            else if (changedHSV && !changedRGB)
            {
                // HSV -> RGB
                HSVToRGB(h, s, v, r, g, b);
                FogComponent->SetFogColor(FLinearColor(r, g, b, a));
            }

            float FogDensity = FogComponent->GetFogDensity();
            if (ImGui::SliderFloat("Density", &FogDensity, 0.00f, 3.0f))
            {
                FogComponent->SetFogDensity(FogDensity);
            }

            float FogDistanceWeight = FogComponent->GetFogDistanceWeight();
            if (ImGui::SliderFloat("Distance Weight", &FogDistanceWeight, 0.00f, 3.0f))
            {
                FogComponent->SetFogDistanceWeight(FogDistanceWeight);
            }

            float FogHeightFallOff = FogComponent->GetFogHeightFalloff();
            if (ImGui::SliderFloat("Height Fall Off", &FogHeightFallOff, 0.001f, 0.15f))
            {
                FogComponent->SetFogHeightFalloff(FogHeightFallOff);
            }

            float FogStartDistance = FogComponent->GetStartDistance();
            if (ImGui::SliderFloat("Start Distance", &FogStartDistance, 0.00f, 50.0f))
            {
                FogComponent->SetStartDistance(FogStartDistance);
            }

            float FogEndtDistance = FogComponent->GetEndDistance();
            if (ImGui::SliderFloat("End Distance", &FogEndtDistance, 0.00f, 50.0f))
            {
                FogComponent->SetEndDistance(FogEndtDistance);
            }

            ImGui::TreePop();
        }
        ImGui::PopStyleColor();
    }
}

void PropertyEditorPanel::RGBToHSV(float r, float g, float b, float& h, float& s, float& v) const
{
    float mx = FMath::Max(r, FMath::Max(g, b));
    float mn = FMath::Min(r, FMath::Min(g, b));
    float delta = mx - mn;

    v = mx;

    if (mx == 0.0f) {
        s = 0.0f;
        h = 0.0f;
        return;
    }
    else {
        s = delta / mx;
    }

    if (delta < 1e-6) {
        h = 0.0f;
    }
    else {
        if (r >= mx) {
            h = (g - b) / delta;
        }
        else if (g >= mx) {
            h = 2.0f + (b - r) / delta;
        }
        else {
            h = 4.0f + (r - g) / delta;
        }
        h *= 60.0f;
        if (h < 0.0f) {
            h += 360.0f;
        }
    }
}

void PropertyEditorPanel::HSVToRGB(float h, float s, float v, float& r, float& g, float& b) const
{
    // h: 0~360, s:0~1, v:0~1
    float c = v * s;
    float hp = h / 60.0f;             // 0~6 구간
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m = v - c;

    if (hp < 1.0f) { r = c;  g = x;  b = 0.0f; }
    else if (hp < 2.0f) { r = x;  g = c;  b = 0.0f; }
    else if (hp < 3.0f) { r = 0.0f; g = c;  b = x; }
    else if (hp < 4.0f) { r = 0.0f; g = x;  b = c; }
    else if (hp < 5.0f) { r = x;  g = 0.0f; b = c; }
    else { r = c;  g = 0.0f; b = x; }

    r += m;  g += m;  b += m;
}

void PropertyEditorPanel::RenderForStaticMesh(UStaticMeshComponent* StaticMeshComp) const
{
    if (StaticMeshComp->GetStaticMesh() == nullptr)
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Static Mesh", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        ImGui::Text("StaticMesh");
        ImGui::SameLine();

        FString PreviewName = StaticMeshComp->GetStaticMesh()->GetRenderData()->DisplayName;
        const TMap<FName, FAssetInfo> Assets = UAssetManager::Get().GetAssetRegistry();

        if (ImGui::BeginCombo("##StaticMesh", GetData(PreviewName), ImGuiComboFlags_None))
        {
            for (const auto& Asset : Assets)
            {
                if (ImGui::Selectable(GetData(Asset.Value.AssetName.ToString()), false))
                {
                    FString MeshName = Asset.Value.PackagePath.ToString() + "/" + Asset.Value.AssetName.ToString();
                    UStaticMesh* StaticMesh = FObjManager::GetStaticMesh(MeshName.ToWideString());
                    if (StaticMesh)
                    {
                        StaticMeshComp->SetStaticMesh(StaticMesh);
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TreePop();
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        ImGui::Text("Add");
        ImGui::SameLine();

        TArray<UClass*> CompClasses;
        GetChildOfClass(UActorComponent::StaticClass(), CompClasses);

        if (ImGui::BeginCombo("##AddComponent", "Components", ImGuiComboFlags_None))
        {
            for (UClass* Class : CompClasses)
            {
                if (ImGui::Selectable(GetData(Class->GetName()), false))
                {
                    USceneComponent* NewComp = Cast<USceneComponent>(StaticMeshComp->GetOwner()->AddComponent(Class));
                    if (NewComp)
                    {
                        NewComp->SetupAttachment(StaticMeshComp);
                    }
                    // 추후 Engine으로부터 SelectedComponent 받아서 선택된 Comp 아래로 붙일 수있으면 붙이기.
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}


void PropertyEditorPanel::RenderForMaterial(UStaticMeshComponent* StaticMeshComp)
{
    if (StaticMeshComp->GetStaticMesh() == nullptr)
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        for (uint32 i = 0; i < StaticMeshComp->GetNumMaterials(); ++i)
        {
            if (ImGui::Selectable(GetData(StaticMeshComp->GetMaterialSlotNames()[i].ToString()), false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    std::cout << GetData(StaticMeshComp->GetMaterialSlotNames()[i].ToString()) << std::endl;
                    SelectedMaterialIndex = i;
                    SelectedStaticMeshComp = StaticMeshComp;
                }
            }
        }

        if (ImGui::Button("    +    "))
        {
            bIsCreateMaterial = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("SubMeshes", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        auto subsets = StaticMeshComp->GetStaticMesh()->GetRenderData()->MaterialSubsets;
        for (uint32 i = 0; i < subsets.Num(); ++i)
        {
            std::string temp = "subset " + std::to_string(i);
            if (ImGui::Selectable(temp.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    StaticMeshComp->SetselectedSubMeshIndex(i);
                    SelectedStaticMeshComp = StaticMeshComp;
                }
            }
        }
        std::string temp = "clear subset";
        if (ImGui::Selectable(temp.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
        {
            if (ImGui::IsMouseDoubleClicked(0))
                StaticMeshComp->SetselectedSubMeshIndex(-1);
        }

        ImGui::TreePop();
    }

    ImGui::PopStyleColor();

    if (SelectedMaterialIndex != -1)
    {
        RenderMaterialView(SelectedStaticMeshComp->GetMaterial(SelectedMaterialIndex));
    }
    if (bIsCreateMaterial)
    {
        RenderCreateMaterialView();
    }
}

void PropertyEditorPanel::RenderMaterialView(UMaterial* Material)
{
    ImGui::SetNextWindowSize(ImVec2(380, 400), ImGuiCond_Once);
    ImGui::Begin("Material Viewer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);

    static ImGuiSelectableFlags BaseFlag = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_None | ImGuiColorEditFlags_NoAlpha;

    FVector MatDiffuseColor = Material->GetMaterialInfo().DiffuseColor;
    FVector MatSpecularColor = Material->GetMaterialInfo().SpecularColor;
    FVector MatAmbientColor = Material->GetMaterialInfo().AmbientColor;
    FVector MatEmissiveColor = Material->GetMaterialInfo().EmissiveColor;

    float dr = MatDiffuseColor.X;
    float dg = MatDiffuseColor.Y;
    float db = MatDiffuseColor.Z;
    float da = 1.0f;
    float DiffuseColorPick[4] = { dr, dg, db, da };

    ImGui::Text("Material Name |");
    ImGui::SameLine();
    ImGui::Text(*Material->GetMaterialInfo().MaterialName);
    ImGui::Separator();

    ImGui::Text("  Diffuse Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Diffuse##Color", (float*)&DiffuseColorPick, BaseFlag))
    {
        FVector NewColor = { DiffuseColorPick[0], DiffuseColorPick[1], DiffuseColorPick[2] };
        Material->SetDiffuse(NewColor);
    }

    float sr = MatSpecularColor.X;
    float sg = MatSpecularColor.Y;
    float sb = MatSpecularColor.Z;
    float sa = 1.0f;
    float SpecularColorPick[4] = { sr, sg, sb, sa };

    ImGui::Text("Specular Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Specular##Color", (float*)&SpecularColorPick, BaseFlag))
    {
        FVector NewColor = { SpecularColorPick[0], SpecularColorPick[1], SpecularColorPick[2] };
        Material->SetSpecular(NewColor);
    }
    
    float ar = MatAmbientColor.X;
    float ag = MatAmbientColor.Y;
    float ab = MatAmbientColor.Z;
    float aa = 1.0f;
    float AmbientColorPick[4] = { ar, ag, ab, aa };

    ImGui::Text("Ambient Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Ambient##Color", (float*)&AmbientColorPick, BaseFlag))
    {
        FVector NewColor = { AmbientColorPick[0], AmbientColorPick[1], AmbientColorPick[2] };
        Material->SetAmbient(NewColor);
    }

    float er = MatEmissiveColor.X;
    float eg = MatEmissiveColor.Y;
    float eb = MatEmissiveColor.Z;
    float ea = 1.0f;
    float EmissiveColorPick[4] = { er, eg, eb, ea };

    ImGui::Text("Emissive Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Emissive##Color", (float*)&EmissiveColorPick, BaseFlag))
    {
        FVector NewColor = { EmissiveColorPick[0], EmissiveColorPick[1], EmissiveColorPick[2] };
        Material->SetEmissive(NewColor);
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Choose Material");
    ImGui::Spacing();

    ImGui::Text("Material Slot Name |");
    ImGui::SameLine();
    ImGui::Text(GetData(SelectedStaticMeshComp->GetMaterialSlotNames()[SelectedMaterialIndex].ToString()));

    ImGui::Text("Override Material |");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    // 메테리얼 이름 목록을 const char* 배열로 변환
    std::vector<const char*> materialChars;
    for (const auto& material : FObjManager::GetMaterials())
    {
        materialChars.push_back(*material.Value->GetMaterialInfo().MaterialName);
    }

    //// 드롭다운 표시 (currentMaterialIndex가 범위를 벗어나지 않도록 확인)
    //if (currentMaterialIndex >= FManagerOBJ::GetMaterialNum())
    //    currentMaterialIndex = 0;

    if (ImGui::Combo("##MaterialDropdown", &CurMaterialIndex, materialChars.data(), FObjManager::GetMaterialNum()))
    {
        UMaterial* material = FObjManager::GetMaterial(materialChars[CurMaterialIndex]);
        SelectedStaticMeshComp->SetMaterial(SelectedMaterialIndex, material);
    }

    if (ImGui::Button("Close"))
    {
        SelectedMaterialIndex = -1;
        SelectedStaticMeshComp = nullptr;
    }

    ImGui::End();
}

void PropertyEditorPanel::RenderCreateMaterialView()
{
    ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_Once);
    ImGui::Begin("Create Material Viewer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);

    static ImGuiSelectableFlags BaseFlag = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_None | ImGuiColorEditFlags_NoAlpha;

    ImGui::Text("New Name");
    ImGui::SameLine();
    static char materialName[256] = "New Material";
    // 기본 텍스트 입력 필드
    ImGui::SetNextItemWidth(128);
    if (ImGui::InputText("##NewName", materialName, IM_ARRAYSIZE(materialName)))
    {
        TempMaterialInfo.MaterialName = materialName;
    }

    FVector MatDiffuseColor = TempMaterialInfo.DiffuseColor;
    FVector MatSpecularColor = TempMaterialInfo.SpecularColor;
    FVector MatAmbientColor = TempMaterialInfo.AmbientColor;
    FVector MatEmissiveColor = TempMaterialInfo.EmissiveColor;

    float dr = MatDiffuseColor.X;
    float dg = MatDiffuseColor.Y;
    float db = MatDiffuseColor.Z;
    float da = 1.0f;
    float DiffuseColorPick[4] = { dr, dg, db, da };

    ImGui::Text("Set Property");
    ImGui::Indent();

    ImGui::Text("  Diffuse Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Diffuse##Color", (float*)&DiffuseColorPick, BaseFlag))
    {
        FVector NewColor = { DiffuseColorPick[0], DiffuseColorPick[1], DiffuseColorPick[2] };
        TempMaterialInfo.DiffuseColor = NewColor;
    }

    float sr = MatSpecularColor.X;
    float sg = MatSpecularColor.Y;
    float sb = MatSpecularColor.Z;
    float sa = 1.0f;
    float SpecularColorPick[4] = { sr, sg, sb, sa };

    ImGui::Text("Specular Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Specular##Color", (float*)&SpecularColorPick, BaseFlag))
    {
        FVector NewColor = { SpecularColorPick[0], SpecularColorPick[1], SpecularColorPick[2] };
        TempMaterialInfo.SpecularColor = NewColor;
    }

    float ar = MatAmbientColor.X;
    float ag = MatAmbientColor.Y;
    float ab = MatAmbientColor.Z;
    float aa = 1.0f;
    float AmbientColorPick[4] = { ar, ag, ab, aa };

    ImGui::Text("Ambient Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Ambient##Color", (float*)&AmbientColorPick, BaseFlag))
    {
        FVector NewColor = { AmbientColorPick[0], AmbientColorPick[1], AmbientColorPick[2] };
        TempMaterialInfo.AmbientColor = NewColor;
    }

    float er = MatEmissiveColor.X;
    float eg = MatEmissiveColor.Y;
    float eb = MatEmissiveColor.Z;
    float ea = 1.0f;
    float EmissiveColorPick[4] = { er, eg, eb, ea };

    ImGui::Text("Emissive Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Emissive##Color", (float*)&EmissiveColorPick, BaseFlag))
    {
        FVector NewColor = { EmissiveColorPick[0], EmissiveColorPick[1], EmissiveColorPick[2] };
        TempMaterialInfo.EmissiveColor = NewColor;
    }
    ImGui::Unindent();

    ImGui::NewLine();
    if (ImGui::Button("Create Material"))
    {
        FObjManager::CreateMaterial(TempMaterialInfo);
    }

    ImGui::NewLine();
    if (ImGui::Button("Close"))
    {
        bIsCreateMaterial = false;
    }

    ImGui::End();
}

void PropertyEditorPanel::OnResize(HWND hWnd)
{
    RECT ClientRect;
    GetClientRect(hWnd, &ClientRect);
    Width = ClientRect.right - ClientRect.left;
    Height = ClientRect.bottom - ClientRect.top;
}
