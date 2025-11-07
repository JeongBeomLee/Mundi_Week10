#include "pch.h"
#include "Widget.h"

IMPLEMENT_CLASS(UWidget)

void UWidget::Initialize()
{
}

void UWidget::Update()
{
}

void UWidget::RenderWidget()
{
}

UWidget::UWidget(const FString& InName)
	: Name(InName)
{
}
