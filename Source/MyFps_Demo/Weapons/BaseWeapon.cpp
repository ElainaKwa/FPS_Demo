// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeapon.h"
#include "BaseWeaponAttachment.h"
#include "BaseDroppedMagazine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstance.h"
#include "Engine/DamageEvents.h"
#include "Net/UnrealNetwork.h"

// ================================================================
//  构造 — 双网格 + 左手弹匣
// ================================================================
ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;  // 纯数据容器，不需要 Tick
	bReplicates = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// ---- FP 武器模型（仅本地玩家可见）----
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bSelfShadowOnly = true;

	// ---- TP 武器模型（对其他玩家可见）----
	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);
	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;  // 不显示给自己的 TP 武器（会和 FP 重叠）

	// ---- 换弹时左手持有的替换弹匣 ----
	LeftHandMagazineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Left Hand Magazine"));
	LeftHandMagazineMesh->SetupAttachment(RootComponent);
	LeftHandMagazineMesh->SetCollisionProfileName(FName("NoCollision"));
	LeftHandMagazineMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	LeftHandMagazineMesh->SetVisibility(false);  // 默认隐藏，换弹时显示

	// 初始隐藏，直到被角色装备
	SetActorHiddenInGame(true);
}

// ================================================================
//  网络复制
// ================================================================
void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 只复制弹药数（最频繁变化的数据）
	DOREPLIFETIME(ABaseWeapon, CurrentBullets);
}

/** CurrentBullets 复制到客户端后刷新 HUD */
void ABaseWeapon::OnRep_CurrentBullets()
{
	UE_LOG(LogTemp, Warning, TEXT("[OnRep] Weapon::CurrentBullets 复制: %d"), CurrentBullets);

	// WeaponOwner 可能在 BeginPlay 前被 OnRep 触发（Owner 同步延迟），兜底获取
	if (!WeaponOwner)
	{
		WeaponOwner = Cast<IBaseWeaponHolder>(GetOwner());
	}
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

// ================================================================
//  BeginPlay — 绑定 Owner + 初始化弹药 + 挂载网格 + 刷新 HUD
// ================================================================
void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	// 绑定 Owner 销毁回调（Owner 死时连带销毁武器）
	if (GetOwner())
	{
		GetOwner()->OnDestroyed.AddDynamic(this, &ABaseWeapon::OnOwnerDestroyed);
	}

	WeaponOwner = Cast<IBaseWeaponHolder>(GetOwner());

	// 初始弹药填满弹匣
	CurrentBullets = MagazineSize;

	if (WeaponOwner)
	{
		// 通知 Owner 把武器网格挂载到角色骨骼上
		WeaponOwner->AttachWeaponMeshes(this);
		// 初始化 HUD 显示
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void ABaseWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 清理所有配件（防止泄漏）
	ClearAllAttachments();
}

/** Owner 销毁时的回调 → 武器跟随销毁 */
void ABaseWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

// ================================================================
//  激活 / 停用
// ================================================================
void ABaseWeapon::ActivateWeapon()
{
	SetActorHiddenInGame(false);
	bIsReloading = false;

	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void ABaseWeapon::DeactivateWeapon()
{
	SetActorHiddenInGame(true);
}

// ================================================================
//  武器掉落
// ================================================================
void ABaseWeapon::DropToGround()
{
	// 解除 Owner 销毁绑定（否则 Owner 5s 后销毁会连带销毁地上武器）
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnDestroyed.RemoveDynamic(this, &ABaseWeapon::OnOwnerDestroyed);
		SetOwner(nullptr);
	}

	WeaponOwner = nullptr;

	// 60 秒后自动清理地上的武器
	if (HasAuthority())
	{
		SetLifeSpan(60.0f);
	}

	// 广播视觉效果到所有客户端
	MulticastDropToGroundVisuals();
}

void ABaseWeapon::MulticastDropToGroundVisuals_Implementation()
{
	// 隐藏 FP 模型
	FirstPersonMesh->SetVisibility(false);

	// TP 模型从角色分离 → 开启物理模拟 → 掉落到地面
	ThirdPersonMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	ThirdPersonMesh->SetSimulatePhysics(true);
	ThirdPersonMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ThirdPersonMesh->SetCollisionProfileName(FName("PhysicsActor"));
	// 忽略 Pawn 碰撞（防止武器推走角色）
	ThirdPersonMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

// ================================================================
//  战斗计算
// ================================================================

/** 从 FP 武器模型的 MuzzleSocket 获取世界空间位置 */
FVector ABaseWeapon::GetMuzzleLocation() const
{
	return FirstPersonMesh->GetSocketLocation(MuzzleSocketName);
}

/** 在瞄准方向圆锥内随机偏移 AimVariance 度 → 模拟散布 */
FVector ABaseWeapon::CalculateSpread(const FVector& AimDirection) const
{
	const float Variance = GetEffectiveAimVariance();
	if (Variance <= 0.0f)
	{
		return AimDirection;
	}

	return UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDirection, Variance);
}

/** 配件插槽 → Socket 名称映射 */
FName ABaseWeapon::GetSocketNameForSlot(EBaseAttachmentSlot Slot) const
{
	switch (Slot)
	{
	case EBaseAttachmentSlot::Muzzle:		return MuzzleSocket;
	case EBaseAttachmentSlot::Optic:		return OpticSocket;
	case EBaseAttachmentSlot::Tactical:	return TacticalSocket;
	case EBaseAttachmentSlot::Foregrip:	return ForegripSocket;
	case EBaseAttachmentSlot::Magazine:	return MagazineSocket;
	default:								return NAME_None;
	}
}

int32 ABaseWeapon::GetMaxAmmo() const
{
	return GetEffectiveMagazineSize();
}

// ================================================================
//  配件系统 — 装备、移除、查询
// ================================================================

void ABaseWeapon::EquipAttachment(EBaseAttachmentSlot Slot, TSubclassOf<ABaseWeaponAttachment> AttachmentClass)
{
	if (!AttachmentClass) return;

	// 先移除旧配件（如果存在）
	RemoveAttachment(Slot);

	// 生成配件 Actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseWeaponAttachment* NewAttachment = GetWorld()->SpawnActor<ABaseWeaponAttachment>(AttachmentClass, SpawnParams);
	if (!NewAttachment) return;

	// 挂载到 FP 武器模型的对应 Socket
	const FName SocketName = GetSocketNameForSlot(Slot);
	FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);
	NewAttachment->AttachToComponent(FirstPersonMesh, AttachRules, SocketName);

	// FP 专属显示（仅本地玩家可见）
	NewAttachment->GetAttachmentMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	NewAttachment->GetAttachmentMesh()->bOnlyOwnerSee = true;

	EquippedAttachments.Add(Slot, NewAttachment);

	// 蓝图事件：配件装备后（音效、特效等）
	NewAttachment->BP_OnEquipped(GetOwner());

	// 配件可能改变弹匣容量 → 刷新 HUD
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
	}
}

void ABaseWeapon::RemoveAttachment(EBaseAttachmentSlot Slot)
{
	TObjectPtr<ABaseWeaponAttachment> Existing;
	if (EquippedAttachments.RemoveAndCopyValue(Slot, Existing) && Existing)
	{
		Existing->BP_OnUnequipped(GetOwner());
		Existing->Destroy();

		if (WeaponOwner)
		{
			WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
		}
	}
}

ABaseWeaponAttachment* ABaseWeapon::GetAttachment(EBaseAttachmentSlot Slot) const
{
	const TObjectPtr<ABaseWeaponAttachment>* Found = EquippedAttachments.Find(Slot);
	return Found ? Found->Get() : nullptr;
}

bool ABaseWeapon::HasAttachment(EBaseAttachmentSlot Slot) const
{
	return EquippedAttachments.Contains(Slot);
}

void ABaseWeapon::ClearAllAttachments()
{
	TArray<EBaseAttachmentSlot> Slots;
	EquippedAttachments.GetKeys(Slots);
	for (const EBaseAttachmentSlot& Slot : Slots)
	{
		RemoveAttachment(Slot);
	}
}

// ================================================================
//  GetEffective* — 遍历所有配件，汇总修正后的实际值
//
//  模式说明：
//    - 倍率属性（伤害/后坐力/散布/换弹时间）：基础值 × ∏配件倍率
//    - 加法属性（弹匣容量）：基础值 + Σ配件加成
//    - 配件倍率默认 1.0（不影响），补偿器 < 1.0（减后坐力），增程枪管 > 1.0（增伤害）
// ================================================================

float ABaseWeapon::GetEffectiveDamage() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetDamageMultiplier();
		}
	}
	return HitDamage * Mult;
}

float ABaseWeapon::GetEffectiveRecoil() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetRecoilMultiplier();
		}
	}
	return FiringRecoil * Mult;
}

float ABaseWeapon::GetEffectiveRecoilInterpSpeed() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetRecoilInterpMultiplier();
		}
	}
	return RecoilInterpSpeed * Mult;
}

float ABaseWeapon::GetEffectiveRecoilRecoverySpeed() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetRecoilRecoveryMultiplier();
		}
	}
	return RecoilRecoverySpeed * Mult;
}

float ABaseWeapon::GetEffectiveRecoilMaxAccumulation() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetRecoilMaxAccumulationMultiplier();
		}
	}
	return RecoilMaxAccumulation * Mult;
}

float ABaseWeapon::GetEffectiveAimVariance() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetAimVarianceMultiplier();
		}
	}
	return AimVariance * Mult;
}

float ABaseWeapon::GetEffectiveReloadTime() const
{
	float Mult = 1.0f;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Mult *= Pair.Value->GetReloadTimeMultiplier();
		}
	}
	return ReloadTime * Mult;
}

int32 ABaseWeapon::GetEffectiveMagazineSize() const
{
	// 弹匣容量使用加法修正（扩容弹匣 +N 发）
	int32 Bonus = 0;
	for (const auto& Pair : EquippedAttachments)
	{
		if (Pair.Value)
		{
			Bonus += Pair.Value->GetMagazineSizeBonus();
		}
	}
	return MagazineSize + Bonus;
}

// ================================================================
//  换弹辅助方法 — 由 GA_WeaponReload 的计时 Task 调用
// ================================================================

/**
 * 弹匣掉落：
 *   服务端：生成 ABaseDroppedMagazine 物理 Actor
 *   所有客户端：左手显示替换弹匣 + 武器弹匣槽配件隐藏
 */
void ABaseWeapon::DropMagazine()
{
	const FName SocketName = GetSocketNameForSlot(EBaseAttachmentSlot::Magazine);
	const FVector SpawnLocation = FirstPersonMesh->GetSocketLocation(SocketName);
	// 给掉落弹匣一个随机的初始速度（模拟甩出感）
	const FVector SpawnVelocity = FirstPersonMesh->GetComponentVelocity()
		+ FVector(FMath::FRandRange(-50.0f, 50.0f), FMath::FRandRange(-50.0f, 50.0f), FMath::FRandRange(-100.0f, -50.0f));

	// 优先使用弹匣配件的模型，否则用武器默认弹匣模型
	UStaticMesh* MeshToDrop = DroppedMagazineMesh;
	ABaseWeaponAttachment* MagAttachment = GetAttachment(EBaseAttachmentSlot::Magazine);
	if (MagAttachment && MagAttachment->GetAttachmentMesh()->GetStaticMesh())
	{
		MeshToDrop = MagAttachment->GetAttachmentMesh()->GetStaticMesh();
	}

	// 服务端生成物理弹匣 Actor
	if (MeshToDrop && HasAuthority())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ABaseDroppedMagazine* DroppedMag = GetWorld()->SpawnActor<ABaseDroppedMagazine>(
			ABaseDroppedMagazine::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

		if (DroppedMag)
		{
			DroppedMag->Initialize(MeshToDrop, SpawnVelocity);
			// 掉落的弹匣不碰撞 Pawn（避免推走角色）
			DroppedMag->GetMagazineMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}
	}

	// 广播到所有客户端：左手显示替换弹匣
	MulticastDropMagazineVisuals(MeshToDrop);
}

void ABaseWeapon::InsertMagazine()
{
	MulticastInsertMagazineVisuals();
}

void ABaseWeapon::CancelReloadVisuals()
{
	MulticastCancelReloadVisuals();
}

// ================================================================
//  换弹 Multicast RPC 实现
// ================================================================

/** 所有客户端：左手显示替换弹匣 + 隐藏武器弹匣槽配件 */
void ABaseWeapon::MulticastDropMagazineVisuals_Implementation(UStaticMesh* Mesh)
{
	// 左手挂载替换弹匣到 FP 手臂的左手骨骼
	USceneComponent* ArmMesh = FirstPersonMesh->GetAttachParent();
	if (ArmMesh && Mesh)
	{
		LeftHandMagazineMesh->SetStaticMesh(Mesh);
		LeftHandMagazineMesh->SetVisibility(true);

		const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);
		LeftHandMagazineMesh->AttachToComponent(ArmMesh, AttachRules, LeftHandSocketName);
	}

	// 隐藏武器弹匣槽上的配件模型（看起来弹匣被取走了）
	ABaseWeaponAttachment* MagAttachment = GetAttachment(EBaseAttachmentSlot::Magazine);
	if (MagAttachment)
	{
		MagAttachment->GetAttachmentMesh()->SetVisibility(false);
	}
}

/** 所有客户端：隐藏左手弹匣 + 恢复武器弹匣槽 */
void ABaseWeapon::MulticastInsertMagazineVisuals_Implementation()
{
	LeftHandMagazineMesh->SetVisibility(false);
	// 将左手弹匣模型移回武器 Root（等待下次换弹使用）
	LeftHandMagazineMesh->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
	LeftHandMagazineMesh->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false), NAME_None);

	// 恢复武器弹匣槽配件可见
	ABaseWeaponAttachment* MagAttachment = GetAttachment(EBaseAttachmentSlot::Magazine);
	if (MagAttachment)
	{
		MagAttachment->GetAttachmentMesh()->SetVisibility(true);
	}
}

/** 所有客户端：取消换弹 → 清理左手模型状态 */
void ABaseWeapon::MulticastCancelReloadVisuals_Implementation()
{
	LeftHandMagazineMesh->SetVisibility(false);
	LeftHandMagazineMesh->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
	LeftHandMagazineMesh->AttachToComponent(RootComponent, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false), NAME_None);
}
