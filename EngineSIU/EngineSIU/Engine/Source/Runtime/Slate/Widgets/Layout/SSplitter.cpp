#include "SSplitter.h"
#include "EngineLoop.h"

extern FEngineLoop GEngineLoop;

void SSplitter::Initialize(FRect InitRect)
{
    __super::Initialize(InitRect);
    
    if (SideLT == nullptr)
    {
        SideLT = new SWindow();
    }
    if (SideRB == nullptr)
    {
        SideRB = new SWindow();
    }
}

void SSplitter::OnResize(uint32 InWidth, uint32 InHeight)
{
    Rect.Width = static_cast<float>(InWidth);
    Rect.Height = static_cast<float>(InHeight);
}

bool SSplitter::OnPressed(const FPoint& InPoint)
{
    if (!IsHover(InPoint))
    {
        return false;
    }

    bIsSplitterPressed = IsSplitterHovered(InPoint);
    
    return bIsPressed = true;
}

bool SSplitter::OnReleased()
{
    bIsPressed = false;
    bIsSplitterPressed = false;
    
    return false;
}

bool SSplitter::IsSplitterHovered(const FPoint& InPoint) const
{
    if (SideLT && SideLT->IsHover(InPoint))
    {
        return false;
    }
    if (SideRB && SideRB->IsHover(InPoint))
    {
        return false;
    }
    return true;
}

void SSplitter::LoadConfig(const TMap<FString, FString>& Config) {}
void SSplitter::SaveConfig(TMap<FString, FString>& Config) const {}


void SSplitterH::Initialize(FRect InRect)
{
    __super::Initialize(InRect);

    UpdateChildRects();
}

void SSplitterH::ClampSplitRatio()
{
    SplitRatio = FMath::Max(SplitRatio, static_cast<float>(SplitterLimitLT) / Rect.Width);
    SplitRatio = FMath::Min(SplitRatio, (Rect.Width - static_cast<float>(SplitterLimitLT)) / Rect.Width);
}

float SSplitterH::GetSplitterLTCenter()
{
    ClampSplitRatio();
    return Rect.Width * SplitRatio;
}

void SSplitterH::LoadConfig(const TMap<FString, FString>& Config)
{
    SplitRatio = GetValueFromConfig(Config, "SplitterH.SplitRatio", 0.5f);

    UpdateChildRects();
}

void SSplitterH::SaveConfig(TMap<FString, FString>& Config) const
{
    Config["SplitterH.SplitRatio"] = std::to_string(SplitRatio);
}

void SSplitterH::OnResize(uint32 InWidth, uint32 InHeight)
{
    __super::OnResize(InWidth, InHeight);
    
    UpdateChildRects();
}

void SSplitterH::OnDrag(const FPoint& Delta)
{
    // 수평 스플리터의 경우, 좌우로 이동
    float CenterX = GetSplitterLTCenter();
    CenterX += Delta.X;

    // 픽셀 단위 이동을 위해 정수형으로 변환 후 계산
    SplitRatio = std::trunc(CenterX) / Rect.Width;
    
    UpdateChildRects();
}

void SSplitterH::UpdateChildRects()
{
    // 픽셀 단위로 계산하기 위해 정수형으로 변환
    const uint32 SplitterCenterX = static_cast<uint32>(GetSplitterLTCenter());
    
    if (SideLT)
    {
        SideLT->Initialize(FRect(
            0.0f,
            0.0f,
            static_cast<float>(SplitterCenterX - SplitterHalfThickness),
            std::trunc(Rect.Height)
        ));
    }
    if (SideRB)
    {
        const float Offset = static_cast<float>(SplitterCenterX + SplitterHalfThickness);
        
        SideRB->Initialize(FRect(
            Offset,
            0.0f,
            std::trunc(Rect.Width - Offset),
            std::trunc(Rect.Height)
        ));
    }
}

void SSplitterV::Initialize(FRect InRect)
{
    __super::Initialize(InRect);

    UpdateChildRects();
}

void SSplitterV::ClampSplitRatio()
{
    SplitRatio = FMath::Max(SplitRatio, static_cast<float>(SplitterLimitLT) / Rect.Height);
    SplitRatio = FMath::Min(SplitRatio, (Rect.Height - static_cast<float>(SplitterLimitLT)) / Rect.Height);
}

float SSplitterV::GetSplitterLTCenter()
{
    ClampSplitRatio();
    return Rect.Height * SplitRatio;
}

void SSplitterV::LoadConfig(const TMap<FString, FString>& Config)
{
    SplitRatio = GetValueFromConfig(Config, "SplitterV.SplitRatio", 0.5f);

    UpdateChildRects();
}

void SSplitterV::SaveConfig(TMap<FString, FString>& Config) const
{
    Config["SplitterV.SplitRatio"] = std::to_string(SplitRatio);
}

void SSplitterV::OnResize(uint32 InWidth, uint32 InHeight)
{
    __super::OnResize(InWidth, InHeight);
    
    UpdateChildRects();
}

void SSplitterV::OnDrag(const FPoint& Delta)
{
    float CenterY = GetSplitterLTCenter();
    CenterY += Delta.Y;

    // 픽셀 단위 이동을 위해 정수형으로 변환 후 계산
    SplitRatio = std::trunc(CenterY) / Rect.Height;
    
    UpdateChildRects();
}

void SSplitterV::UpdateChildRects()
{
    // 픽셀 단위로 계산하기 위해 정수형으로 변환
    const uint32 SplitterCenterY = static_cast<uint32>(GetSplitterLTCenter());
    
    if (SideLT)
    {
        SideLT->Initialize(FRect(
            0.0f,
            0.0f,
            std::trunc(Rect.Width),
            static_cast<float>(SplitterCenterY - SplitterHalfThickness)
        ));
    }
    if (SideRB)
    {
        const float Offset = static_cast<float>(SplitterCenterY + SplitterHalfThickness);
        
        SideRB->Initialize(FRect(
            0.0f,
            Offset,
            std::trunc(Rect.Width),
            std::trunc(Rect.Height - Offset)
        ));
    }
}
