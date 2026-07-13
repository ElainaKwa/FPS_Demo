// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseCharacter.h"
#include "Weapons/BaseWeapon.h"
#include "GameAbilitySystem/BaseAbilitySystemComponent.h"
#include "GameAbilitySystem/BaseWeaponAttributeSet.h"
#include "GameAbilitySystem/BaseHealthAttributeSet.h"
#include "GameAbilitySystem/BaseGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

// ================================================================
//  构造 — 创建 GAS 组件 + AttributeSet
// ================================================================
ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 胶囊体尺寸（UE 标准人体：半径 42cm，半高 96cm）
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// 第三人称骨骼网格初始偏移（下移半高让脚着地，旋转适配 UE 骨骼朝向）
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	// ---- GAS 核心 ----
	AbilitySystemComponent = CreateDefaultSubobject<UBaseAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// Mixed 模式：GE 应用到所有人，属性只复制给 Owner
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthAttributeSet = CreateDefaultSubobject<UBaseHealthAttributeSet>(TEXT("HealthAttributeSet"));
	WeaponAttributeSet = CreateDefaultSubobject<UBaseWeaponAttributeSet>(TEXT("WeaponAttributeSet"));
}

// ================================================================
//  网络复制 — 只复制 CurrentWeapon，Kills/Deaths 在 PlayerState 里
// ================================================================
void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseCharacter, CurrentWeapon);
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// ① 初始化 ASC 的 ActorInfo（owner = avatar = this）+ 服务端授予能力
	InitAbilitySystem();

	// ② 生成默认武器
	SpawnDefaultWeapon();

	// ③ 如果 TP Mesh 没有骨骼资源（纯 FP 游戏），关闭动画避免报错
	if (!GetMesh()->GetSkeletalMeshAsset())
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationCustomMode);
	}
}

void ABaseCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// ================================================================
//  GAS 初始化
// ================================================================
void ABaseCharacter::InitAbilitySystem()
{
	if (AbilitySystemComponent)
	{
		// 设置 Avatar 和 Owner 为自身（角色即为能力的宿主和执行者）
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// 只在服务端授予能力（客户端不需要 GiveAbility，否则报错）
		if (HasAuthority())
		{
			AbilitySystemComponent->GrantDefaultAbilities();
		}
	}
}

void ABaseCharacter::SpawnDefaultWeapon()
{
	if (DefaultWeaponClass)
	{
		AddWeapon(DefaultWeaponClass);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] DefaultWeaponClass 未配置"), *GetName());
	}
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// ================================================================
//  AttributeSet ↔ Weapon 同步
// ================================================================

/** 把武器的弹药数据同步到 AttributeSet（用于 GAS GE 消耗/补充弹药后的数据一致性） */
void ABaseCharacter::SyncAttributeSetFromWeapon() const
{
	if (!WeaponAttributeSet || !CurrentWeapon)
	{
		return;
	}

	const int32 MaxAmmo = CurrentWeapon->GetEffectiveMagazineSize();
	WeaponAttributeSet->SetMaxAmmo(static_cast<float>(MaxAmmo));
	WeaponAttributeSet->SetCurrentAmmo(static_cast<float>(CurrentWeapon->CurrentBullets));
}

// ================================================================
//  IBaseWeaponHolder — 武器网格挂载
// ================================================================

/** 默认行为：只挂载武器的第三人称网格到角色的 TP 骨骼上 */
void ABaseCharacter::AttachWeaponMeshes(ABaseWeapon* Weapon)
{
	if (!Weapon)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] 挂载武器 %s  |  MeshHasAsset=%d  |  Socket=%s"),
		*GetName(), *Weapon->GetName(),
		GetMesh()->GetSkeletalMeshAsset() != nullptr,
		*ThirdPersonWeaponSocket.ToString());

	// 隐藏 FP 武器模型（基础类不处理第一人称）
	Weapon->GetFirstPersonMesh()->SetVisibility(false);
	Weapon->GetThirdPersonMesh()->SetVisibility(true);

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);

	// 如果角色有 TP 骨骼，挂载到武器 Socket
	if (GetMesh()->GetSkeletalMeshAsset())
	{
		Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachRules, ThirdPersonWeaponSocket);
	}
	else
	{
		// 没有骨骼则直接挂到 Root（纯 FP 模式）
		Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachRules);
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] 武器 TP Mesh: HasAsset=%d, bVisible=%d, bOwnerNoSee=%d"),
		*GetName(),
		Weapon->GetThirdPersonMesh()->GetSkeletalMeshAsset() != nullptr,
		Weapon->GetThirdPersonMesh()->IsVisible(),
		Weapon->GetThirdPersonMesh()->bOwnerNoSee);
}

// ================================================================
//  瞄准位置 — 从角色眼睛位置向前 LineTrace，用于敌人 AI 判断能否看到目标
// ================================================================
FVector ABaseCharacter::GetWeaponTargetLocation() const
{
	// 从角色眼睛高度（胶囊体半高）向前 100 米
	const FVector Start = GetActorLocation() + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector End = Start + (GetActorForwardVector() * 10000.0f);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

	// 命中则返回命中点，否则返回最远点
	return Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
}

// ================================================================
//  蒙太奇播放 — 基类仅在 TP Mesh 上播放
//  PlayerCharacter 会重写这些方法，通过 Multicast RPC 同时在 FP+TP 上播放
// ================================================================
void ABaseCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	if (Montage && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}
}

void ABaseCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	if (Montage && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}
}

// ================================================================
//  后坐力 — 基类空实现（敌人无后坐力）
//  PlayerCharacter 重写委托给 URecoilComponent
// ================================================================
void ABaseCharacter::AddWeaponRecoil(float RecoilAmount, float InterpSpeed, float RecoverySpeed, float MaxAccumulation)
{
	// 基类空实现 — 只有玩家有后坐力
}

// ================================================================
//  HUD 更新
// ================================================================
void ABaseCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo)
{
	OnBulletCountUpdated.Broadcast(CurrentAmmo, MaxAmmo);
}

void ABaseCharacter::UpdateHealthHUD()
{
	if (HealthAttributeSet)
	{
		OnHealthUpdated.Broadcast(HealthAttributeSet->GetHealth(), HealthAttributeSet->GetMaxHealth());
	}
}

// ================================================================
//  死亡系统
// ================================================================

bool ABaseCharacter::IsDead() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Dead);
}

void ABaseCharacter::OnDeath()
{
	// ----- 守卫：防止同一帧的多个 GE 重复触发死亡 -----
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Dead))
	{
		return;
	}

	// ----- ① 打死亡标签 + 取消所有能力 -----
	if (AbilitySystemComponent)
	{
		// State_Dead 标签会阻塞所有能力激活（Crouch/Sprint/Jump/Fire/Reload）
		AbilitySystemComponent->AddLooseGameplayTag(BaseGameplayTags::State_Dead);
		// 中断所有正在执行的能力
		AbilitySystemComponent->CancelAllAbilities();
	}

	// ----- ② 武器掉落（仅服务端）-----
	if (CurrentWeapon && HasAuthority())
	{
		// 从库存移除，否则 SpawnDefaultWeapon 会错误找回旧武器
		OwnedWeapons.Remove(CurrentWeapon);
		CurrentWeapon->DropToGround();  // 解除 Owner 绑定 + 开启物理
		CurrentWeapon = nullptr;
	}

	// ----- ③ 广播死亡视觉效果到所有客户端（Ragdoll）-----
	MulticastDeathVisuals();
}

void ABaseCharacter::MulticastDeathVisuals_Implementation()
{
	// 关闭胶囊体碰撞（不再挡子弹、不再被推挤）
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 对 TP Mesh 开启 Ragdoll 物理模拟（所有客户端可见）
	if (GetMesh()->GetSkeletalMeshAsset())
	{
		GetMesh()->SetSimulatePhysics(true);
		// PhysicsOnly：开启物理碰撞，关闭 Query 碰撞（不挡子弹）
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	}
}

// ================================================================
//  武器复制回调 — 客户端收到 CurrentWeapon 后同步 ASC + HUD
// ================================================================
void ABaseCharacter::OnRep_CurrentWeapon()
{
	if (CurrentWeapon && AbilitySystemComponent)
	{
		// 通知 ASC 当前武器已更新（能力通过 ASC 获取武器数据）
		AbilitySystemComponent->SetCurrentWeapon(CurrentWeapon);
		// 同步弹药 AttributeSet
		SyncAttributeSetFromWeapon();
		// 刷新 HUD
		UpdateWeaponHUD(CurrentWeapon->CurrentBullets, CurrentWeapon->GetEffectiveMagazineSize());
	}
}

// ================================================================
//  武器管理 — 添加 / 切换 / 查找
// ================================================================

void ABaseCharacter::AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass)
{
	// 武器生成只在服务端执行（客户端通过 CurrentWeapon 复制获取）
	if (!HasAuthority() || !WeaponClass)
	{
		return;
	}

	// 已有同类型武器 → 直接切换（避免重复生成）
	ABaseWeapon* Existing = FindWeaponOfClass(WeaponClass);
	if (Existing)
	{
		SwitchToWeapon(Existing);
		return;
	}

	// 生成新武器 Actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseWeapon* NewWeapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
	if (!NewWeapon)
	{
		return;
	}

	// 存入库存
	OwnedWeapons.Add(NewWeapon);

	// 切换装备：停用旧武器 → 激活新武器
	if (CurrentWeapon)
	{
		CurrentWeapon->DeactivateWeapon();  // 隐藏模型
	}

	CurrentWeapon = NewWeapon;
	CurrentWeapon->ActivateWeapon();  // 显示模型 + 挂载到角色

	// 通知 ASC 当前武器已变更
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetCurrentWeapon(CurrentWeapon);
	}

	// 同步弹药数据到 AttributeSet
	SyncAttributeSetFromWeapon();
}

void ABaseCharacter::SwitchToWeapon(ABaseWeapon* NewWeapon)
{
	if (!HasAuthority() || !NewWeapon || NewWeapon == CurrentWeapon)
	{
		return;
	}

	// 停用旧武器（隐藏模型）
	if (CurrentWeapon)
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// 激活新武器（显示模型 + 挂载）
	CurrentWeapon = NewWeapon;
	CurrentWeapon->ActivateWeapon();

	// 通知 ASC + 同步弹药
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetCurrentWeapon(CurrentWeapon);
	}

	SyncAttributeSetFromWeapon();
}

/** 在 OwnedWeapons 数组中查找指定类的武器（用于切换和去重） */
ABaseWeapon* ABaseCharacter::FindWeaponOfClass(TSubclassOf<ABaseWeapon> WeaponClass) const
{
	for (ABaseWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon && Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}
	return nullptr;
}
