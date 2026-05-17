// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCharacter.h"
#include "Weapons/BaseWeapon.h"
#include "GameAbilitySystem/BaseAbilitySystemComponent.h"
#include "GameAbilitySystem/BaseGameplayTags.h"
#include "GameAbilitySystem/BaseStaminaAttributeSet.h"
#include "GameAbilitySystem/BaseHealthAttributeSet.h"
#include "GameAbilitySystem/BaseMovementAttributeSet.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

APlayerCharacter::APlayerCharacter()
{
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 420.0f;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	GetMesh()->bOwnerNoSee = true;
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetCapsuleComponent());
	FirstPersonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	FirstPersonMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	FirstPersonMesh->bOnlyOwnerSee = true;
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->bCastHiddenShadow = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 10.0f, 64.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	StaminaAttributeSet = CreateDefaultSubobject<UBaseStaminaAttributeSet>(TEXT("StaminaAttributeSet"));

	MovementAttributeSet = CreateDefaultSubobject<UBaseMovementAttributeSet>(TEXT("MovementAttributeSet"));
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerCharacter, bIsSprinting);
	DOREPLIFETIME(APlayerCharacter, bIsReloading);
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (StaminaAttributeSet)
	{
		const float Target = StaminaAttributeSet->GetStamina();
		const float MaxTarget = StaminaAttributeSet->GetMaxStamina();
		const float InterpSpeed = 10.0f;
		SmoothedStamina = FMath::FInterpTo(SmoothedStamina, Target, DeltaTime, InterpSpeed);
		OnStaminaUpdated.Broadcast(SmoothedStamina, MaxTarget);
	}
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	SmoothedStamina = StaminaAttributeSet ? StaminaAttributeSet->GetStamina() : 0.0f;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void APlayerCharacter::InitAbilitySystem()
{
	Super::InitAbilitySystem();

	if (AbilitySystemComponent && HasAuthority())
	{
		AbilitySystemComponent->GrantMovementAbilities();
		AbilitySystemComponent->GrantPassiveAbilities();
	}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::OnMove);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::OnLook);
		}
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::OnJumpStarted);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnJumpEnded);
		}
		if (FireAction)
		{
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &APlayerCharacter::OnStartFiring);
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnStopFiring);
		}
		if (ReloadAction)
		{
			EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &APlayerCharacter::OnReload);
		}
		if (SwitchWeaponAction)
		{
			EnhancedInput->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &APlayerCharacter::OnSwitchWeapon);
		}
		if (CrouchAction)
		{
			EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &APlayerCharacter::OnCrouchStarted);
			EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnCrouchEnded);
		}
		if (SprintAction)
		{
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &APlayerCharacter::OnSprintStarted);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnSprintEnded);
		}
	}
}

void APlayerCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D MoveVector = Value.Get<FVector2D>();

	if (Controller)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDir, MoveVector.Y);
		AddMovementInput(RightDir, MoveVector.X);
	}
}

void APlayerCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerYawInput(LookVector.X);
		AddControllerPitchInput(LookVector.Y);
	}
}

void APlayerCharacter::OnJumpStarted()
{
	ServerJump();
}

void APlayerCharacter::OnJumpEnded()
{
	StopJumping();
}

void APlayerCharacter::OnStartFiring()
{
	ServerStartFiring();
}

void APlayerCharacter::OnStopFiring()
{
	ServerStopFiring();
}

void APlayerCharacter::OnReload()
{
	UE_LOG(LogTemp, Warning, TEXT("[Input] OnReload 客户端按下换弹键"));
	ServerReload();
}

void APlayerCharacter::OnSwitchWeapon()
{
	ServerSwitchWeapon();
}

void APlayerCharacter::ServerStartFiring_Implementation()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting))
	{
		AbilitySystemComponent->CancelAbilityHandle(AbilitySystemComponent->GetSprintAbilityHandle());
		return;
	}

	AbilitySystemComponent->TryActivateAbility(AbilitySystemComponent->GetFireAbilityHandle());
}

void APlayerCharacter::ServerStopFiring_Implementation()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Fire));
	}
}

void APlayerCharacter::ServerReload_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[ServerReload] RPC 收到 | Authority=%d"), HasAuthority() ? 1 : 0);

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerReload] 失败: ASC 为 null"));
		return;
	}

	const FGameplayAbilitySpecHandle FireHandle = AbilitySystemComponent->GetFireAbilityHandle();
	UE_LOG(LogTemp, Warning, TEXT("[ServerReload] FireHandle=%d ReloadHandle=%d"),
		FireHandle.IsValid() ? 1 : 0, AbilitySystemComponent->GetReloadAbilityHandle().IsValid() ? 1 : 0);

	AbilitySystemComponent->CancelFireAbility();

	if (AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading))
	{
		return;
	}

	FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GetReloadAbilityHandle();
	if (!Handle.IsValid())
	{
		AbilitySystemComponent->GrantDefaultAbilities();
		Handle = AbilitySystemComponent->GetReloadAbilityHandle();
	}

	UE_LOG(LogTemp, Warning, TEXT("[ServerReload] Handle.IsValid=%d"), Handle.IsValid());

	if (Handle.IsValid())
	{
		const bool bSuccess = AbilitySystemComponent->TryActivateAbility(Handle);
		UE_LOG(LogTemp, Warning, TEXT("[ServerReload] TryActivateAbility 返回: %d"), bSuccess);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ServerReload] 失败: ReloadHandle 无效"));
	}
}

void APlayerCharacter::ServerSwitchWeapon_Implementation()
{
	if (OwnedWeapons.Num() <= 1)
	{
		return;
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	int32 Index = OwnedWeapons.Find(CurrentWeapon);
	Index = (Index + 1) % OwnedWeapons.Num();

	SwitchToWeapon(OwnedWeapons[Index]);
}

void APlayerCharacter::AttachWeaponMeshes(ABaseWeapon* Weapon)
{
	Super::AttachWeaponMeshes(Weapon);

	if (!Weapon)
	{
		return;
	}

	Weapon->GetFirstPersonMesh()->SetVisibility(true);

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);

	Weapon->GetFirstPersonMesh()->AttachToComponent(FirstPersonMesh, AttachRules, FirstPersonWeaponSocket);
	Weapon->GetFirstPersonMesh()->AddLocalRotation(Weapon->FirstPersonMeshRotationOffset);
}

FVector APlayerCharacter::GetWeaponTargetLocation() const
{
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + (FirstPersonCamera->GetForwardVector() * MaxAimDistance);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

	return Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
}

void APlayerCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	MulticastPlayFiringMontage(Montage);
}

void APlayerCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	MulticastPlayReloadMontage(Montage);
}

void APlayerCharacter::AddWeaponRecoil(float RecoilAmount)
{
	MulticastAddWeaponRecoil(RecoilAmount);
}

void APlayerCharacter::MulticastPlayFiringMontage_Implementation(UAnimMontage* Montage)
{
	UE_LOG(LogTemp, Warning, TEXT("[Multicast] PlayFiring | Montage=%s | HasAnim=%d"),
		Montage ? *Montage->GetName() : TEXT("null"),
		FirstPersonMesh->GetAnimInstance() ? 1 : 0);

	if (Montage && FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void APlayerCharacter::MulticastPlayReloadMontage_Implementation(UAnimMontage* Montage)
{
	UE_LOG(LogTemp, Warning, TEXT("[Multicast] PlayReload | Montage=%s | HasAnim=%d"),
		Montage ? *Montage->GetName() : TEXT("null"),
		FirstPersonMesh->GetAnimInstance() ? 1 : 0);

	if (Montage && FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.1f);
	}
}

void APlayerCharacter::MulticastAddWeaponRecoil_Implementation(float RecoilAmount)
{
	AddControllerPitchInput(RecoilAmount);
}

void APlayerCharacter::UpdateHealthHUD()
{
	if (HealthAttributeSet)
	{
		OnHealthUpdated.Broadcast(HealthAttributeSet->GetHealth(), HealthAttributeSet->GetMaxHealth());
	}
}

void APlayerCharacter::OnDeath()
{
	Super::OnDeath();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->DisableInput(PC);
	}
}

void APlayerCharacter::UpdateStaminaHUD()
{
}

void APlayerCharacter::OnCrouchStarted()
{
	if (CrouchInputMode == ECrouchInputMode::Toggle)
	{
		if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching))
		{
			ServerUnCrouch();
		}
		else
		{
			ServerCrouch();
		}
	}
	else
	{
		ServerCrouch();
	}
}

void APlayerCharacter::OnCrouchEnded()
{
	if (CrouchInputMode != ECrouchInputMode::Toggle)
	{
		ServerUnCrouch();
	}
}

void APlayerCharacter::OnSprintStarted()
{
	if (SprintInputMode == ESprintInputMode::Toggle)
	{
		if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting))
		{
			ServerUnSprint();
		}
		else
		{
			ServerSprint();
		}
	}
	else
	{
		ServerSprint();
	}
}

void APlayerCharacter::OnSprintEnded()
{
	if (SprintInputMode != ESprintInputMode::Toggle)
	{
		ServerUnSprint();
	}
}

void APlayerCharacter::ServerCrouch_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[ServerCrouch] RPC received | ASC=%d"), AbilitySystemComponent ? 1 : 0);

	if (!AbilitySystemComponent)
	{
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting))
	{
		const FGameplayAbilitySpecHandle SprintHandle = AbilitySystemComponent->GetSprintAbilityHandle();
		UE_LOG(LogTemp, Warning, TEXT("[ServerCrouch] Cancelling Sprint | Handle=%d"), SprintHandle.IsValid() ? 1 : 0);
		if (SprintHandle.IsValid())
		{
			AbilitySystemComponent->CancelAbilityHandle(SprintHandle);
		}
	}

	const FGameplayAbilitySpecHandle CrouchHandle = AbilitySystemComponent->GetCrouchAbilityHandle();
	UE_LOG(LogTemp, Warning, TEXT("[ServerCrouch] Activating Crouch | Handle=%d"), CrouchHandle.IsValid() ? 1 : 0);
	if (CrouchHandle.IsValid())
	{
		const bool bSuccess = AbilitySystemComponent->TryActivateAbility(CrouchHandle);
		UE_LOG(LogTemp, Warning, TEXT("[ServerCrouch] TryActivateAbility = %d"), bSuccess);
	}
}

void APlayerCharacter::ServerUnCrouch_Implementation()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayAbilitySpecHandle CrouchHandle = AbilitySystemComponent->GetCrouchAbilityHandle();
	if (CrouchHandle.IsValid())
	{
		AbilitySystemComponent->CancelAbilityHandle(CrouchHandle);
	}
}

void APlayerCharacter::ServerSprint_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[ServerSprint] RPC received | ASC=%d"), AbilitySystemComponent ? 1 : 0);

	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayAbilitySpecHandle SprintHandle = AbilitySystemComponent->GetSprintAbilityHandle();
	UE_LOG(LogTemp, Warning, TEXT("[ServerSprint] Activating Sprint | Handle=%d"), SprintHandle.IsValid() ? 1 : 0);
	if (SprintHandle.IsValid())
	{
		const bool bSuccess = AbilitySystemComponent->TryActivateAbility(SprintHandle);
		UE_LOG(LogTemp, Warning, TEXT("[ServerSprint] TryActivateAbility = %d"), bSuccess);
	}
}

void APlayerCharacter::ServerUnSprint_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[ServerUnSprint] RPC received"));

	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayAbilitySpecHandle SprintHandle = AbilitySystemComponent->GetSprintAbilityHandle();
	if (SprintHandle.IsValid())
	{
		AbilitySystemComponent->CancelAbilityHandle(SprintHandle);
	}
}

void APlayerCharacter::ServerJump_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[ServerJump] RPC received | ASC=%d"), AbilitySystemComponent ? 1 : 0);

	if (!AbilitySystemComponent)
	{
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting))
	{
		const FGameplayAbilitySpecHandle SprintHandle = AbilitySystemComponent->GetSprintAbilityHandle();
		if (SprintHandle.IsValid())
		{
			AbilitySystemComponent->CancelAbilityHandle(SprintHandle);
		}
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching))
	{
		const FGameplayAbilitySpecHandle CrouchHandle = AbilitySystemComponent->GetCrouchAbilityHandle();
		if (CrouchHandle.IsValid())
		{
			AbilitySystemComponent->CancelAbilityHandle(CrouchHandle);
		}
	}

	const FGameplayAbilitySpecHandle JumpHandle = AbilitySystemComponent->GetJumpAbilityHandle();
	UE_LOG(LogTemp, Warning, TEXT("[ServerJump] Activating Jump | Handle=%d"), JumpHandle.IsValid() ? 1 : 0);
	if (JumpHandle.IsValid())
	{
		const bool bSuccess = AbilitySystemComponent->TryActivateAbility(JumpHandle);
		UE_LOG(LogTemp, Warning, TEXT("[ServerJump] TryActivateAbility = %d"), bSuccess);
	}
}

void APlayerCharacter::ApplyMovementSpeed()
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!CMC || !MovementAttributeSet)
	{
		return;
	}

	const float BaseSpeed = MovementAttributeSet->GetBaseMoveSpeed();

	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting))
	{
		CMC->MaxWalkSpeed = BaseSpeed * MovementAttributeSet->GetSprintSpeedMultiplier();
	}
	else if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching))
	{
		CMC->MaxWalkSpeedCrouched = BaseSpeed * MovementAttributeSet->GetCrouchSpeedMultiplier();
		CMC->MaxWalkSpeed = CMC->MaxWalkSpeedCrouched;
	}
	else
	{
		CMC->MaxWalkSpeed = BaseSpeed;
	}
}

void APlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(BaseGameplayTags::State_Airborne);
	}
}

void APlayerCharacter::MulticastPerformCrouch_Implementation()
{
	if (HasAuthority())
	{
		Crouch();
	}
	else
	{
		Crouch(true);
	}
}

void APlayerCharacter::MulticastPerformUnCrouch_Implementation()
{
	if (HasAuthority())
	{
		UnCrouch();
	}
	else
	{
		UnCrouch(true);
	}
}

void APlayerCharacter::MulticastPerformJump_Implementation()
{
	if (HasAuthority())
	{
		Jump();
	}
	else
	{
		Jump();
	}
}

void APlayerCharacter::MulticastStartSprint_Implementation()
{
	bIsSprinting = true;
}

void APlayerCharacter::MulticastStopSprint_Implementation()
{
	bIsSprinting = false;
}

void APlayerCharacter::MulticastSetReloading_Implementation(bool bReloading)
{
	bIsReloading = bReloading;
}
