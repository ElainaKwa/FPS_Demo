# 后坐力系统设计文档

## 1. 设计目标

将后坐力从 PlayerCharacter 的临时实现重构为独立的 **URecoilComponent**，并支持**每件武器独立配置后坐力曲线参数**，通过配件倍率修正。

- **解耦**：后坐力逻辑从 PlayerCharacter 剥离到独立组件，Player 仅持有引用
- **武器差异化**：不同武器可配置不同的上跳幅度、上升速度、恢复速度和累积上限
- **配件可组合**：每个后坐力参数都有对应的配件倍率，遵循现有 `GetEffective*()` 模式
- **平滑手感**：Tick 驱动的 FInterpTo 插值，替代原先的瞬间 Pitch 跳变

## 2. 架构设计

### 2.1 三层职责分离

```
┌─────────────────────────────────────────────────┐
│  ABaseWeapon（数据层）                            │
│  ├── FiringRecoil              ← 单发上跳幅度     │
│  ├── RecoilInterpSpeed         ← 上升插值速度     │
│  ├── RecoilRecoverySpeed       ← 停火恢复速度     │
│  └── RecoilMaxAccumulation     ← 连射累积上限     │
│                                                   │
│  + GetEffective*() 遍历配件倍率                    │
└──────────────────────┬──────────────────────────┘
                       │ 每次开火传入 4 个参数
                       ▼
┌─────────────────────────────────────────────────┐
│  ABaseWeaponAttachment（修正层）                  │
│  ├── RecoilMultiplier                           │
│  ├── RecoilInterpMultiplier                     │
│  ├── RecoilRecoveryMultiplier                   │
│  └── RecoilMaxAccumulationMultiplier            │
└──────────────────────┬──────────────────────────┘
                       │ 倍率修正后的值
                       ▼
┌─────────────────────────────────────────────────┐
│  URecoilComponent（执行层，挂在 Player 上）       │
│  ├── 运行时参数（由 AddRecoil 每次更新）          │
│  ├── TargetPitch / CurrentPitch（插值状态）      │
│  └── Tick → FInterpTo → AddControllerPitchInput  │
└─────────────────────────────────────────────────┘
```

### 2.2 组件定位

`URecoilComponent` 是**纯执行器**——不持有自己的配置，所有参数从武器侧传入。这遵循了项目中 "武器是数据容器" 的核心设计原则：

| 层 | 职责 | 类比 |
|----|------|------|
| ABaseWeapon | 定义后坐力数值曲线 | 武器数据 |
| ABaseWeaponAttachment | 修正数值（倍率） | 配件加成 |
| URecoilComponent | 平滑 + 应用 Pitch | 执行器 |

## 3. 数据流

```
GA_WeaponFire::PerformFire()
  │
  ├── Weapon->GetEffectiveRecoil()                    ← 单发幅度 × 配件倍率
  ├── Weapon->GetEffectiveRecoilInterpSpeed()          ← 上升速度 × 配件倍率
  ├── Weapon->GetEffectiveRecoilRecoverySpeed()        ← 恢复速度 × 配件倍率
  └── Weapon->GetEffectiveRecoilMaxAccumulation()      ← 累积上限 × 配件倍率
         │
         │ 4 个参数通过接口传入
         ▼
WeaponOwner->AddWeaponRecoil(amount, interp, recovery, maxAccum)
         │
         ├── ABaseCharacter（敌人）：空实现，不产生后坐力
         └── APlayerCharacter（玩家）：
               ├── RecoilComponent->AddRecoil(4 params)  ← 累加 TargetPitch
               └── MulticastAddWeaponRecoil(4 params)    ← 广播到所有客户端
                     └── RecoilComponent->AddRecoil(4 params)
```

### 3.1 逐帧平滑流程

```
每帧 Tick（URecoilComponent::TickComponent）：
  │
  ├── Target < Current ?
  │     ├── 是（回落中）→ FInterpTo(Current, Target, DeltaTime, RecoverySpeed)
  │     └── 否（上升中）→ FInterpTo(Current, Target, DeltaTime, InterpSpeed)
  │
  ├── DeltaPitch = CurrentNew - CurrentOld
  ├── if !NearlyZero(DeltaPitch) → AddControllerPitchInput(-DeltaPitch)
  │     └── 负值 = 抬头（枪口上跳）
  │
  └── TargetPitch = FInterpTo(Target, 0, DeltaTime, RecoverySpeed)
        └── 停火后 Target 自然衰减回 0
```

### 3.2 行为效果

| 场景 | 表现 |
|------|------|
| 单发 | Target 瞬间 +N → Current 以 InterpSpeed 追赶 → 到达后 Target 自动衰减 → Current 以 RecoverySpeed 回落 |
| 连射（全自动） | 每发累加 Target → 达到 MaxAccumulation 后稳定 → Current 持续追赶 |
| 停火 | Target 自动衰减到 0 → Current 平滑恢复 → 准星回到原位 |
| AK（高后坐力） | FiringRecoil 大 + InterpSpeed 低 → 上跳大、恢复慢 |
| M4（低后坐力） | FiringRecoil 小 + InterpSpeed 高 + RecoverySpeed 高 → 上跳小、恢复快 |

## 4. 关键代码

### 4.1 URecoilComponent — 纯执行器

```cpp
// RecoilComponent.h
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URecoilComponent : public UActorComponent
{
    void AddRecoil(float Amount, float InInterpSpeed,
                   float InRecoverySpeed, float InMaxAccumulation);
private:
    float TargetPitch = 0.0f;
    float CurrentPitch = 0.0f;
    float InterpSpeed = 15.0f;      // 运行时参数，每次 AddRecoil 可能更新
    float RecoverySpeed = 5.0f;     // 切换武器时这些值会变
    float MaxAccumulation = 20.0f;
};
```

### 4.2 武器侧 GetEffective 模式

```cpp
// 以 InterpSpeed 为例，RecoverySpeed / MaxAccumulation 同理
float ABaseWeapon::GetEffectiveRecoilInterpSpeed() const
{
    float Mult = 1.0f;
    for (const auto& Pair : EquippedAttachments)
        if (Pair.Value)
            Mult *= Pair.Value->GetRecoilInterpMultiplier();
    return RecoilInterpSpeed * Mult;
}
```

### 4.3 接口签名

```cpp
// IBaseWeaponHolder
virtual void AddWeaponRecoil(float RecoilAmount,
    float InterpSpeed, float RecoverySpeed, float MaxAccumulation) {}

// GA_WeaponFire 调用处
Weapon->WeaponOwner->AddWeaponRecoil(
    Weapon->GetEffectiveRecoil(),
    Weapon->GetEffectiveRecoilInterpSpeed(),
    Weapon->GetEffectiveRecoilRecoverySpeed(),
    Weapon->GetEffectiveRecoilMaxAccumulation());
```

## 5. 踩过的坑

| 问题 | 根因 | 解决 |
|------|------|------|
| 后坐力向下而非向上 | UE 中 `AddControllerPitchInput(正)` = 低头 | 取反：`AddControllerPitchInput(-DeltaPitch)` |
| 后坐力瞬间跳变 | 直接在 Multicast RPC 中调 `AddControllerPitchInput` | 改为 Tick 驱动 FInterpTo 逐帧平滑 |
| PlayerCharacter 职责膨胀 | 后坐力参数、状态、Tick 逻辑全堆在 Player 里 | 拆出 URecoilComponent 独立组件 |
| 所有武器共用一套手感参数 | InterpSpeed 等写死在组件上 | 参数下放到 ABaseWeapon，每次 AddRecoil 动态传入 |
| 接口签名变更波及面广 | 加参数后 BaseCharacter/BaseEnemy 都需要改签名 | 基类给空默认实现，敌人不产生后坐力 |
| 配件倍率缺失 | 只设计了 RecoilMultiplier，没有平滑参数倍率 | 新增 Interp/Recovery/MaxAccumulation 三个倍率 |

## 6. 涉及文件

| 文件 | 职责 |
|------|------|
| `Components/RecoilComponent.h/.cpp` | 后坐力执行器：累加 + 插值 + Pitch 输出 |
| `Weapons/BaseWeapon.h/.cpp` | 4 个后坐力属性 + 4 个 GetEffective 方法 |
| `Weapons/BaseWeaponAttachment.h` | 4 个后坐力倍率属性 |
| `Weapons/BaseWeaponHolder.h` | 接口 AddWeaponRecoil 签名 |
| `BaseCharacter.h/.cpp` | 空实现（敌人无后坐力） |
| `BaseEnemy.h/.cpp` | 空实现 |
| `PlayerCharacter.h/.cpp` | 持有 RecoilComponent + 委托 + RPC |
| `GA_WeaponFire.cpp` | 开火时读取武器参数并传入 |
