#pragma once

#include "CoreMinimal.h"
#include "BaseCrosshairWidget.h"
#include "Crosshair_CrossDot.generated.h"

class UImage;

UCLASS(Blueprintable)
class MYFPS_DEMO_API UCrosshair_CrossDot : public UBaseCrosshairWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	virtual void OnSpreadUpdated_Implementation(float Spread) override;
	virtual void OnVisibilityChanged_Implementation(bool bVisible) override;

	virtual FCrosshairSettings GatherSettings_Implementation() const override;
	virtual void ApplySettings_Implementation(const FCrosshairSettings& Settings) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Top;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Bottom;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Left;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Right;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Dot;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "4.0", ClampMax = "100.0"))
	float LineLength = 13.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float LineThickness = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float BaseGap = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float DotSize = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "10.0", ClampMax = "300.0"))
	float MaxSpreadPixels = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float CrosshairRotation = 90.0f;

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetLineLength(float InLength);

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetLineThickness(float InThickness);

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetBaseGap(float InGap);

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetDotSize(float InSize);

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairColor(FLinearColor InColor);

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairRotation(float InRotation);

private:
	void ApplyVisualSettings();
	void ApplySpreadToLines(float Offset);
	void SetAllLinesVisibility(bool bVisible);
};
