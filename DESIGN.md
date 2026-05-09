# FPS_Demo 项目设计文档

## 1. 项目概述

FPS_Demo 是一个基于 Unreal Engine 5.7 的第一人称射击（FPS）游戏原型项目，采用 Gameplay Ability System（GAS）作为武器系统的核心架构。

```
FPS_Demo.uproject
├── Source/
│   └── MyFps_Demo/        ← 主模块，GAS 武器系统
└── Content/
```

---

## 2. 模块详解

### 2.1 MyFps_Demo（主模块）

**定位**: 项目核心模块，承载 GAS 武器系统。

**技术栈**:

| 技术 | 用途 |
|------|------|
| Gameplay Ability System (GAS) | 开火/换弹能力的激活、执行、取消、冷却管理 |
| Enhanced Input | 统一的输入映射与触发式输入处理 |
| UMG / Slate | HUD 弹药计数器、生命值等 UI 控件 |
| Native GameplayTags | 状态标记（State.Reloading）与事件广播（Event.OutOfAmmo） |
| GameplayEffect | 弹药消耗、弹药补充、伤害数值的运行时修改 |
| AttributeSet | 可网络复制的弹药属性（CurrentAmmo、MaxAmmo） |
| AbilityTask | 异步等待输入释放、计时延迟等能力逻辑编排 |

**模块依赖** (`MyFps_Demo.Build.cs`):
```
Core, CoreUObject, Engine, InputCore, EnhancedInput,
GameplayAbilities, GameplayTasks, GameplayTags
```

**目录结构**:
```
MyFps_Demo/
├── BaseCharacter.h/.cpp           # 角色：ASC + AttributeSet + 武器持有者接口
├── BaseGameMode.h/.cpp
├── BasePlayerController.h/.cpp
├── Weapons/
│   ├── BaseWeapon.h/.cpp           # 武器（数据容器 + 可视化）
│   ├── BaseWeaponHolder.h/.cpp     # 武器持有者接口（IBaseWeaponHolder）
│   ├── BaseWeaponAttachment.h/.cpp # 配件（属性修改器）
│   └── BaseDroppedMagazine.h/.cpp  # 弹匣物理（换弹时掉落）
└── GameAbilitySystem/
    ├── BaseAbilitySystemComponent.h/.cpp  # 自定义 ASC
    ├── BaseWeaponAttributeSet.h/.cpp      # 弹药属性集
    ├── BaseGameplayTags.h/.cpp            # 原生 GameplayTag 定义
    └── Abilities/
        ├── Fire/
        │   └── GA_WeaponFire.h/.cpp     # 开火能力
        └── Reload/
            └── GA_WeaponReload.h/.cpp   # 换弹能力
```

#### 2.1.1 GAS 核心组件

**AbliitySystemComponent (`UBaseAbilitySystemComponent`)**

角色 Created 时作为子对象创建，`BeginPlay` 中调用 `InitAbilityActorInfo(this, this)` 初始化。持有当前武器引用，负责在武器切换时更新能力可访问的武器数据。在服务器端通过 `GrantDefaultAbilities()` 授予 Fire 和 Reload 能力 Spec。

**AttributeSet (`UBaseWeaponAttributeSet`)**

仅包含两个弹药属性：
- `CurrentAmmo`（float，网络复制，钳制范围 [0, MaxAmmo]）
- `MaxAmmo`（float，网络复制，最低 1.0）

通过 GameplayEffect 进行修改：
- 开火：`GE_AmmoCost` → `CurrentAmmo -= 1`
- 换弹：`GE_ReloadAmmo` → `CurrentAmmo += MagazineSize`（PreAttributeChange 自动钳制）

**GameplayTags**

| Tag | 类型 | 作用 |
|-----|------|------|
| `Ability.Fire` | 能力标签 | 标记开火能力 |
| `Ability.Reload` | 能力标签 | 标记换弹能力 |
| `State.Reloading` | 状态标签 | 激活中时阻塞开火，同时阻塞自身重复激活 |
| `Event.OutOfAmmo` | 事件标签 | 弹药耗尽时广播，触发自动换弹 |
| `Data.Damage` | 数据标签 | SetByCaller Magnitude 标签，传递伤害数值 |

#### 2.1.2 能力详细设计

**GA_WeaponFire（开火能力）**

```
能力策略: InstancedPerActor（每个角色一个实例，复用）
阻塞标签: State.Reloading（换弹期间不可开火）
输入方式: AbilityLocalInputPressed(InputID::Fire)

ActivateAbility()
  ├─ CommitAbility() → 检查阻塞标签 & 属性消耗
  ├─ 启动 WaitInputRelease 任务（监听松开开火键）
  └─ PerformFire() → 执行一次射击

PerformFire()
  ├─ 从 AttributeSet 读取 CurrentAmmo → ≤0 则广播 Event.OutOfAmmo 并结束
  ├─ 从武器获取 MuzzleLocation / TargetLocation / Spread
  ├─ LineTrace 检测命中
  │   ├─ 命中目标有 ASC → ApplyGameplayEffectSpecToSelf (GE_Damage)
  │   └─ 命中目标无 ASC → 回退到 ApplyDamage（兼容旧 NPC）
  ├─ 播放 FiringMontage / 施加后坐力 / 更新 HUD
  ├─ ApplyGameplayEffectSpecToOwner (GE_AmmoCost) → 消耗弹药
  ├─ 同步 Weapon.CurrentBullets ← AttributeSet.GetCurrentAmmo()
  ├─ 剩余弹药 ≤ 0 → 广播 Event.OutOfAmmo & 结束
  ├─ 松开开火键 → 结束
  └─ 未松开 & 全自动 → WaitDelay(RefireRate) → OnRefireReady() → PerformFire() 循环
      未松开 & 半自动 → WaitDelay(RefireRate) → OnRefireReady() → 结束（需重新按下）
```

**GA_WeaponReload（换弹能力）**

```
能力策略: InstancedPerActor
激活时施加: State.Reloading（阻塞标签，也阻塞自身二次激活）
输入方式: AbilityLocalInputPressed(InputID::Reload)

ActivateAbility()
  ├─ 校验: 武器有效 / 弹药未满 / CommitAbility 成功
  ├─ 设置 Weapon.bIsReloading = true
  ├─ 播放 ReloadMontage
  ├─ 并发启动三个 WaitDelay 任务:
  │   ├─ WaitDelay(MagazineDropDelay) → OnDropMagazine()
  │   ├─ WaitDelay(EffectiveTime - MagazineInsertBeforeEnd) → OnInsertMagazine()
  │   └─ WaitDelay(EffectiveTime) → OnReloadComplete()

OnReloadComplete()
  ├─ ApplyGameplayEffectSpecToOwner (GE_ReloadAmmo) → 补充弹药
  ├─ 同步 Weapon.CurrentBullets ← AttributeSet.GetCurrentAmmo()
  ├─ 更新 HUD
  └─ EndAbility → 移除 State.Reloading 标签

EndAbility (被取消时)
  ├─ 设置 Weapon.bIsReloading = false
  └─ Weapon.CancelReloadVisuals() → 清理左手弹匣模型
```

---

## 3. 设计模式

### 3.1 策略模式（Strategy）— 能力系统

```
Ability（策略接口）
  ├── GA_WeaponFire（具体策略：开火）
  └── GA_WeaponReload（具体策略：换弹）
```

每种武器行为被封装为独立的 `UGameplayAbility` 子类。角色持有 ASC，通过 `GrantAbility()` 注册策略，运行时通过 `TryActivateAbility()` 执行。这使得添加新能力（如 ADS 瞄准、近战）只需创建新的 UGameplayAbility 子类，无需修改角色或武器代码。

### 3.2 数据驱动模式 — 武器作为数据提供者

```
角色（行为宿主）
  ↓ IBaseWeaponHolder 接口
武器（纯数据 + 可视化）
  ├── 基础属性: MagazineSize, HitDamage, RefireRate, ...
  └── 有效属性: GetEffectiveDamage() = HitDamage × 配件乘数
```

`ABaseWeapon` 不再包含任何行为逻辑（开火/换弹）。它仅提供：
- **数据**: 通过 Editable UPROPERTY 配置，运行时通过 `GetEffective*()` 方法汇总配件加成
- **可视化**: 骨骼网格、弹匣物理、模型可见性切换
- **辅助方法**: `GetMuzzleLocation()`, `CalculateSpread()`, `DropMagazine()`

能力通过 ASC 获取当前武器引用，读取其数据来驱动行为。武器切换时，ASC 更新引用，能力行为即刻反映新武器的属性。

### 3.3 接口隔离模式 — IBaseWeaponHolder

```
IBaseWeaponHolder
  ├── GetWeaponTargetLocation()   → 瞄准目标（用于弹道计算）
  ├── PlayFiringMontage()         → 播放开火动画
  ├── PlayReloadMontage()        → 播放换弹动画
  ├── AddWeaponRecoil()          → 施加屏幕后坐力
  ├── UpdateWeaponHUD()          → 刷新 UI
  ├── AttachWeaponMeshes()       → 挂载武器骨骼网格
  └── GetCurrentWeapon()         → 获取当前武器
```

角色和武器之间的通信完全通过接口进行。能力不需要知道角色的具体类型 —— 它仅依赖接口方法。这允许未来将武器系统扩展到 AI 或载具上，只需实现同样的接口即可。

### 3.4 观察者模式 — Tag 事件驱动

```
GA_WeaponFire                     ABaseCharacter
   │                                  │
   ├── HandleGameplayEvent ────────→ 注册 Event.OutOfAmmo 监听
   │   (Event.OutOfAmmo)              │
   │                                  └── OnReload() → 激活换弹能力
```

当弹药耗尽时，开火能力广播 `Event.OutOfAmmo` GameplayTag 事件。角色在 `BeginPlay` 中注册了该事件的监听器，收到事件后自动触发换弹能力。这是松耦合的事件驱动通信 —— 开火能力不知道谁在监听，也不知道监听者会做什么。

### 3.5 组件模式 — ASC + AttributeSet

```
ABaseCharacter
  ├── UBaseAbilitySystemComponent  ← 能力管理
  └── UBaseWeaponAttributeSet      ← 属性存储
```

GAS 的核心设计模式：AbilitySystemComponent（能力管理）和 AttributeSet（属性存储）作为 Actor 的组件存在。这种分离使得：
- 属性可以被多个能力并行修改（通过 GE 叠加）
- 属性修改自动网络复制
- 能力之间通过 Tag 进行阻塞/协同（如换弹时阻塞开火）

---

## 4. 数据流

### 4.1 开火完整流程

```
玩家按下鼠标左键
  │
  ▼
InputAction (FireAction, ETriggerEvent::Started)
  │
  ▼
ABaseCharacter::OnStartFiring()
  │
  ▼
ASC->TryActivateAbility(FireHandle)
  │
  ▼
ASC 查找 Handle 对应的 AbilitySpec
  │
  ├─ 检查 ActivationBlockedTags (State.Reloading)
  │   └─ 如果正在换弹 → 阻塞，不激活（也通过 ActivateAbility 手动检查兜底）
  │
  ▼
GA_WeaponFire::ActivateAbility()
  │
  ├─ CommitAbility() ── 检查成本/冷却
  │
  ├─ 创建 WaitInputRelease Task ── 监听松开开火键
  │
  ▼
PerformFire()
  │
  ├─ 读取 AttributeSet → CurrentAmmo ≤ 0?
  │   └─ 是 → 广播 Event.OutOfAmmo → EndAbility
  │
  ├─ 从 Weapon 读取数据:
  │   MuzzleLocation, TargetLocation, Spread, Damage
  │
  ├─ LineTrace (从枪口沿弹道方向)
  │
  ├─ 命中检测:
  │   ├─ 目标有 ASC → ApplyGameplayEffectSpecToSelf (GE_Damage, SetByCaller: Data.Damage)
  │   └─ 目标无 ASC → UGameplayStatics::ApplyDamage()
  │
  ├─ PlayFiringMontage() ── 开火动画
  ├─ AddWeaponRecoil()    ── 镜头后坐力
  │
  ├─ 先 --Weapon.CurrentBullets（不依赖 GE）
  ├─ ApplyGameplayEffectSpecToOwner (GE_AmmoCost) → 消费弹药（附加）
  ├─ ASC->SetNumericAttributeBase → 同步 AttributeSet
  │
  ├─ UpdateWeaponHUD() ── 刷新 UI
  │
  ├─ 弹药归零?
  │   └─ 是 → 广播 Event.OutOfAmmo → EndAbility
  │
  ├─ 松开开火键?
  │   └─ 是 → EndAbility
  │
  └─ 未松开 & bFullAuto?
      ├─ 是 → WaitDelay(RefireRate) → OnRefireReady → PerformFire() 循环
      └─ 否 → WaitDelay(RefireRate) → EndAbility (需重新按下)

玩家松开鼠标左键
  │
  ▼
ASC->AbilityLocalInputReleased(EAbilityInputID::Fire)
  │
  ▼
WaitInputRelease Task 触发 OnInputReleased
  │
  └─ bInputReleased = true → 下一次 PerformFire 检测到后结束
```

### 4.2 换弹完整流程

```
玩家按下 R 键 / Event.OutOfAmmo 触发
  │
  ▼
ABaseCharacter::OnReload()
  ├─ ASC->CancelFireAbility() ── 强制中断开火
  └─ ASC->TryActivateAbility(ReloadHandle)
  │
  ▼
GA_WeaponReload::ActivateAbility()
  │
  ├─ 校验: 武器存在 / 弹药未满 / CommitAbility
  │
  ├─ 施加 ActivationOwnedTags → State.Reloading (阻塞开火 & 二次换弹)
  │
  ├─ PlayReloadMontage()
  │
  ├─ 并发启动三个计时任务:
  │   │
  │   ├─ [0.15s] WaitDelay(MagazineDropDelay)
  │   │   └─ OnDropMagazine()
  │   │       ├─ 生成 ABaseDroppedMagazine（物理弹匣）
  │   │       └─ 左手持替换弹匣
  │   │
  │   ├─ [1.6s] WaitDelay(EffectiveTime - MagazineInsertBeforeEnd)
  │   │   └─ OnInsertMagazine()
  │   │       └─ 将弹匣插回枪体，隐藏左手模型
  │   │
  │   └─ [2.0s] WaitDelay(EffectiveTime)
  │       └─ OnReloadComplete()
  │           ├─ ApplyGE (ReloadAmmoEffect) → CurrentAmmo = MaxAmmo
  │           ├─ 同步 Weapon.CurrentBullets
  │           ├─ UpdateWeaponHUD()
  │           └─ EndAbility → 移除 State.Reloading

[切枪时取消]
  │
  ▼
ASC->CancelAllAbilities()
  │
  └─ GA_WeaponReload::EndAbility(bWasCancelled=true)
      ├─ Weapon.bIsReloading = false
      └─ Weapon.CancelReloadVisuals() ── 清理左手弹匣模型
```

### 4.3 武器切换流程

```
玩家按下切换键
  │
  ▼
ABaseCharacter::OnSwitchWeapon()
  │
  ├─ OwnedWeapons.Num ≤ 1 → 跳过
  │
  ├─ ASC->CancelAllAbilities() ── 取消所有活动能力（开火/换弹）
  │
  ├─ CurrentWeapon->DeactivateWeapon()
  │   └─ SetActorHiddenInGame(true)
  │
  ├─ 更新 CurrentWeapon = OwnedWeapons[nextIndex]
  │
  ├─ CurrentWeapon->ActivateWeapon()
  │   └─ SetActorHiddenInGame(false)
  │
  ├─ ASC->SetCurrentWeapon(NewWeapon) ── 更新能力可访问的武器引用
  │
  └─ SyncAttributeSetFromWeapon()
      ├─ AttributeSet.MaxAmmo = Weapon.GetEffectiveMagazineSize()
      └─ AttributeSet.CurrentAmmo = Weapon.CurrentBullets
```

---

## 5. 类图

```
┌──────────────────────────────────────────────────────────────┐
│                    UAbilitySystemComponent                    │
│                         (Engine)                              │
└──────────────────────────┬───────────────────────────────────┘
                           │ 继承
           ┌───────────────▼──────────────────┐
           │   UBaseAbilitySystemComponent       │
           │   ├─ CurrentWeapon (ABaseWeapon*) │
           │   ├─ FireAbilityHandle             │
           │   ├─ ReloadAbilityHandle           │
           │   └─ GrantDefaultAbilities()       │
           └───────────────────────────────────┘
                           │ 持有
           ┌───────────────▼──────────────────┐
           │          ABaseCharacter          │
           │  implements IAbilitySystemInterface│
           │  implements IBaseWeaponHolder     │
           │  ├─ AbilitySystemComponent         │
           │  ├─ WeaponAttributeSet             │
           │  ├─ CurrentWeapon                  │
           │  └─ OwnedWeapons[]                 │
           └────────┬──────────────────────────┘
                    │ 持有
    ┌───────────────▼──────────────────┐
    │          ABaseWeapon             │
    │  (数据容器 + 可视化)              │
    │  ├─ MagazineSize, HitDamage...    │
    │  ├─ FirstPersonMesh               │
    │  ├─ GetMuzzleLocation()           │
    │  ├─ CalculateSpread()             │
    │  ├─ DropMagazine()                │
    │  ├─ InsertMagazine()              │
    │  └─ CancelReloadVisuals()         │
    └──────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│                     IBaseWeaponHolder                       │
│  ├─ GetWeaponTargetLocation()                               │
│  ├─ PlayFiringMontage()                                     │
│  ├─ PlayReloadMontage()                                     │
│  ├─ AddWeaponRecoil()                                       │
│  ├─ UpdateWeaponHUD()                                       │
│  ├─ AttachWeaponMeshes()                                    │
│  └─ GetCurrentWeapon()                                      │
└────────────────────────────────────────────────────────────┘

┌─────────────────────────────┐  ┌─────────────────────────────┐
│      UGA_WeaponFire          │  │     UGA_WeaponReload         │
│  extends UBaseGameplayAbility│  │  extends UBaseGameplayAbility│
│  ├─ DamageEffectClass        │  │  ├─ ReloadAmmoEffectClass    │
│  ├─ AmmoCostEffectClass      │  │  └─ 计时异步任务             │
│  └─ 全自动/半自动循环        │  │                              │
└─────────────────────────────┘  └─────────────────────────────┘
```

---

## 6. 扩展指南

### 6.1 添加新能力（如 ADS 瞄准）

1. 在 `GameAbilitySystem/Abilities/ADS/` 创建 `GA_WeaponADS.h/.cpp`
2. 继承 `UGameplayAbility`，设置 `InstancingPolicy = InstancedPerActor`
3. 定义所需 GameplayTag（如 `Ability.ADS`、`State.ADS`）
4. 在 `UBaseAbilitySystemComponent.h` 中添加 `EAbilityInputID::ADS` 枚举值
5. 在 `GrantDefaultAbilities()` 中授予能力 Spec
6. 在 `ABaseCharacter::SetupPlayerInputComponent()` 中绑定输入

### 6.2 添加新属性（如武器射速）

1. 在 `UBaseWeaponAttributeSet` 中添加 `FGameplayAttributeData FireRate` 和对应的 `ATTRIBUTE_ACCESSORS`
2. 在 `PreAttributeChange()` 中添加钳制逻辑
3. 添加 `GetLifetimeReplicatedProps` 网络复制
4. 创建 `GE_FireRate` Blueprint 资源来修改射速
5. 在 `GA_WeaponFire` 中通过 AttributeSet 读取射速而非直接从 Weapon 读取

### 6.3 添加新武器类型

1. 创建新的 `ABaseWeapon` Blueprint 子类
2. 配置 UPROPERTY：MagazineSize、HitDamage、RefireRate、FiringRecoil、AimVariance 等
3. 为 FiringMontage 和 ReloadMontage 指定动画
4. 设置 `bFullAuto = true/false` 控制射击模式
5. 配置 MuzzleSocketName、附件插槽等

能力代码无需修改 —— 它们通过接口和属性读取来自动适配不同武器。

---

## 7. 构建说明

1. 确保安装了 UE 5.7
2. 右键 `FPS_Demo.uproject` → **Generate Visual Studio project files**
3. 打开 `FPS_Demo.sln`，在 Visual Studio 中构建 **Development Editor** 配置
4. 或在命令行中运行:
   ```
   Engine\Build\BatchFiles\Build.bat FPS_DemoEditor Win64 Development -Project="FPS_Demo.uproject"
   ```
5. **在编辑器中配置**:
   - 为 `UBaseAbilitySystemComponent` 指定 `FireAbilityClass` 和 `ReloadAbilityClass` Blueprint 子类
   - 创建 `GE_Damage`、`GE_AmmoCost`、`GE_ReloadAmmo` GameplayEffect Blueprint 资源
   - 将这些 GE 资源分配到对应能力的 `EffectClass` 属性

---

## 8. 注意事项
- **能力类不存在于编辑器**：`GA_WeaponFire` 和 `GA_WeaponReload` 需要在编辑器中创建 Blueprint 子类才能被 ASC 引用（`TSubclassOf<UGameplayAbility>` 属性需要通过 Blueprint 资产来配置）。
- **GameplayEffect 是 Blueprint 资产**：伤害、弹药消耗、弹药补充的 GE 需要在编辑器中创建，因为 UE 的 GameplayEffect 通常定义在 Blueprint 中。
- **网络复制**：AttributeSet 已标记 `Replicated`，ASC 设置为 `Mixed` 复制模式。属性修改通过 GE 自动网络同步。
- **AttributeSet 与 Weapon.CurrentBullets 的同步**：开火/换弹能力在修改 AttributeSet 后会立即同步到 `Weapon.CurrentBullets`，确保 HUD 等依赖 Weapon 数据的代码仍能正常工作。
