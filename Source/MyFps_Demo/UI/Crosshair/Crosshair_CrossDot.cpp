#include "Crosshair_CrossDot.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"

void UCrosshair_CrossDot::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogTemp, Warning, TEXT("Crosshair_CrossDot::NativeConstruct"));
	UE_LOG(LogTemp, Warning, TEXT("  Image_Top    = %s"), Image_Top ? TEXT("OK") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("  Image_Bottom = %s"), Image_Bottom ? TEXT("OK") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("  Image_Left   = %s"), Image_Left ? TEXT("OK") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("  Image_Right  = %s"), Image_Right ? TEXT("OK") : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("  Image_Dot    = %s"), Image_Dot ? TEXT("OK") : TEXT("NULL"));

	ApplyVisualSettings();
}

void UCrosshair_CrossDot::OnSpreadUpdated_Implementation(float Spread)
{
	ApplySpreadToLines(Spread * MaxSpreadPixels);
}

void UCrosshair_CrossDot::OnVisibilityChanged_Implementation(bool bVisible)
{
	SetAllLinesVisibility(bVisible);
}

FCrosshairSettings UCrosshair_CrossDot::GatherSettings_Implementation() const
{
	FCrosshairSettings Settings;

	Settings.FloatParams.Add(TEXT("LineLength"), LineLength);
	Settings.FloatParams.Add(TEXT("LineThickness"), LineThickness);
	Settings.FloatParams.Add(TEXT("BaseGap"), BaseGap);
	Settings.FloatParams.Add(TEXT("DotSize"), DotSize);
	Settings.FloatParams.Add(TEXT("MaxSpreadPixels"), MaxSpreadPixels);
	Settings.FloatParams.Add(TEXT("CrosshairRotation"), CrosshairRotation);

	Settings.ColorParams.Add(TEXT("CrosshairColor"), CrosshairColor);

	return Settings;
}

void UCrosshair_CrossDot::ApplySettings_Implementation(const FCrosshairSettings& Settings)
{
	if (const float* V = Settings.FloatParams.Find(TEXT("LineLength"))) { LineLength = *V; }
	if (const float* V = Settings.FloatParams.Find(TEXT("LineThickness"))) { LineThickness = *V; }
	if (const float* V = Settings.FloatParams.Find(TEXT("BaseGap"))) { BaseGap = *V; }
	if (const float* V = Settings.FloatParams.Find(TEXT("DotSize"))) { DotSize = *V; }
	if (const float* V = Settings.FloatParams.Find(TEXT("MaxSpreadPixels"))) { MaxSpreadPixels = *V; }
	if (const float* V = Settings.FloatParams.Find(TEXT("CrosshairRotation"))) { CrosshairRotation = *V; }

	if (const FLinearColor* C = Settings.ColorParams.Find(TEXT("CrosshairColor"))) { CrosshairColor = *C; }

	ApplyVisualSettings();
}

void UCrosshair_CrossDot::ApplyVisualSettings()
{
	if (Image_Top)
	{
		Image_Top->SetColorAndOpacity(CrosshairColor);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Top->Slot))
		{
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetSize(FVector2D(LineLength, LineThickness));
		}
	}
	if (Image_Bottom)
	{
		Image_Bottom->SetColorAndOpacity(CrosshairColor);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Bottom->Slot))
		{
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetSize(FVector2D(LineLength, LineThickness));
		}
	}
	if (Image_Left)
	{
		Image_Left->SetColorAndOpacity(CrosshairColor);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Left->Slot))
		{
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetSize(FVector2D(LineThickness, LineLength));
		}
	}
	if (Image_Right)
	{
		Image_Right->SetColorAndOpacity(CrosshairColor);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Right->Slot))
		{
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetSize(FVector2D(LineThickness, LineLength));
		}
	}
	if (Image_Dot)
	{
		Image_Dot->SetColorAndOpacity(CrosshairColor);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Dot->Slot))
		{
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetSize(FVector2D(DotSize, DotSize));
		}
	}

	if (Image_Top)    { Image_Top->SetRenderTransformAngle(CrosshairRotation); }
	if (Image_Bottom) { Image_Bottom->SetRenderTransformAngle(CrosshairRotation); }
	if (Image_Left)   { Image_Left->SetRenderTransformAngle(CrosshairRotation); }
	if (Image_Right)  { Image_Right->SetRenderTransformAngle(CrosshairRotation); }
}

void UCrosshair_CrossDot::ApplySpreadToLines(float Offset)
{
	const float Gap = BaseGap + Offset;

	if (Image_Top)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Top->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(0.0f, -Gap));
		}
	}
	if (Image_Bottom)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Bottom->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(0.0f, Gap));
		}
	}
	if (Image_Left)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Left->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(-Gap, 0.0f));
		}
	}
	if (Image_Right)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Right->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(Gap, 0.0f));
		}
	}
	if (Image_Dot)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_Dot->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(0.0f, 0.0f));
		}
	}
}

void UCrosshair_CrossDot::SetAllLinesVisibility(bool bVisible)
{
	const ESlateVisibility Vis = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	for (UImage* Img : { Image_Top.Get(), Image_Bottom.Get(), Image_Left.Get(), Image_Right.Get(), Image_Dot.Get() })
	{
		if (Img)
		{
			Img->SetVisibility(Vis);
		}
	}
}

void UCrosshair_CrossDot::SetLineLength(float InLength)
{
	LineLength = FMath::Max(InLength, 0.0f);
	ApplyVisualSettings();
}

void UCrosshair_CrossDot::SetLineThickness(float InThickness)
{
	LineThickness = FMath::Max(InThickness, 0.0f);
	ApplyVisualSettings();
}

void UCrosshair_CrossDot::SetBaseGap(float InGap)
{
	BaseGap = FMath::Max(InGap, 0.0f);
}

void UCrosshair_CrossDot::SetDotSize(float InSize)
{
	DotSize = FMath::Max(InSize, 0.0f);
	ApplyVisualSettings();
}

void UCrosshair_CrossDot::SetCrosshairColor(FLinearColor InColor)
{
	CrosshairColor = InColor;
	ApplyVisualSettings();
}

void UCrosshair_CrossDot::SetCrosshairRotation(float InRotation)
{
	CrosshairRotation = InRotation;
	ApplyVisualSettings();
}
