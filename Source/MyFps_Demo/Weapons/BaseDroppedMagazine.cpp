// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseDroppedMagazine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ABaseDroppedMagazine::ABaseDroppedMagazine()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	MagazineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Magazine Mesh"));
	RootComponent = MagazineMesh;
	MagazineMesh->SetSimulatePhysics(true);
	MagazineMesh->SetEnableGravity(true);
	MagazineMesh->SetCollisionProfileName(FName("PhysicsActor"));
	MagazineMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MagazineMesh->SetNotifyRigidBodyCollision(true);
}

void ABaseDroppedMagazine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseDroppedMagazine, DropMesh);
	DOREPLIFETIME(ABaseDroppedMagazine, DropVelocity);
}

void ABaseDroppedMagazine::Initialize(UStaticMesh* InMesh, const FVector& InitialVelocity)
{
	DropMesh = InMesh;
	DropVelocity = InitialVelocity;

	if (HasAuthority())
	{
		OnRep_DropData();
	}
}

void ABaseDroppedMagazine::OnRep_DropData()
{
	if (DropMesh)
	{
		MagazineMesh->SetStaticMesh(DropMesh);
	}

	if (!HasAuthority())
	{
		return;
	}

	MagazineMesh->SetPhysicsLinearVelocity(DropVelocity);
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
