# 换弹能力设计文档

## 1. 设计目标

将换弹逻辑从 `ABaseWeapon::StartReload()` / `CancelReload()` / `OnReloadComplete()` 迁移到 GAS 的 `UGA_WeaponReload` 中：

- **异步编排**：弹匣弹出 → 装填 → 完成，通过 `WaitDelay` Task 链实现，替代原始 `TimerManager`
- **状态控制**：换弹期间 `State.Reloading` 标签阻塞开火，结束或取消自动移除
- **自动换弹**：弹药归零时通过 `HandleGameplayEvent(Event.OutOfAmmo)` + `AbilityTriggers` 自动触发
- **强中断**：切枪或玩家主动换弹时立即终止当前开火能力

## 2. 架构设计

### 2.1 能力层级

```
UBaseGameplayAbility
  └── UGA_WeaponReload
```

### 2.2 数据流

```
触发源 (手动按R / 弹药归零事件)
  │
  ▼
ABaseCharacter::OnReload()
  ├── CancelFireAbility()  ← 强制终止开火
  ├── Handle 无效 → GrantDefaultAbilities() 重试
  └── TryActivateAbility(ReloadHandle)
        │
        ▼
GA_WeaponReload::ActivateAbility()
  ├── Weapon 为 null → 退出
  ├── CurrentBullets >= MagazineSize → 退出 (满弹)
  ├── CommitAbility()
  │   └── ActivationOwnedTags 添加 State.Reloading → 阻塞开火
  ├── PlayReloadMontage()
  │
  ├── [并发] WaitDelay(MagazineDropDelay) → OnDropMagazine()
  │   ├── ABaseWeapon::DropMagazine()
  │   │   ├── Spawn ABaseDroppedMagazine (物理弹匣)
  │   │   └── 左手持替换弹匣模型
  │   └── 碰撞设置: ECC_Pawn = Ignore (防止弹匣撞角色)
  │
  ├── [并发] WaitDelay(EffectiveTime - MagazineInsertBeforeEnd) → OnInsertMagazine()
  │   └── ABaseWeapon::InsertMagazine()
  │       └── 隐藏左手弹匣, 恢复枪体弹匣模型
  │
  └── [并发] WaitDelay(EffectiveTime) → OnReloadComplete()
        ├── ApplyGameplayEffectSpecToOwner (GE_ReloadAmmo) → 补充弹药
        ├── 同步: Weapon.CurrentBullets = AttributeSet.CurrentAmmo
        ├── UpdateWeaponHUD()
        └── EndAbility → 移除 State.Reloading
```

### 2.3 取消流程

```
换弹中按切枪键 / 再次按 R
  │
  ▼
CancelFireAbility() + CancelAbilities(Reload)
  │
  ▼
GA_WeaponReload::EndAbility(bWasCancelled = true)
  ├── Weapon.bIsReloading = false
  └── Weapon.CancelReloadVisuals()
      └── 隐藏左手弹匣 → 恢复到武器根组件
```

## 3. 关键代码

### 3.1 构造函数 — 能力注册 + 自动触发

```cpp
UGA_WeaponReload::UGA_WeaponReload()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Reload));
    ActivationOwnedTags.AddTag(BaseGameplayTags::State_Reloading);

    // 弹药归零事件自动触发换弹
    FAbilityTriggerData TriggerData;
    TriggerData.TriggerTag = BaseGameplayTags::Event_OutOfAmmo;
    TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(TriggerData);
}
```

### 3.2 计时编排 — 三个并发 WaitDelay

```cpp
void UGA_WeaponReload::ActivateAbility(...)
{
    // ... 前置检查 ...

    const float EffectiveTime = Weapon->GetEffectiveReloadTime();

    // 弹匣掉落
    UAbilityTask_WaitDelay* DropTask = UAbilityTask_WaitDelay::WaitDelay(
        this, Weapon->MagazineDropDelay);
    DropTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnDropMagazine);
    DropTask->ReadyForActivation();

    // 弹匣装填 (从开始时算起)
    const float InsertTime = FMath::Max(0.0f, EffectiveTime - Weapon->MagazineInsertBeforeEnd);
    UAbilityTask_WaitDelay* InsertTask = UAbilityTask_WaitDelay::WaitDelay(
        this, InsertTime);
    InsertTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnInsertMagazine);
    InsertTask->ReadyForActivation();

    // 换弹完成
    UAbilityTask_WaitDelay* CompleteTask = UAbilityTask_WaitDelay::WaitDelay(
        this, EffectiveTime);
    CompleteTask->OnFinish.AddDynamic(this, &UGA_WeaponReload::OnReloadComplete);
    CompleteTask->ReadyForActivation();
}
```

### 3.3 开火中断

```cpp
// BaseAbilitySystemComponent.cpp
void UBaseAbilitySystemComponent::CancelFireAbility()
{
    if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(FireAbilityHandle))
    {
        if (Spec->IsActive())
            CancelAbilitySpec(*Spec, nullptr);  // protected → 子类可访问
    }
}

// BaseCharacter.cpp
void ABaseCharacter::OnReload()
{
    AbilitySystemComponent->CancelFireAbility();  // 先中断开火
    // ... 再激活换弹
}
```

## 4. 踩过的坑

| 问题 | 根因 | 解决 |
|------|------|------|
| 换弹后开火不中断 | `CancelAbilities(&FireTag)` 依赖 Tag 匹配，但 UE 5.7 API 迁移中 `AbilityTags`/`AssetTags` 不一致 | 新增 `CancelFireAbility()` 公开方法，内部直接通过句柄定位 Fire Spec 并调 protected `CancelAbilitySpec` |
| 满弹时换弹被拦截（正确行为）但首次开火后仍无法换弹 | 弹药消耗只在 GE fallback 分支执行，GE 分支不更新 `Weapon->CurrentBullets` | 统一为 "先减 CurrentBullets，GE + AttributeSet 同步为附加" |
| 弹匣掉落碰撞角色导致位移 | DropMagazine 使用 `PhysicsActor` 预设 | 掉落时设 `SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore)` |
| `LeftHandMagazineMesh` 私有无法外部访问 | 换弹取消逻辑移到能力后，能力无法访问武器私有成员 | 新增 `ABaseWeapon::CancelReloadVisuals()` 公开方法 |
| 弹药归零不自动换弹 | `HandleGameplayEvent` 分发事件不走 `RegisterGameplayTagEvent` 路径 | 在 `GA_WeaponReload` 构造函数添加 `AbilityTriggers`，源类型设为 `GameplayEvent` |
| 换弹能力完全不激活 | `HasAuthority()` 检查 + `Handle.IsValid()` 均为 false | 移除 `HasAuthority`，句柄无效时重试 `GrantDefaultAbilities` |

## 5. 配置文件

### 5.1 需要的 Blueprint 资产

- `BP_GA_Reload`（继承 `GA_WeaponReload`）
  - `ReloadAmmoEffectClass` → `GE_ReloadAmmo`
- `GE_ReloadAmmo`：Instant, Modifier: `CurrentAmmo`, Add, SetByCaller

### 5.2 角色 BP 配置

- `AbilitySystemComponent` → `Reload Ability Class` = `BP_GA_Reload`
- 输入：`ReloadAction` 绑定 `OnReload`(Triggered)

### 5.3 DefaultEngine.ini

换弹取消逻辑不用额外配置。切枪时的 `CancelAllAbilities()` 自动触发 `EndAbility(bWasCancelled=true)`。
