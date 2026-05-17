// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "BaseEnemy.generated.h"

class UWidgetComponent;
class UUserWidget;

UCLASS(Blueprintable)
class MYFPS_DEMO_API ABaseEnemy : public ABaseCharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	UPROPERTY(EditAnywhere, Category = "AI")
	float SightRange = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "AI")
	float SightHalfAngle = 45.0f;

	UPROPERTY(EditAnywhere, Category = "AI")
	float FireRange = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "AI")
	float LoseSightTime = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float PatrolSpeed = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float ChaseSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	UPROPERTY()
	float LastSeenTime = 0.0f;

	UPROPERTY()
	bool bIsFiring = false;

public:
	ABaseEnemy();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual FVector GetWeaponTargetLocation() const override;
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;
	virtual void AddWeaponRecoil(float RecoilAmount) override;
	virtual void UpdateHealthHUD() override;
	virtual void OnDeath() override;
	virtual void MulticastDeathVisuals_Implementation() override;

	UFUNCTION(BlueprintCallable, Category = "AI")
	void StartFiring();

	UFUNCTION(BlueprintCallable, Category = "AI")
	void StopFiring();

	UFUNCTION(BlueprintCallable, Category = "AI")
	void Reload();

protected:
	virtual void InitAbilitySystem() override;

	bool CanSeeTarget() const;
	void UpdateTarget();
	void MoveTowardTarget(float DeltaSeconds);
	void FaceTarget(float DeltaSeconds);
};
