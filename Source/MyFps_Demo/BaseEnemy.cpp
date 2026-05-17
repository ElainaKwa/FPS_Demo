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

	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(true);

		if (HealthBarWidgetClass)
		{
			HealthBarWidget->SetWidgetClass(HealthBarWidgetClass);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] HealthBarWidgetClass 未配置，血条不会显示"), *GetName());
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
					FString::Printf(TEXT("[%s] HealthBarWidgetClass 未配置，请在 BP 中设置"), *GetName()));
			}
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

void ABaseEnemy::AddWeaponRecoil(float RecoilAmount)
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
	Super::OnDeath();

	if (HasAuthority())
	{
		SetLifeSpan(5.0f);
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
