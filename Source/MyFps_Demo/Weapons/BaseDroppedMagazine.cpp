// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseDroppedMagazine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

ABaseDroppedMagazine::ABaseDroppedMagazine()
{
	PrimaryActorTick.bCanEverTick = false;

	MagazineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Magazine Mesh"));
	RootComponent = MagazineMesh;
	MagazineMesh->SetSimulatePhysics(true);
	MagazineMesh->SetEnableGravity(true);
	MagazineMesh->SetCollisionProfileName(FName("PhysicsActor"));
	MagazineMesh->SetNotifyRigidBodyCollision(true);
}

void ABaseDroppedMagazine::Initialize(UStaticMesh* InMesh, const FVector& InitialVelocity)
{
	if (InMesh)
	{
		MagazineMesh->SetStaticMesh(InMesh);
	}

	MagazineMesh->SetPhysicsLinearVelocity(InitialVelocity);
	MagazineMesh->AddAngularImpulseInRadians(FVector(FMath::FRandRange(-3.0f, 3.0f), FMath::FRandRange(-3.0f, 3.0f), FMath::FRandRange(-3.0f, 3.0f)), NAME_None, true);
}

void ABaseDroppedMagazine::BeginPlay()
{
	Super::BeginPlay();

	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, this, &ABaseDroppedMagazine::OnDestroyTimer, AutoDestroyTime, false);
}

void ABaseDroppedMagazine::OnDestroyTimer()
{
	Destroy();
}
