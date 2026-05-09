# 开火能力设计文档

## 1. 设计目标

将开火逻辑从 `ABaseWeapon::Fire()` 迁移到 GAS 的 `UGA_WeaponFire` 中，实现以下目标：

- **解耦**：武器（`ABaseWeapon`）退化为数据容器 + 可视化，行为由能力驱动
- **扩展性**：新增武器类型无需修改武器或角色代码，只需配置数据
- **标准化**：弹药消耗通过 GameplayEffect 修改 AttributeSet，伤害通过 GE 传递
- **可组合**：换弹阻塞、自动换弹等控制通过 GameplayTag 实现松耦合

## 2. 架构设计

### 2.1 能力层级

```
UGameplayAbility (Engine)
  └── UBaseGameplayAbility (自定义基类)
        ├── UGA_WeaponFire (开火)
        └── UGA_WeaponReload (换弹)
```

`UBaseGameplayAbility` 统一设置 `InstancingPolicy = InstancedPerActor`，每个角色持有一个实例。

### 2.2 数据流

```
玩家按下鼠标左键
  │
  ▼
ABaseCharacter::OnStartFiring()
  │
  ▼
ASC->TryActivateAbility(FireHandle)
  │
  ▼
GA_WeaponFire::ActivateAbility()
  ├── 检查 State.Reloading → 有则退出
  ├── CommitAbility()
  ├── 启动 WaitInputRelease Task（监听松开）
  └── PerformFire() → 执行一次射击
        ├── 读取武器数据: MuzzleLocation, Spread, Damage
        ├── LineTrace 命中检测
        │   ├── 目标有 ASC → ApplyGameplayEffectSpecToSelf (GE_Damage)
        │   └── 目标无 ASC → TakeDamage (回退兼容)
        ├── 播放蒙太奇, 施加后坐力, 更新 HUD
        ├── CurrentBullets -= 1
        ├── 弹药 GE (如果配置)
        ├── SetNumericAttributeBase (同步 AttributeSet)
        ├── 弹药归零 → HandleGameplayEvent(Event.OutOfAmmo) → 自动换弹
        └── 全自动 → WaitDelay(RefireRate) → OnRefireReady() → 循环
            半自动 → WaitDelay(RefireRate) → EndAbility
```

### 2.3 核心决策

| 决策 | 原因 |
|------|------|
| `InstancedPerActor` | 需要维护 `bInputReleased` 状态，同时避免每次开火创建新实例 |
| 弹药消耗先在 Weapon 上 `--CurrentBullets`，GE 作为附加 | 防止 GE 未配置时弹药无法消耗，导致换弹永远被拦截 |
| `Ability_Fire` AssetTag 设在 CDO，非 Spec | UE 5.7 中 `DynamicAbilityTags` 已废弃，`SetAssetTags` 是推荐方式 |
| 松开开火用 `AbilityLocalInputReleased` | `WaitInputRelease` Task 需要此信号；不可用 `CancelAbilities` 否则全自动循环被强制中断 |
| 换弹时开火中断用 `CancelAbilitySpec(FireSpec)` | Tag 匹配因 API 迁移问题不可靠；直接通过句柄定位 Fire Spec 并取消 |

## 3. 关键代码

### 3.1 构造函数 — 能力注册

```cpp
UGA_WeaponFire::UGA_WeaponFire()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    SetAssetTags(FGameplayTagContainer(BaseGameplayTags::Ability_Fire));
}
```

### 3.2 激活 — 换弹阻塞检查

```cpp
void UGA_WeaponFire::ActivateAbility(...)
{
    // 手动检查换弹状态（ActivationBlockedTags 被 BP 覆盖不可靠）
    if (GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(
            BaseGameplayTags::State_Reloading))
    {
        EndAbility(...);
        return;
    }
    // ...
}
```

### 3.3 弹药消耗 — GE + 兜底

```cpp
// 始终先扣武器弹药
Weapon->CurrentBullets = FMath::Max(0, Weapon->CurrentBullets - 1);

// GE 作为附加（如果配置了）
if (AmmoCostEffectClass)
{
    auto Spec = MakeOutgoingGameplayEffectSpec(AmmoCostEffectClass, Level);
    ApplyGameplayEffectSpecToOwner(...);
}

// 同步 AttributeSet
ASC->SetNumericAttributeBase(
    UBaseWeaponAttributeSet::GetCurrentAmmoAttribute(),
    static_cast<float>(Weapon->CurrentBullets));
```

### 3.4 全自动循环

```cpp
void UGA_WeaponFire::OnRefireReady()
{
    // 松开或非全自动 → 结束
    if (!bInputReleased && GetWeapon() && GetWeapon()->bFullAuto)
        PerformFire();   // 继续循环
    else
        EndAbility(...);
}
```

## 4. 踩过的坑

| 问题 | 根因 | 解决 |
|------|------|------|
| 换弹时仍能开火 | C++ 构造函数在 `ActivationBlockedTags` 设了 `State_Reloading`，但 BP 子类 CDO 覆盖为空 | 移除 C++ 侧 `ActivationBlockedTags` 赋值，改为 `ActivateAbility` 里手动 `HasMatchingGameplayTag` |
| 换弹键按了无反应 | 弹药消耗 GE 不生效 → `CurrentBullets` 不减少 → 换弹判断 `CurrentBullets >= MagazineSize` 始终成立 | 弹药消耗逻辑改为先减 `CurrentBullets`，GE 作为附加 |
| `OnFinished` 不存在 | UE 5.7 `UAbilityTask_WaitDelay` 委托名为 `OnFinish` | 统一用 `OnFinish` |
| `ApplyGameplayEffectSpecToTarget` 参数不匹配 | 第 5 参数应为 `FGameplayAbilityTargetDataHandle&` | 改为 `TargetASC->ApplyGameplayEffectSpecToSelf()` |
| `TryActivateAbilitiesByTag` 不生效 | UE 5.7 中 `TryActivateAbilitiesByTag` 查旧 `AbilityTags`，而我们用新 `SetAssetTags` | 回退到 `TryActivateAbility(Handle)` 句柄激活 |

## 5. 配置文件

### 5.1 需要的 Blueprint 资产

- `BP_GA_Fire`（继承 `GA_WeaponFire`）
  - `DamageEffectClass` → `GE_Damage`
  - `AmmoCostEffectClass` → `GE_AmmoCost`
- `GE_Damage`：Instant, SetByCaller `Data.Damage`
- `GE_AmmoCost`：Instant, Modifier: `CurrentAmmo`, Add, -1.0

### 5.2 角色 BP 配置

- `AbilitySystemComponent` → `Fire Ability Class` = `BP_GA_Fire`
- 输入：`FireAction` 绑定 `OnStartFiring`(Started) / `OnStopFiring`(Completed)
