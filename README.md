# FPS_Demo

基于 Unreal Engine 5.7 开发

> **不论何时，忽略 `Source/FPS_Demo/` 中的内容。** 该目录为遗留代码，不参与当前架构，不进行维护。

## 概述

FPS_Demo 是一个采用 Gameplay Ability System（GAS）实现武器机制的第一人称射击项目，核心模块为 **`MyFps_Demo`**。

## 架构 (`MyFps_Demo`)

### GAS 核心组件

| 组件 | 类 | 职责 |
|------|------|------|
| ASC | `UBaseAbilitySystemComponent` | 角色持有，管理能力、效果和标签 |
| AttributeSet | `UBaseWeaponAttributeSet` | 弹药属性（`CurrentAmmo`、`MaxAmmo`），支持网络复制 |
| GameplayTags | `BaseGameplayTags` | 原生标签，用于能力激活、状态阻塞和事件广播 |

### 能力

| 能力 | 类 | GameplayTag | 描述 |
|------|-------|-------------|------|
| 开火 | `UGA_WeaponFire` | `Ability.Fire` | 即时命中检测，支持全自动/半自动；从武器读取散布、伤害、射速数据；通过 GE 造成伤害和消耗弹药 |
| 换弹 | `UGA_WeaponReload` | `Ability.Reload` | 计时换弹序列（弹匣掉落/插入物理），通过 GE 补充弹药 |

### GameplayTags

| 标签 | 用途 |
|-----|------|
| `Ability.Fire` | 开火能力激活标签 |
| `Ability.Reload` | 换弹能力激活标签 |
| `State.Firing` | 开火中阻塞（预留） |
| `State.Reloading` | 换弹中，阻塞开火能力激活 |
| `Event.OutOfAmmo` | 弹药归零时广播 |
| `Data.Damage` | 伤害 GE 的 SetByCaller 数值标签 |

### 武器作为数据提供者

`ABaseWeapon` 不再包含任何开火/换弹状态机。其职责为：
- **数据容器**：弹匣容量、伤害、散布、后坐力、射速等
- **可视化管理**：第一/第三人称网格、配件插槽
- **辅助方法**：`GetMuzzleLocation()`、`CalculateSpread()`、`DropMagazine()`、`InsertMagazine()`
- **配件系统**：伤害、后坐力、散布、换弹时间、弹匣容量的附加强化

### 数据流

```
Input (按下开火键)
  → ABaseCharacter::OnStartFiring()
    → ASC->AbilityLocalInputPressed(EAbilityInputID::Fire)
      → UGA_WeaponFire::ActivateAbility()
        → 读取武器数据（伤害、散布、枪口位置）
        → 执行即时命中检测
        → 对目标应用伤害 GE（或回退到 ApplyDamage）
        → 对自身应用弹药消耗 GE
        → 播放蒙太奇、施加后坐力、通过 IBaseWeaponHolder 更新 HUD
        → 全自动：等待射速间隔后循环
        → 弹药归零：广播 Event.OutOfAmmo

Input (按下换弹键)
  → ASC->AbilityLocalInputPressed(EAbilityInputID::Reload)
    → UGA_WeaponReload::ActivateAbility()
      → 施加 State.Reloading 标签（阻塞开火）
      → 播放换弹蒙太奇
      → 计时序列：弹出弹匣 → 插入弹匣 → 通过 GE 补充弹药
      → 取消时（切枪）：EndAbility 移除 State.Reloading，清理弹匣模型
```

### 武器持有者接口

`IBaseWeaponHolder` 为能力提供与角色通信的通道：
- `GetWeaponTargetLocation()` — 瞄准目标位置（用于即时命中检测）
- `PlayFiringMontage()` / `PlayReloadMontage()` — 动画播放
- `AddWeaponRecoil()` — 镜头俯仰后坐力
- `UpdateWeaponHUD()` — UI 弹药计数
- `GetCurrentWeapon()` — 武器数据访问

## 目录结构

```
Source/MyFps_Demo/
├── MyFps_Demo.Build.cs
├── BaseCharacter.h/.cpp              # 角色（ASC + AttributeSet）
├── BaseGameMode.h/.cpp
├── BasePlayerController.h/.cpp
├── Weapons/
│   ├── BaseWeapon.h/.cpp             # 武器（数据 + 可视化，无逻辑）
│   ├── BaseWeaponHolder.h/.cpp       # 武器持有者接口
│   ├── BaseWeaponAttachment.h/.cpp   # 配件属性修改器
│   └── BaseDroppedMagazine.h/.cpp    # 换弹时掉落的物理弹匣
└── GameAbilitySystem/
    ├── BaseAbilitySystemComponent.h/.cpp # 自定义 ASC
    ├── BaseWeaponAttributeSet.h/.cpp     # 弹药属性集
    ├── BaseGameplayTags.h/.cpp           # 原生 GameplayTag 定义
    └── Abilities/
        ├── Fire/
        │   ├── GA_WeaponFire.h         # 开火能力
        │   └── GA_WeaponFire.cpp
        └── Reload/
            ├── GA_WeaponReload.h       # 换弹能力
            └── GA_WeaponReload.cpp
```

## 构建

1. 右键 `FPS_Demo.uproject` → **Generate Visual Studio project files**
2. 打开 `FPS_Demo.sln` 并构建
3. 必需引擎模块（已配置于 `MyFps_Demo.Build.cs`）：
   - `GameplayAbilities`、`GameplayTasks`、`GameplayTags`

## 扩展指南

### 添加新能力

1. 在 `GameAbilitySystem/Abilities/<子文件夹名>/` 创建继承 `UGameplayAbility` 的类
2. 设置 `InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor`
3. 使用 `BaseGameplayTags` 定义激活和阻塞标签
4. 在 `UBaseAbilitySystemComponent::GrantDefaultAbilities()` 中授予能力，指定唯一的 `EAbilityInputID`
5. 在 `ABaseCharacter::SetupPlayerInputComponent()` 中绑定输入

### 添加新属性

1. 在 `UBaseWeaponAttributeSet` 中添加 `FGameplayAttributeData` 属性和 `ATTRIBUTE_ACCESSORS` 宏
2. 在 `PreAttributeChange()` 中实现钳制逻辑
3. 创建 `UGameplayEffect`（Blueprint 子类）修改该属性
4. 在对应能力中引用该 GE 类

### 添加瞄准（ADS）能力示例

```cpp
// GA_WeaponADS.h
UCLASS()
class UGA_WeaponADS : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_WeaponADS()
    {
        InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    }
    // ...
};

// BaseAbilitySystemComponent.cpp - 授予能力
ADSAbilityHandle = GiveAbility(FGameplayAbilitySpec(
    ADSAbilityClass, 1, static_cast<int32>(EAbilityInputID::ADS), this
));
```

## 文档同步

> 更新项目代码时，必须同步更新 `DESIGN.md`，确保架构变更、新增模块、数据流变化等都能在文档中准确反映。文档与代码保持一致性。
