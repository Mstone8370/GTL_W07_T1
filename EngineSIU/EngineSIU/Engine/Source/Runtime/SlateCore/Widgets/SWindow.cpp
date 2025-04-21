#include "SWindow.h"


SWindow::SWindow(FRect InRect)
    : Rect(InRect)
{}

void SWindow::Initialize(FRect InitRect)
{
    Rect = InitRect;
}

void SWindow::OnResize(uint32 InWidth, uint32 InHeight)
{
    Rect.Width = InWidth;
    Rect.Height = InHeight;
}

bool SWindow::IsHover(const FPoint& InPoint) 
{
    bIsHovered = (Rect.TopLeftX <= InPoint.X && InPoint.X < Rect.TopLeftX + Rect.Width) &&
                 (Rect.TopLeftY <= InPoint.Y && InPoint.Y < Rect.TopLeftY + Rect.Height);
    return bIsHovered;
}

bool SWindow::OnPressed(const FPoint& InPoint)
{
    return false;
}

bool SWindow::OnReleased() 
{
    return false;
}
