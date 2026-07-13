// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCharacter.h"
#include "BasePlayerController.h"
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
#include "Components/RecoilComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

// ================================================================
//  构造 — FP 双网格 + 摄像机 + 后坐力组件
// ================================================================
APlayerCharacter::APlayerCharacter()
{
	// 胶囊体旋转跟随控制器（FPS 标准设置）
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;

	// 移动时不自动转向（FPS 中移动方向由输入控制，不随速度转向）
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 420.0f;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	// ---- 第三人称网格（继承自 ACharacter）----
	// bOwnerNoSee：自己不看到 TP 身体（避免遮挡 FP 视野）
	GetMesh()->bOwnerNoSee = true;
	// WorldSpaceRepresentation：TP 网格在世界大战空间中渲染（适合阴影、反射等）
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	// ---- 第一人称网格（手臂 + 武器挂载点）----
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetCapsuleComponent());
	FirstPersonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	FirstPersonMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	// bOnlyOwnerSee：只有本地玩家看到 FP 手臂
	FirstPersonMesh->bOnlyOwnerSee = true;
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->bCastHiddenShadow = false;

	// ---- 第一人称摄像机 ----
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	// 位置在胶囊体中心偏上 ≈ 眼睛高度
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 10.0f, 64.0f));
	// bUsePawnControlRotation：摄像机跟随控制器的 Pitch/Yaw
	FirstPersonCamera->bUsePawnControlRotation = true;

	// ---- Player 专属 AttributeSet ----
	StaminaAttributeSet = CreateDefaultSubobject<UBaseStaminaAttributeSet>(TEXT("StaminaAttributeSet"));
	MovementAttributeSet = CreateDefaultSubobject<UBaseMovementAttributeSet>(TEXT("MovementAttributeSet"));

	// ---- 后坐力组件（纯执行器，参数由武器侧传入）----
	RecoilComponent = CreateDefaultSubobject<URecoilComponent>(TEXT("RecoilComponent"));
}

// ================================================================
//  网络复制
// ================================================================
void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerCharacter, bIsSprinting);
	DOREPLIFETIME(APlayerCharacter, bIsReloading);
}

// ================================================================
//  Tick — 体力平滑显示
// ================================================================
void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 体力平滑：让 UI 显示值追赶 AttributeSet 实际值（避免数字跳变）
	if (StaminaAttributeSet)
	{
		const float Target = StaminaAttributeSet->GetStamina();
		const float MaxTarget = StaminaAttributeSet->GetMaxStamina();
		const float InterpSpeed = 10.0f;
		SmoothedStamina = FMath::FInterpTo(SmoothedStamina, Target, DeltaTime, InterpSpeed);
		OnStaminaUpdated.Broadcast(SmoothedStamina, MaxTarget);
	}
}

// ================================================================
//  BeginPlay — 注册 Enhanced Input Mapping Context
// ================================================================
void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	// 初始化体力平滑值为当前实际值（避免 UI 从 0 插值上来）
	SmoothedStamina = StaminaAttributeSet ? StaminaAttributeSet->GetStamina() : 0.0f;

	// 注册 Enhanced Input 映射上下文 → 按键绑定生效
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				// 优先级 0：所有输入上下文的最底层
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

// ================================================================
//  GAS 初始化 — Player 额外授予移动能力和被动能力
// ================================================================
void APlayerCharacter::InitAbilitySystem()
{
	Super::InitAbilitySystem();  // 基类：InitAbilityActorInfo + GrantDefaultAbilities

	// 玩家专属：移动能力（Crouch/Sprint/Jump）+ 被动（体力回复）
	if (AbilitySystemComponent && HasAuthority())
	{
		AbilitySystemComponent->GrantMovementAbilities();
		AbilitySystemComponent->GrantPassiveAbilities();
	}
}

// ================================================================
//  Enhanced Input 绑定 — Started/Triggered/Completed 事件
// ================================================================
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			// Triggered：持续触发（WASD 按住时每帧调用 OnMove）
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
			// Started → 开始开火（按下瞬间）
			// Completed → 停止开火（松开时 GA_WeaponFire 收到 InputReleased）
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &APlayerCharacter::OnStartFiring);
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &APlayerCharacter::OnStopFiring);
		}
		if (ReloadAction)
		{
			// Started：只触发一次（避免 Triggered 导致的重复 RPC）
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

// ================================================================
//  输入回调 — 全部以 IsDead() 守卫开头
//  （防止死后按键通过 Server RPC 激活能力导致崩溃）
// ================================================================

void APlayerCharacter::OnMove(const FInputActionValue& Value)
{
	if (IsDead()) return;
	const FVector2D MoveVector = Value.Get<FVector2D>();

	// 后退时取消冲刺
	if (bIsSprinting && MoveVector.Y < 0.0f)
	{
		ServerUnSprint();
	}

	// 移动方向基于摄像机朝向（而非角色朝向），FPS 标准做法
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
	if (IsDead()) return;
	const FVector2D LookVector = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerYawInput(LookVector.X);
		AddControllerPitchInput(LookVector.Y);
	}
}

void APlayerCharacter::OnJumpStarted()
{
	if (IsDead()) return;
	ServerJump();
}

void APlayerCharacter::OnJumpEnded()
{
	StopJumping();
}

void APlayerCharacter::OnStartFiring()
{
	if (IsDead()) return;
	ServerStartFiring();
}

void APlayerCharacter::OnStopFiring()
{
	ServerStopFiring();
}

void APlayerCharacter::OnReload()
{
	if (IsDead()) return;
	UE_LOG(LogTemp, Warning, TEXT("[Input] OnReload 客户端按下换弹键"));
	ServerReload();
}

void APlayerCharacter::OnSwitchWeapon()
{
	if (IsDead()) return;
	ServerSwitchWeapon();
}

// ================================================================
//  Server RPC — 客户端 → 服务端转发输入
//  这些函数在服务端执行，调用 TryActivateAbility 激活 GAS 能力
// ================================================================

void APlayerCharacter::ServerStartFiring_Implementation()
{
	if (!AbilitySystemComponent) return;

	// 冲刺中开火 → 先取消冲刺（冲刺时不能开火）
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
		// 通知 GA_WeaponFire 的 WaitInputRelease Task：按键已释放
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

	// 先取消开火能力（换弹优先）
	const FGameplayAbilitySpecHandle FireHandle = AbilitySystemComponent->GetFireAbilityHandle();
	UE_LOG(LogTemp, Warning, TEXT("[ServerReload] FireHandle=%d ReloadHandle=%d"),
		FireHandle.IsValid() ? 1 : 0, AbilitySystemComponent->GetReloadAbilityHandle().IsValid() ? 1 : 0);

	AbilitySystemComponent->CancelFireAbility();

	// State.Reloading 标签阻塞自身重复激活
	if (AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading))
	{
		return;
	}

	FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GetReloadAbilityHandle();
	if (!Handle.IsValid())
	{
		// 兜底：如果 ReloadHandle 失效，重新授予能力
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
	// 只有一把武器 → 不用切
	if (OwnedWeapons.Num() <= 1) return;

	// 取消所有活动能力（开火/换弹），避免切枪时产生冲突
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	// 循环切换到下一把
	int32 Index = OwnedWeapons.Find(CurrentWeapon);
	Index = (Index + 1) % OwnedWeapons.Num();

	SwitchToWeapon(OwnedWeapons[Index]);
}

// ================================================================
//  IBaseWeaponHolder 重写 — 武器网格挂载（FP + TP 双网格）
// ================================================================

void APlayerCharacter::AttachWeaponMeshes(ABaseWeapon* Weapon)
{
	// 先执行基类：挂载 TP 网格
	Super::AttachWeaponMeshes(Weapon);

	if (!Weapon) return;

	// 额外挂载 FP 武器网格到 FP 手臂骨骼
	Weapon->GetFirstPersonMesh()->SetVisibility(true);

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);
	Weapon->GetFirstPersonMesh()->AttachToComponent(FirstPersonMesh, AttachRules, FirstPersonWeaponSocket);
	Weapon->GetFirstPersonMesh()->AddLocalRotation(Weapon->FirstPersonMeshRotationOffset);
}

// 瞄准位置：FP 摄像机向前 LineTrace，比基类更精确
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

// ================================================================
//  蒙太奇播放 — 通过 Multicast RPC 在 FP + TP 双网格上播放
//  基类只在 TP 上播放，玩家需要在两个网格上都播放
// ================================================================

void APlayerCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	MulticastPlayFiringMontage(Montage);
}

void APlayerCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	MulticastPlayReloadMontage(Montage);
}

// ================================================================
//  后坐力 — 委托给 RecoilComponent + Multicast RPC
//  RecoilComponent.Tick 逐帧 FInterpTo 平滑应用 Pitch
// ================================================================

void APlayerCharacter::AddWeaponRecoil(float RecoilAmount, float InterpSpeed, float RecoverySpeed, float MaxAccumulation)
{
	if (RecoilComponent)
	{
		RecoilComponent->AddRecoil(RecoilAmount, InterpSpeed, RecoverySpeed, MaxAccumulation);
	}
	// 广播到所有客户端（远程客户端也需要看到后坐力效果）
	MulticastAddWeaponRecoil(RecoilAmount, InterpSpeed, RecoverySpeed, MaxAccumulation);
}

// ================================================================
//  Multicast RPC 实现 — 在所有客户端同步视觉效果
// ================================================================

void APlayerCharacter::MulticastPlayFiringMontage_Implementation(UAnimMontage* Montage)
{
	UE_LOG(LogTemp, Warning, TEXT("[Multicast] PlayFiring | Montage=%s | FPAnim=%d | TPAnim=%d"),
		Montage ? *Montage->GetName() : TEXT("null"),
		FirstPersonMesh->GetAnimInstance() ? 1 : 0,
		GetMesh()->GetAnimInstance() ? 1 : 0);

	if (!Montage) return;

	// 在 FP 手臂网格上播放
	if (FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage);
	}

	// 在 TP 身体网格上播放（其他玩家能看到）
	if (GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}
}

void APlayerCharacter::MulticastPlayReloadMontage_Implementation(UAnimMontage* Montage)
{
	UE_LOG(LogTemp, Warning, TEXT("[Multicast] PlayReload | Montage=%s | FPAnim=%d | TPAnim=%d"),
		Montage ? *Montage->GetName() : TEXT("null"),
		FirstPersonMesh->GetAnimInstance() ? 1 : 0,
		GetMesh()->GetAnimInstance() ? 1 : 0);

	if (!Montage) return;

	// 换弹有 0.1s 淡入时间，避免动画突变
	if (FirstPersonMesh->GetAnimInstance())
	{
		FirstPersonMesh->GetAnimInstance()->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.1f);
	}

	if (GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.1f);
	}
}

void APlayerCharacter::MulticastAddWeaponRecoil_Implementation(float RecoilAmount, float InterpSpeed, float RecoverySpeed, float MaxAccumulation)
{
	// 所有客户端（包括远程）累加后坐力目标值
	// 实际的平滑 Pitch 由各自的 RecoilComponent.Tick 处理
	if (RecoilComponent)
	{
		RecoilComponent->AddRecoil(RecoilAmount, InterpSpeed, RecoverySpeed, MaxAccumulation);
	}
}

// ================================================================
//  HUD 更新
// ================================================================
void APlayerCharacter::UpdateHealthHUD()
{
	if (HealthAttributeSet)
	{
		OnHealthUpdated.Broadcast(HealthAttributeSet->GetHealth(), HealthAttributeSet->GetMaxHealth());
	}
}

// ================================================================
//  死亡系统（重写）
// ================================================================

/** 死亡入口 — 服务端调用。客户端视觉效果在 MulticastDeathVisuals 中处理 */
void APlayerCharacter::OnDeath()
{
	Super::OnDeath();
	// 额外的客户端视觉效果由 MulticastDeathVisuals_Implementation 处理
}

void APlayerCharacter::MulticastDeathVisuals_Implementation()
{
	// ① 基类 Ragdoll（胶囊体关碰撞 + TP Mesh 物理模拟）
	Super::MulticastDeathVisuals_Implementation();

	// ② 隐藏第一人称渲染（所有客户端）
	FirstPersonMesh->SetVisibility(false);
	if (CurrentWeapon)
	{
		CurrentWeapon->GetFirstPersonMesh()->SetVisibility(false);
	}

	// ③ 死亡视角：镜头后拉 + 上移
	FirstPersonCamera->SetRelativeLocation(FVector(-300.0f, 0.0f, 100.0f));

	// ④ 显示鼠标 + 切换到纯 UI 输入模式 + 弹出死亡 Widget（仅本地玩家）
	if (ABasePlayerController* PC = Cast<ABasePlayerController>(GetController()))
	{
		if (PC->IsLocalPlayerController())
		{
			PC->SetShowMouseCursor(true);
			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);

			PC->DisableInput(PC);
			PC->ShowDeathScreen();
		}
	}
}

void APlayerCharacter::UpdateStaminaHUD()
{
}

// ================================================================
//  下蹲输入 — 支持 Hold/Toggle 两种模式
// ================================================================
void APlayerCharacter::OnCrouchStarted()
{
	if (IsDead()) return;
	if (CrouchInputMode == ECrouchInputMode::Toggle)
	{
		// Toggle：按下时切换蹲/站
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
		// Hold：按住蹲，松开站
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

// ================================================================
//  冲刺输入 — 支持 Hold/Toggle 两种模式
// ================================================================
void APlayerCharacter::OnSprintStarted()
{
	if (IsDead()) return;
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

// ================================================================
//  移动能力 Server RPC
//  （含大量调试日志，适合排查 GAS 能力激活问题）
// ================================================================

void APlayerCharacter::ServerCrouch_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[ServerCrouch] RPC received | ASC=%d"), AbilitySystemComponent ? 1 : 0);

	if (!AbilitySystemComponent) return;

	// 冲刺中下蹲：先取消冲刺
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
	if (!AbilitySystemComponent) return;

	const FGameplayAbilitySpecHandle CrouchHandle = AbilitySystemComponent->GetCrouchAbilityHandle();
	if (CrouchHandle.IsValid())
	{
		AbilitySystemComponent->CancelAbilityHandle(CrouchHandle);
	}
}

void APlayerCharacter::ServerSprint_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[ServerSprint] RPC received | ASC=%d"), AbilitySystemComponent ? 1 : 0);

	if (!AbilitySystemComponent) return;

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

	if (!AbilitySystemComponent) return;

	const FGameplayAbilitySpecHandle SprintHandle = AbilitySystemComponent->GetSprintAbilityHandle();
	if (SprintHandle.IsValid())
	{
		AbilitySystemComponent->CancelAbilityHandle(SprintHandle);
	}
}

void APlayerCharacter::ServerJump_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[ServerJump] RPC received | ASC=%d"), AbilitySystemComponent ? 1 : 0);

	if (!AbilitySystemComponent) return;

	// 跳跃时自动取消 Sprint/Crouch（不能在空中冲刺/下蹲）
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

// ================================================================
//  移动速度 — 根据 GAS 标签动态调整
//  由 GA_Sprint/GA_Crouch 的 Tick 调用
// ================================================================
void APlayerCharacter::ApplyMovementSpeed()
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!CMC || !MovementAttributeSet) return;

	const float BaseSpeed = MovementAttributeSet->GetBaseMoveSpeed();

	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Sprinting))
	{
		// 冲刺速度 = 基础速度 × 冲刺倍率
		CMC->MaxWalkSpeed = BaseSpeed * MovementAttributeSet->GetSprintSpeedMultiplier();
	}
	else if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Crouching))
	{
		// 下蹲速度 = 基础速度 × 下蹲倍率
		CMC->MaxWalkSpeedCrouched = BaseSpeed * MovementAttributeSet->GetCrouchSpeedMultiplier();
		CMC->MaxWalkSpeed = CMC->MaxWalkSpeedCrouched;
	}
	else
	{
		CMC->MaxWalkSpeed = BaseSpeed;
	}
}

// ================================================================
//  落地回调 — 移除 State.Airborne 标签
// ================================================================
void APlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(BaseGameplayTags::State_Airborne);
	}
}

// ================================================================
//  移动 Multicast RPC — 在所有客户端执行移动相关操作
//  HasAuthority 分支：服务端走标准接口，客户端走 bClientSimulation
// ================================================================

void APlayerCharacter::MulticastPerformCrouch_Implementation()
{
	if (HasAuthority())
	{
		Crouch();
	}
	else
	{
		// bClientSimulation = true：客户端模拟下蹲（不改变网络状态，仅视觉效果）
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
	Jump();
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
