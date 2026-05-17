// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseDroppedMagazine.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class MYFPS_DEMO_API ABaseDroppedMagazine : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MagazineMesh;

	UPROPERTY(EditAnywhere, Category = "Drop", meta = (ClampMin = 0.1f, ClampMax = 30.0f, Units = "s"))
	float AutoDestroyTime = 5.0f;

	UPROPERTY(ReplicatedUsing = OnRep_DropData)
	TObjectPtr<UStaticMesh> DropMesh;

	UPROPERTY(Replicated)
	FVector DropVelocity;

	FTimerHandle DestroyTimer;

public:

	ABaseDroppedMagazine();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Drop")
	void Initialize(UStaticMesh* InMesh, const FVector& InitialVelocity);

	UFUNCTION(BlueprintPure, Category = "Drop")
	UStaticMeshComponent* GetMagazineMesh() const { return MagazineMesh; }

protected:

	virtual void BeginPlay() override;

	void OnDestroyTimer();

	UFUNCTION()
	void OnRep_DropData();
};
