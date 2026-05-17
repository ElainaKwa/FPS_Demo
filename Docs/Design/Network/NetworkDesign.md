# 网络同步注意事项

本文档记录 UE 5.7 + GAS FPS 项目中网络同步的设计决策、已知陷阱和推荐模式。目标是避免同类 bug 反复出现。

---

## 1. 网络架构总览

### 1.1 Listen Server 模式

本项目使用 **Listen Server** 架构：主机同时充当客户端和服务器。这意味着：

- 主机上的 Server RPC **即时执行**（不走网络，同一帧内完成）
- 主机上 `HasAuthority()` 始终为 true
- 主机上的 GAS 能力在服务器侧运行，客户端侧通过 RPC 触发
- Multicast RPC 在主机上也会执行（服务器是客户端之一）

### 1.2 ASC 复制模式

```cpp
// BaseCharacter.cpp:27-28
AbilitySystemComponent->SetIsReplicated(true);
AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
```

**Mixed 模式**的含义：
- GameplayEffect 完整复制到所有客户端
- GameplayTag 复制到所有客户端
- GameplayCue 复制到所有客户端
- 属性变更通过 AttributeSet 的 `ReplicatedUsing` 回调复制

### 1.3 ServerOnly 能力执行策略

所有武器能力使用 `NetExecutionPolicy = ServerOnly`：

```cpp
// GA_WeaponFire / GA_WeaponReload 构造函数
NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
```

**含义**：能力仅在服务器激活，客户端通过 Server RPC 转发输入。客户端**不应**直接调用 `TryActivateAbility`。

---

## 2. RPC 使用规范

### 2.1 本项目的 RPC 分类

| 类型 | 用途 | 示例 |
|------|------|------|
| Server RPC | 客户端输入转发 | `ServerStartFiring`, `ServerReload`, `ServerSwitchWeapon` |
| NetMulticast | 视觉表现同步 | `MulticastPlayFiringMontage`, `MulticastPlayReloadMontage`, `MulticastAddWeaponRecoil` |

### 2.2 可靠性选择

本项目所有 RPC 均为 **Reliable**。对于高频 RPC（如连发开火），应考虑改为 Unreliable 以减少带宽。当前实现可接受，因为开火 RPC 只在按下/松开时发送，不是每帧触发。

### 2.3 Listen Server 下的 RPC 特殊行为

在 Listen Server 上，Server RPC 由主机客户端直接在服务器上执行（无网络延迟）。这意味着：

```cpp
// PlayerCharacter.cpp:147-151
void APlayerCharacter::OnReload()
{
    UE_LOG(LogTemp, Warning, TEXT("[Input] OnReload 客户端按下换弹键"));
    ServerReload();  // Listen Server: 同帧执行 ServerReload_Implementation
}
```

**注意**：`ETriggerEvent::Triggered` 在 Listen Server 上会导致输入事件每帧触发。对于单次动作（如换弹），必须使用 `ETriggerEvent::Started`：

```cpp
// PlayerCharacter.cpp:92 — 正确
EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &APlayerCharacter::OnReload);
```

### 2.4 RPC 安全检查

Server RPC 实现中必须验证调用者合法性：

```cpp
void APlayerCharacter::ServerReload_Implementation()
{
    // 当前实现没有验证调用者，理想情况应添加：
    // if (!HasAuthority()) return;
    // 实际上 Server RPC 本身只在服务器执行，但防御性检查不会有害
}
```

---

## 3. GAS 能力网络同步

### 3.1 Authority 守卫

**每个 ServerOnly 能力的 `ActivateAbility` 必须以 Authority 守卫开头**：

```cpp
// GA_WeaponFire.cpp:23-27 / GA_WeaponReload.cpp:44-48
void UGA_WeaponFire::ActivateAbility(...)
{
    if (!K2_HasAuthority())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // ...
}
```

**原因**：`NetExecutionPolicy = ServerOnly` 不完全阻止客户端收到 `ActivateAbility` 调用。在特定情况下（如预测键存在），客户端可能收到调用但无权限执行。守卫确保安全退出。

### 3.2 能力激活链路

标准的输入→能力激活链路：

```
客户端按下 R
  → OnReload()
  → ServerReload() (Server RPC)
  → ServerReload_Implementation()
  → ASC->TryActivateAbility(ReloadHandle)
  → GAS: CanActivateAbility() → InternalActivateAbility() → ActivateAbility()
```

**关键原则**：客户端永远不直接调用 `TryActivateAbility`，所有能力激活走 Server RPC。

### 3.3 CanActivateAbility 替代 ActivationBlockedTags ⚠️

**严重陷阱**：C++ 构造函数中设置的 `ActivationBlockedTags` 会被 Blueprint 子类默认值**覆盖为空**。

```cpp
// ❌ 错误方式 — BP 会覆盖为空
UGA_WeaponReload::UGA_WeaponReload()
{
    ActivationBlockedTags.AddTag(BaseGameplayTags::State_Reloading);  // BP 子类中此值为空！
}

// ✅ 正确方式 — 重写 CanActivateAbility
bool UGA_WeaponReload::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;

    if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        if (ASC->HasMatchingGameplayTag(BaseGameplayTags::State_Reloading))
            return false;
    }
    return true;
}
```

**要点**：
- `CanActivateAbility` 是 C++ 虚函数，不受 BP 属性覆盖影响
- 使用 `ActorInfo->AbilitySystemComponent.Get()`（非 `GetAbilitySystemComponentFromActorInfo()`，后者是 non-const 方法）
- 必须先调用 `Super::CanActivateAbility` 保留框架检查（冷却、消耗等）
- 将阻止逻辑放在 `CanActivateAbility` 而非 `ActivateAbility`，避免"死激活"问题（见第 9 节）

### 3.4 标签手动管理替代 ActivationOwnedTags ⚠️

同样的问题：`ActivationOwnedTags` 也会被 BP 覆盖。当 BP 中该容器为空时，能力结束时 GAS 不会移除标签，导致标签卡死。

```cpp
// ❌ 错误方式 — 依赖 ActivationOwnedTags 自动管理
ActivationOwnedTags.AddTag(BaseGameplayTags::State_Reloading);  // BP 覆盖后，结束时标签不移除

// ✅ 正确方式 — 手动管理标签生命周期
void UGA_WeaponReload::ActivateAbility(...)
{
    // ... CommitAbility 之后
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->AddLooseGameplayTag(BaseGameplayTags::State_Reloading);
    }
    // ...
}

void UGA_WeaponReload::EndAbility(...)
{
    // ... 清理之前
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(BaseGameplayTags::State_Reloading);
    }
    Super::EndAbility(...);
}
```

**关键**：
- `AddLooseGameplayTag` / `RemoveLooseGameplayTag` 是显式管理，不依赖 UPROPERTY 容器
- Loose Tag 仍然通过 ASC 正常复制到客户端（Mixed 模式下）
- 添加在 `CommitAbility` 之后，移除在 `Super::EndAbility` 之前
- `EndAbility` 中的标签移除**必须**同时在 `bWasCancelled=true` 和 `false` 路径执行

### 3.5 InstancedPerActor 与 ServerOnly 的交互

本项目所有能力使用 `InstancingPolicy = InstancedPerActor`：

```cpp
InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
```

**含义**：每个 Actor 只有一个能力实例。如果能力已激活，`TryActivateAbility` 会因"能力正在运行"而失败。这是防止重复激活的第二道防线（`CanActivateAbility` 是第一道）。

**注意**：`InstancedPerActor` 的能力实例在能力授予时创建，不是每次激活时创建。这意味着实例上的属性值在多次激活之间保持。

---

## 4. 属性复制与同步

### 4.1 AttributeSet 的 ReplicatedUsing 回调

所有 AttributeSet 属性使用 `ReplicatedUsing` 和 `GAMEPLAYATTRIBUTE_REPNOTIFY`：

```cpp
// BaseWeaponAttributeSet.h
UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentAmmo)
FGameplayAttributeData CurrentAmmo;

// BaseWeaponAttributeSet.cpp
void UBaseWeaponAttributeSet::OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UBaseWeaponAttributeSet, CurrentAmmo, OldValue);
    // 更新 HUD
}
```

**复制条件**：使用 `COND_None` + `REPNOTIFY_Always`：

```cpp
// BaseWeaponAttributeSet.cpp:19
DOREPLIFETIME_CONDITION_NOTIFY(UBaseWeaponAttributeSet, CurrentAmmo, COND_None, REPNOTIFY_Always);
```

`REPNOTIFY_Always` 确保即使服务器推送的值与客户端当前值相同，`OnRep` 回调也会触发。这对 HUD 同步至关重要。

### 4.2 双路复制策略

弹药数据通过两条独立的复制路径同步：

| 路径 | 属性 | 复制方式 | 权威性 |
|------|------|---------|--------|
| 武器 | `ABaseWeapon::CurrentBullets` | `DOREPLIFETIME` + `OnRep_CurrentBullets` | 本地值镜像 |
| AttributeSet | `UBaseWeaponAttributeSet::CurrentAmmo` | GAS Mixed 模式 + `OnRep_CurrentAmmo` | 权威复制值 |

**AttributeSet 是权威复制值**，`Weapon.CurrentBullets` 是本地镜像。两条路径确保即使其中一条失败，HUD 仍能更新。

### 4.3 弹药消耗顺序 ⚠️

弹药消耗必须遵循严格顺序：

```cpp
// GA_WeaponFire.cpp:145-156 — 正确顺序
Weapon->CurrentBullets = FMath::Max(0, Weapon->CurrentBullets - 1);  // 1. 先减本地值

if (AmmoCostEffectClass)
{
    FGameplayEffectSpecHandle AmmoSpec = MakeOutgoingGameplayEffectSpec(AmmoCostEffectClass, GetAbilityLevel());
    ApplyGameplayEffectSpecToOwner(..., AmmoSpec);  // 2. 再应用 GE
}

if (ASC)
{
    ASC->SetNumericAttributeBase(..., static_cast<float>(Weapon->CurrentBullets));  // 3. 最后同步 AttributeSet
}
```

**原因**：如果 GE 失败（如效果类未配置），本地值已经扣减，不会出现"GE 失败导致弹药不消耗"的问题。

### 4.4 SyncAttributeSetFromWeapon 的调用时机

```cpp
// BaseCharacter.cpp:91-101
void ABaseCharacter::SyncAttributeSetFromWeapon() const
{
    if (!WeaponAttributeSet || !CurrentWeapon) return;
    const int32 MaxAmmo = CurrentWeapon->GetEffectiveMagazineSize();
    WeaponAttributeSet->SetMaxAmmo(static_cast<float>(MaxAmmo));
    WeaponAttributeSet->SetCurrentAmmo(static_cast<float>(CurrentWeapon->CurrentBullets));
}
```

**调用时机**：
- 武器切换时（`OnRep_CurrentWeapon`、`SwitchToWeapon`）
- 武器添加时（`AddWeapon`）

**不调用时机**：弹药消耗时（此时由能力自行同步 AttributeSet，避免重复）

### 4.5 PostGameplayEffectExecute 中的 Authority 检查

```cpp
// BaseWeaponAttributeSet.cpp:27-29
void UBaseWeaponAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    if (Data.Target.GetOwnerActor() && !Data.Target.GetOwnerActor()->HasAuthority())
    {
        return;  // 客户端不执行属性修改逻辑
    }
    // ...
}
```

**原因**：GE 在 Mixed 模式下会复制到客户端，`PostGameplayEffectExecute` 可能在客户端被调用。不检查 Authority 会导致属性修改逻辑双重执行。

---

## 5. GameplayTag 网络

### 5.1 Loose Tag 的复制行为

`AddLooseGameplayTag` / `RemoveLooseGameplayTag` 添加的标签通过 ASC 的 Mixed 复制模式自动复制到客户端。"Loose" 仅表示不由 GameplayEffect 管理，不影响复制。

### 5.2 标签引用计数陷阱 ⚠️

GAS 内部对标签使用引用计数。当同一标签被多次添加（如多个能力同时添加 `State.Reloading`），计数递增；移除时只递减一次。如果添加/移除不配对，标签会卡死。

**解决方案**：对状态标签使用 `AddLooseGameplayTag` / `RemoveLooseGameplayTag` 显式管理，保证每个 `Add` 有且仅有一个对应 `Remove`：

```cpp
// ActivateAbility 中添加（仅一次）
ASC->AddLooseGameplayTag(BaseGameplayTags::State_Reloading);

// EndAbility 中移除（仅一次，无论取消还是完成）
ASC->RemoveLooseGameplayTag(BaseGameplayTags::State_Reloading);
```

`CanActivateAbility` 中的阻止检查确保不会出现第二次添加（能力不会在标签存在时被激活）。

### 5.3 State.Reloading 卡死案例复盘

**Bug 现象**：`State.Reloading` 永久卡在 ASC 上，所有后续换弹失败。

**根因链**：
1. C++ 构造函数设置 `ActivationOwnedTags.AddTag(State_Reloading)` 和 `ActivationBlockedTags.AddTag(State_Reloading)`
2. Blueprint 子类的 CDO 默认值将这两个容器**覆盖为空**
3. 某次换弹激活：GAS 添加了 `State.Reloading`（来自 BP 中可能保留的 `ActivationOwnedTags`）
4. 换弹结束时：GAS 检查 `ActivationOwnedTags`（空）决定移除哪些标签 → **不移除任何标签**
5. `State.Reloading` 卡死
6. 后续换弹：`TryActivateAbility` 成功（`ActivationBlockedTags` 为空），但 `ActivateAbility` 中手动检查发现标签存在 → 调用 `EndAbility` → GAS 又添加 `State.Reloading`（`ActivationOwnedTags` 仍不为空的部分情况）→ `EndAbility` 只移除一个引用 → 卡死持续

**修复**：见第 3.3、3.4 节，用 `CanActivateAbility` + Loose Tag 手动管理替代 UPROPERTY 容器。

---

## 6. Blueprint 与 C++ 交互的坑

### 6.1 UPROPERTY 容器被 BP 默认值覆盖 ⚠️⚠️⚠️

**这是本项目最严重的网络陷阱。** 以下 UPROPERTY 容器在 C++ 构造函数中设置的值会被 Blueprint 子类默认值覆盖：

| 容器 | 影响 | 安全替代方案 |
|------|------|------------|
| `ActivationBlockedTags` | 能力无法阻止重复激活 | 重写 `CanActivateAbility` |
| `ActivationOwnedTags` | 能力结束时标签不移除 | `AddLooseGameplayTag` / `RemoveLooseGameplayTag` |
| `AbilityTriggers` | 事件触发能力不生效 | 在 BP 中手动配置触发器 |

**原因**：UE 序列化 BP CDO 时，空容器会覆盖 C++ 构造函数中填充的容器。这不是 bug，而是 UE 的设计：BP 序列化值优先于 C++ 构造函数。

**规则**：不要在 C++ 构造函数中向 `FGameplayTagContainer` 类型的 UPROPERTY 添加值。如需在 C++ 中设置，使用 `PostInitProperties` 或虚函数重写。

### 6.2 构造函数中设置 UPROPERTY 的值对 BP 子类无效

更广泛的规则：**C++ 构造函数中设置的 UPROPERTY 值，在 BP 子类实例化时可能被覆盖**。这适用于所有 UPROPERTY，不只是容器。

安全做法：
- 简单值（int、float、bool）：通常安全，因为 BP 只在显式修改时覆盖
- 容器值（TArray、TMap、TSet、FGameplayTagContainer）：**不安全**，BP CDO 默认为空容器，会覆盖 C++ 构造函数的填充

---

## 7. 武器与角色复制

### 7.1 CurrentWeapon 的 OnRep 回调

```cpp
// BaseCharacter.h
UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
TObjectPtr<ABaseWeapon> CurrentWeapon;

// BaseCharacter.cpp:211-219
void ABaseCharacter::OnRep_CurrentWeapon()
{
    if (CurrentWeapon && AbilitySystemComponent)
    {
        AbilitySystemComponent->SetCurrentWeapon(CurrentWeapon);
        SyncAttributeSetFromWeapon();
        UpdateWeaponHUD(CurrentWeapon->CurrentBullets, CurrentWeapon->GetEffectiveMagazineSize());
    }
}
```

**注意**：`OnRep_CurrentWeapon` 不接收旧值参数（未声明 `ABaseWeapon* OldWeapon`），因此无法在回调中比较新旧武器。如需比较，需手动缓存旧值。

### 7.2 OwnedWeapons 不复制

```cpp
// BaseCharacter.h — 不带 Replicated
TArray<TObjectPtr<ABaseWeapon>> OwnedWeapons;
```

`OwnedWeapons` 仅在服务器存在。客户端不知道角色拥有哪些武器。这简化了网络流量，但意味着客户端代码不能依赖 `OwnedWeapons`。

### 7.3 WeaponOwner 在客户端可能为 nullptr ⚠️

```cpp
// BaseWeapon.cpp:52-59
void ABaseWeapon::OnRep_CurrentBullets()
{
    if (!WeaponOwner)
    {
        WeaponOwner = Cast<IBaseWeaponHolder>(GetOwner());  // 防御：重新获取
    }
    if (WeaponOwner)
    {
        WeaponOwner->UpdateWeaponHUD(CurrentBullets, GetEffectiveMagazineSize());
    }
}
```

**原因**：`WeaponOwner` 是 `GetOwner()` 转换的结果。在客户端，`GetOwner()` 可能因 Owner Actor 尚未同步而返回 nullptr。`OnRep_CurrentBullets` 可能在 Owner 同步完成前触发，因此必须做 nullptr 检查并重新获取。

### 7.4 武器切换时的 CancelAllAbilities 与标签清理

```cpp
// PlayerCharacter.cpp:218-220
void APlayerCharacter::ServerSwitchWeapon_Implementation()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->CancelAllAbilities();  // 取消所有能力（包括换弹）
    }
    // ...
}
```

`CancelAllAbilities` 会触发每个活跃能力的 `EndAbility(bWasCancelled=true)`。如果使用 Loose Tag 手动管理，`EndAbility` 中的 `RemoveLooseGameplayTag` 会正确清理标签。如果依赖 `ActivationOwnedTags`（可能为空），标签不会被清理。

---

## 8. UE 5.7 网络相关 API 陷阱

### 8.1 UAbilityTask_WaitDelay 委托名

```cpp
// ✅ 正确
DropTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnDropMagazine);

// ❌ 错误 — 没有 OnFinished
DropTask->OnFinished.AddDynamic(this, &UGA_WeaponReload::OnDropMagazine);
```

UE 5.7 中 `UAbilityTask_WaitDelay` 的完成委托名为 `OnFinish`，不是 `OnFinished`。

### 8.2 AbilityTags 已废弃

```cpp
// ❌ 已废弃
AbilityTags = FGameplayTagContainer(BaseGameplayTags::Ability_Reload);

// ✅ 使用 SetAssetTags
SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Reload));
```

### 8.3 ApplyGameplayEffectSpecToTarget 参数

```cpp
// ❌ 错误理解 — 最后参数不是 ASC 指针
ApplyGameplayEffectSpecToTarget(Handle, ActorInfo, ActivationInfo, Spec, TargetASC);

// ✅ 正确方式 — 对目标 ASC 使用 ApplyGameplayEffectSpecToSelf
TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
```

### 8.4 类重命名需添加 ActiveClassRedirects

在 `Config/DefaultEngine.ini` 中添加重定向，否则旧版本保存的地图/资产引用会断裂：

```ini
[/Script/Engine.Engine]
+ActiveClassRedirects=(OldName="/Script/MyFps_Demo.OldClassName",NewName="/Script/MyFps_Demo.NewClassName")
```

---

## 9. 常见错误模式（反面教材）

### 9.1 在 ActivateAbility 中做应属于 CanActivateAbility 的检查 → 死激活

```cpp
// ❌ 错误：在 ActivateAbility 中检查阻止条件
void UGA_WeaponReload::ActivateAbility(...)
{
    // GAS 已经通过了 CanActivateAbility，已经开始激活流程
    // ActivationOwnedTags 已被添加到 ASC
    if (ASC->HasMatchingGameplayTag(State_Reloading))
    {
        EndAbility(...);  // "死激活"：能力被激活又立即取消
        return;
    }
}
```

**问题**：
1. GAS 在调用 `ActivateAbility` 前已经添加了 `ActivationOwnedTags`，标签引用计数已增加
2. `EndAbility` 只移除当前激活的标签引用，残留引用使标签卡死
3. 即使没有引用计数问题，"激活→取消"循环也是浪费

**正确做法**：在 `CanActivateAbility` 中阻止，能力永远不会被激活。

### 9.2 依赖 ActivationOwnedTags 自动清理 → BP 覆盖后标签卡死

见第 3.4 节和第 5.3 节。核心问题：BP 子类可能将 `ActivationOwnedTags` 覆盖为空，GAS 结束能力时无法移除标签。

### 9.3 客户端直接调用 TryActivateAbility

```cpp
// ❌ 错误：客户端直接调用
void APlayerCharacter::OnReload()
{
    AbilitySystemComponent->TryActivateAbility(ReloadHandle);  // 客户端无权限！
}
```

**正确做法**：通过 Server RPC 转发到服务器执行。

### 9.4 AttributeSet PostGameplayEffectExecute 不检查 Authority

```cpp
// ❌ 错误：不检查 Authority
void UBaseWeaponAttributeSet::PostGameplayEffectExecute(...)
{
    SetCurrentAmmo(FMath::Clamp(GetCurrentAmmo(), 0.0f, GetMaxAmmo()));
    // 客户端也会执行此逻辑 → 双重 Clamp、HUD 双重更新
}

// ✅ 正确：检查 Authority
void UBaseWeaponAttributeSet::PostGameplayEffectExecute(...)
{
    if (Data.Target.GetOwnerActor() && !Data.Target.GetOwnerActor()->HasAuthority())
        return;
    SetCurrentAmmo(FMath::Clamp(GetCurrentAmmo(), 0.0f, GetMaxAmmo()));
}
```

---

## 10. 推荐模式（正面模板）

### 10.1 Authority 守卫模板

```cpp
void UGA_YourAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!K2_HasAuthority())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // ... 能力逻辑
}
```

### 10.2 标签手动管理模板

```cpp
// 头文件
virtual bool CanActivateAbility(...) const override;

// 构造函数 — 不设置 ActivationOwnedTags / ActivationBlockedTags
UGA_YourAbility::UGA_YourAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    // 不在这里设置 ActivationOwnedTags / ActivationBlockedTags
}

// CanActivateAbility — C++ 代码阻止，不受 BP 覆盖
bool UGA_YourAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;

    if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        if (ASC->HasMatchingGameplayTag(YourBlockingTag))
            return false;
    }
    return true;
}

// ActivateAbility — CommitAbility 后添加标签
void UGA_YourAbility::ActivateAbility(...)
{
    // Authority 守卫 + 其他检查...

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->AddLooseGameplayTag(YourStateTag);
    }

    // ... 能力逻辑
}

// EndAbility — 无论取消还是完成，都移除标签
void UGA_YourAbility::EndAbility(...)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(YourStateTag);
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

### 10.3 Server RPC → TryActivateAbility 流程模板

```cpp
// 客户端输入
void APlayerCharacter::OnYourAction()
{
    ServerYourAction();
}

// Server RPC 实现
void APlayerCharacter::ServerYourAction_Implementation()
{
    if (!AbilitySystemComponent) return;

    // 1. 取消冲突能力
    AbilitySystemComponent->CancelConflictingAbility();

    // 2. 防御性检查：能力状态标签
    if (AbilitySystemComponent->HasMatchingGameplayTag(BlockingTag))
    {
        return;  // 已在执行，跳过
    }

    // 3. 获取能力句柄
    FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GetYourAbilityHandle();
    if (!Handle.IsValid())
    {
        AbilitySystemComponent->GrantDefaultAbilities();
        Handle = AbilitySystemComponent->GetYourAbilityHandle();
    }

    // 4. 激活能力
    if (Handle.IsValid())
    {
        AbilitySystemComponent->TryActivateAbility(Handle);
    }
}
```

### 10.4 属性双路同步模板

```cpp
// 在能力中修改属性时，同时更新两条路径
void UGA_YourAbility::ModifyAmmo(ABaseWeapon* Weapon, int32 NewValue)
{
    // 1. 更新本地镜像
    Weapon->CurrentBullets = NewValue;

    // 2. 应用 GE（如适用）
    if (AmmoEffectClass)
    {
        FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(AmmoEffectClass, GetAbilityLevel());
        ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Spec);
    }

    // 3. 同步 AttributeSet（权威复制值）
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->SetNumericAttributeBase(
            UBaseWeaponAttributeSet::GetCurrentAmmoAttribute(),
            static_cast<float>(NewValue));
    }

    // 4. 更新 HUD
    if (Weapon->WeaponOwner)
    {
        Weapon->WeaponOwner->UpdateWeaponHUD(Weapon->CurrentBullets, Weapon->GetEffectiveMagazineSize());
    }
}
```

---

## 附录：相关文件索引

| 文件 | 关键内容 |
|------|---------|
| `Source/MyFps_Demo/BaseCharacter.cpp` | ASC 初始化、Mixed 模式、武器复制、OnRep 回调 |
| `Source/MyFps_Demo/BaseCharacter.h` | CurrentWeapon 复制声明、OwnedWeapons |
| `Source/MyFps_Demo/PlayerCharacter.cpp` | Server RPC、Multicast RPC、输入绑定 |
| `Source/MyFps_Demo/GameAbilitySystem/Abilities/Reload/GA_WeaponReload.cpp` | CanActivateAbility、Loose Tag 管理 |
| `Source/MyFps_Demo/GameAbilitySystem/Abilities/Fire/GA_WeaponFire.cpp` | 弹药消耗顺序、属性双路同步 |
| `Source/MyFps_Demo/GameAbilitySystem/BaseWeaponAttributeSet.cpp` | OnRep 回调、PostGameplayEffectExecute Authority 检查 |
| `Source/MyFps_Demo/GameAbilitySystem/BaseAbilitySystemComponent.cpp` | GiveAbility、CancelFireAbility |
| `Source/MyFps_Demo/Weapons/BaseWeapon.cpp` | CurrentBullets 复制、WeaponOwner nullptr 防御 |
| `Docs/Design/Reload/ReloadDesign.md` | 换弹系统 bug 历史与解决方案 |
| `Docs/Design/Fire/FireDesign.md` | 开火系统 bug 历史与解决方案 |
