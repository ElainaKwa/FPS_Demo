// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseEnemy.h"
#include "Weapons/BaseWeapon.h"
#include "UI/Health/BaseHealthBarWidget.h"
#include "GameAbilitySystem/BaseAbilitySystemComponent.h"
#include "GameAbilitySystem/BaseHealthAttributeSet.h"
#include "GameAbilitySystem/BaseGameplayTags.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

ABaseEnemy::ABaseEnemy()
{
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 540, 0);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(GetCapsuleComponent());
	HealthBarWidget->SetRelativeLocation(FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20));
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::World);
	HealthBarWidget->SetDrawSize(FVector2D(200, 10));
	HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABaseEnemy::BeginPlay()
{
	Super::BeginPlay();

	InitialLocation = GetActorLocation();

	UE_LOG(LogTemp, Warning, TEXT("[%s] BeginPlay | InitialLocation=%s"),
		*GetName(), *InitialLocation.ToString());

	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(true);

		if (HealthBarWidgetClass)
		{
			HealthBarWidget->SetWidgetClass(HealthBarWidgetClass);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] HealthBarWidgetClass not set"), *GetName());
		}
	}

	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		UpdateHealthHUD();
	});
}

void ABaseEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Dead))
	{
		return;
	}

	UpdateTarget();

	if (TargetActor)
	{
		FaceTarget(DeltaSeconds);

		const float Distance = FVector::Dist(GetActorLocation(), TargetActor->GetActorLocation());

		if (Distance <= FireRange && CanSeeTarget())
		{
			if (!bIsFiring)
			{
				StartFiring();
			}
		}
		else
		{
			if (bIsFiring)
			{
				StopFiring();
			}
			MoveTowardTarget(DeltaSeconds);
		}
	}
	else
	{
		if (bIsFiring)
		{
			StopFiring();
		}
	}
}

void ABaseEnemy::InitAbilitySystem()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		AbilitySystemComponent->GrantDefaultAbilities();
	}
}

FVector ABaseEnemy::GetWeaponTargetLocation() const
{
	if (TargetActor)
	{
		return TargetActor->GetActorLocation() + FVector(0, 0, 90.0f);
	}
	return GetActorLocation() + GetActorForwardVector() * 5000.0f;
}

void ABaseEnemy::PlayFiringMontage(UAnimMontage* Montage)
{
	if (Montage && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}
}

void ABaseEnemy::AddWeaponRecoil(float RecoilAmount, float InterpSpeed, float RecoverySpeed, float MaxAccumulation)
{
}

void ABaseEnemy::UpdateHealthHUD()
{
	if (HealthBarWidget && HealthAttributeSet)
	{
		if (UBaseHealthBarWidget* Widget = Cast<UBaseHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
		{
			Widget->UpdateHealth(HealthAttributeSet->GetHealth(), HealthAttributeSet->GetMaxHealth());
		}
	}
}

void ABaseEnemy::OnDeath()
{
	const bool bAlreadyDead = AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Dead);

	UE_LOG(LogTemp, Warning, TEXT("[%s] OnDeath | AlreadyDead=%d | Authority=%d"),
		*GetName(), bAlreadyDead ? 1 : 0, HasAuthority() ? 1 : 0);

	Super::OnDeath();

	if (HasAuthority() && !bAlreadyDead)
	{
		// Clear any existing respawn timer, then set a new one
		GetWorld()->GetTimerManager().ClearTimer(RespawnTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(
			RespawnTimerHandle,
			this,
			&ABaseEnemy::Respawn,
			RespawnDelay,
			false
		);

		UE_LOG(LogTemp, Warning, TEXT("[%s] Respawn timer set | Delay=%.1fs"),
			*GetName(), RespawnDelay);
	}
}

void ABaseEnemy::MulticastDeathVisuals_Implementation()
{
	Super::MulticastDeathVisuals_Implementation();

	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(false);
	}
}

void ABaseEnemy::StartFiring()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->TryActivateAbility(AbilitySystemComponent->GetFireAbilityHandle());
		bIsFiring = true;
	}
}

void ABaseEnemy::StopFiring()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Fire));
		bIsFiring = false;
	}
}

void ABaseEnemy::Reload()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelFireAbility();
		if (FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GetReloadAbilityHandle(); Handle.IsValid())
		{
			AbilitySystemComponent->TryActivateAbility(Handle);
		}
	}
}

void ABaseEnemy::UpdateTarget()
{
	if (TargetActor)
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
		{
			if (TargetASC->HasMatchingGameplayTag(BaseGameplayTags::State_Dead))
			{
				TargetActor = nullptr;
			}
		}
	}

	if (!TargetActor)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->GetPawn())
		{
			const float Dist = FVector::Dist(GetActorLocation(), PC->GetPawn()->GetActorLocation());
			if (Dist <= SightRange)
			{
				TargetActor = PC->GetPawn();
			}
		}
	}
}

bool ABaseEnemy::CanSeeTarget() const
{
	if (!TargetActor) return false;

	const FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	const float Dot = FVector::DotProduct(GetActorForwardVector(), Direction);
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));

	if (Dot < CosHalfAngle) return false;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(TargetActor);

	const FVector Start = GetActorLocation() + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector End = TargetActor->GetActorLocation() + FVector(0, 0, 90.0f);

	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	return !Hit.bBlockingHit;
}

void ABaseEnemy::MoveTowardTarget(float DeltaSeconds)
{
	if (!TargetActor) return;

	const FVector Dir = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	AddMovementInput(Dir, 1.0f);

	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
}

void ABaseEnemy::FaceTarget(float DeltaSeconds)
{
	if (!TargetActor) return;

	const FVector Dir = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const FRotator TargetRot = Dir.Rotation();

	const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaSeconds, 10.0f);
	SetActorRotation(NewRot);
}

// ------------------------------------------------------------------
//  Respawn system
// ------------------------------------------------------------------

void ABaseEnemy::Respawn()
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] Respawn() called"), *GetName());

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Respawn() aborted: not authority"), *GetName());
		return;
	}

	// 1. Pick a respawn location
	const FVector NewLocation = SelectRespawnLocation();

	// 2. Reset movement component (clear any leftover velocity / falling state)
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (CMC)
	{
		CMC->StopMovementImmediately();
		CMC->Velocity = FVector::ZeroVector;
		CMC->SetMovementMode(MOVE_Walking);
	}

	// 3. Teleport to new location (safe for CharacterMovementComponent)
	TeleportTo(NewLocation, GetActorRotation(), false, true);

	// 4. Reset health
	if (HealthAttributeSet)
	{
		HealthAttributeSet->SetHealth(HealthAttributeSet->GetMaxHealth());
		UE_LOG(LogTemp, Warning, TEXT("[%s] Health reset to %.0f"), *GetName(), HealthAttributeSet->GetHealth());
	}

	// 5. Remove death tag
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(BaseGameplayTags::State_Dead);
	}

	// 6. Re-equip weapon
	if (!CurrentWeapon)
	{
		SpawnDefaultWeapon();
		UE_LOG(LogTemp, Warning, TEXT("[%s] Weapon respawned | CurrentWeapon=%s"),
			*GetName(), CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
	}

	// 7. Broadcast visuals to all clients
	MulticastRespawnVisuals();

	UE_LOG(LogTemp, Warning, TEXT("[%s] Respawn complete | Location=%s | Health=%.0f"),
		*GetName(), *NewLocation.ToString(),
		HealthAttributeSet ? HealthAttributeSet->GetHealth() : -1.0f);
}

void ABaseEnemy::MulticastRespawnVisuals_Implementation()
{
	// Stop ragdoll, reset mesh to standing pose
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false));
	GetMesh()->InitAnim(true);

	// Restore capsule collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Show actor
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// Restore health bar
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(true);
	}

	UpdateHealthHUD();
}

FVector ABaseEnemy::SelectRespawnLocation() const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] SelectRespawnLocation: No NavSystem, returning InitialLocation"), *GetName());
		return InitialLocation;
	}

	FVector SearchOrigin;
	if (bUseCustomRespawnOrigins && RespawnOrigins.Num() > 0)
	{
		SearchOrigin = RespawnOrigins[FMath::RandRange(0, RespawnOrigins.Num() - 1)];
	}
	else
	{
		SearchOrigin = InitialLocation;
	}

	for (int32 Attempt = 0; Attempt < 10; ++Attempt)
	{
		FNavLocation NavLocation;
		const bool bFound = NavSys->GetRandomReachablePointInRadius(
			SearchOrigin,
			RespawnSearchRadius,
			NavLocation
		);

		if (!bFound)
		{
			continue;
		}

		if (IsLocationSafe(NavLocation.Location))
		{
			return NavLocation.Location;
		}
	}

	// Fallback: any point on NavMesh
	FNavLocation FallbackLoc;
	if (NavSys->GetRandomPoint(FallbackLoc))
	{
		return FallbackLoc.Location;
	}

	return InitialLocation;
}

bool ABaseEnemy::IsLocationSafe(const FVector& Location) const
{
	// 1. Min distance from all players
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		APawn* PlayerPawn = PC->GetPawn();
		if (!PlayerPawn) continue;

		const float Dist = FVector::Dist(Location, PlayerPawn->GetActorLocation());
		if (Dist < MinRespawnDistanceToPlayer)
		{
			return false;
		}
	}

	// 2. Capsule overlap test — check at capsule center (NavMesh returns floor-level point)
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector CapsuleCenter = Location + FVector(0, 0, HalfHeight);

	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// Check for any blocking collision with world geometry or other pawns
	const bool bOverlap = GetWorld()->OverlapAnyTestByChannel(
		CapsuleCenter,
		FQuat::Identity,
		ECC_Pawn,
		CapsuleShape,
		Params
	);

	if (bOverlap)
	{
		return false;
	}

	// 3. Ceiling clearance check — ensure the character can stand upright
	FHitResult CeilingHit;
	const FVector CeilingStart = CapsuleCenter;
	const FVector CeilingEnd = CapsuleCenter + FVector(0, 0, HalfHeight + 50.0f);

	GetWorld()->LineTraceSingleByChannel(
		CeilingHit,
		CeilingStart,
		CeilingEnd,
		ECC_Visibility,
		Params
	);

	if (CeilingHit.bBlockingHit)
	{
		return false;  // Low ceiling — not enough room to stand
	}

	return true;
}
