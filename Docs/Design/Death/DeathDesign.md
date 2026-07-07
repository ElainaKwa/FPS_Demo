# 死亡系统设计文档

## 1. 设计目标

死亡时完成以下流程：

1. **逻辑层**：打 `State.Dead` 标签 → 取消所有能力 → 武器掉落
2. **视觉层**：隐藏 FP 渲染 → 拉远镜头 → Ragdoll 物理
3. **输入层**：阻塞所有输入 → 显示鼠标 → 弹出死亡提示 UI
4. **计分层**：通知 GameMode → PlayerState 更新击杀/死亡数

## 2. 触发入口

`BaseHealthAttributeSet::PostGameplayEffectExecute` 检测血量归零：

```cpp
if (GetHealth() <= 0.0f)
{
    AActor* Killer = Data.EffectSpec.GetContext().GetOriginalInstigator();
    Character->OnDeath();              // ① 死亡处理
    GM->OnKill(Killer, Character);     // ② 计分
}
```

## 3. 死亡处理链

### 3.1 ABaseCharacter::OnDeath() — 基类

```
OnDeath()
  ├── State_Dead 标签守卫（防重复触发）
  ├── IsDead() 辅助方法 ← 新增
  │     └── return ASC && ASC->HasMatchingGameplayTag(State_Dead);
  ├── AddLooseGameplayTag(State_Dead)  → 阻塞所有能力激活
  ├── CancelAllAbilities()             → 中断开火/换弹/移动能力
  ├── 武器 DropToGround()（仅服务端）
  └── MulticastDeathVisuals()          → 所有客户端 Ragdoll
```

### 3.2 MulticastDeathVisuals — Ragdoll

```cpp
void ABaseCharacter::MulticastDeathVisuals_Implementation()
{
    GetCapsuleComponent()->SetCollisionEnabled(NoCollision);
    if (GetMesh()->GetSkeletalMeshAsset())
    {
        GetMesh()->SetSimulatePhysics(true);
        GetMesh()->SetCollisionEnabled(PhysicsOnly);
    }
}
```

### 3.3 APlayerCharacter::OnDeath() — 玩家特化

```
OnDeath()
  ├── Super::OnDeath()                    → 基类处理
  ├── FirstPersonMesh->SetVisibility(false)      → 隐藏 FP 手臂
  ├── CurrentWeapon->GetFirstPersonMesh()->SetVisibility(false) → 隐藏 FP 武器
  ├── FirstPersonCamera->SetRelativeLocation(-300, 0, 100) → 镜头后拉
  └── ABasePlayerController:
        ├── SetShowMouseCursor(true)       → 显示鼠标
        ├── SetInputMode(UIOnly)           → 纯 UI 输入模式
        ├── DisableInput(this)             → 禁止玩家输入
        └── ShowDeathScreen()              → 隐藏游戏 HUD + 显示死亡 Widget
```

### 3.4 输入阻塞 — IsDead() 守卫

所有 Enhanced Input 回调在入口处检查 `IsDead()`：

```
OnStartFiring()    → if (IsDead()) return;
OnReload()         → if (IsDead()) return;
OnSwitchWeapon()   → if (IsDead()) return;
OnJumpStarted()    → if (IsDead()) return;
OnCrouchStarted()  → if (IsDead()) return;
OnSprintStarted()  → if (IsDead()) return;
OnMove()           → if (IsDead()) return;
OnLook()           → if (IsDead()) return;
```

`IsDead()` 定义在 `ABaseCharacter`：
```cpp
bool ABaseCharacter::IsDead() const
{
    return ASC && ASC->HasMatchingGameplayTag(State_Dead);
}
```

### 3.5 ShowDeathScreen — 死亡 UI

`ABasePlayerController::ShowDeathScreen()`：

```
从 Viewport 移除：                 添加到 Viewport：
├── CrosshairWidget                └── DeathWidget（优先级 10，最上层）
├── BulletCounterWidget
├── HealthBarWidget
├── StaminaBarWidget
└── ScoreWidget
```

## 4. 涉及文件

| 文件 | 职责 |
|------|------|
| `GameAbilitySystem/BaseHealthAttributeSet.cpp` | 血量归零 → OnDeath + OnKill |
| `BaseCharacter.h/.cpp` | OnDeath 基类 + IsDead() + MulticastDeathVisuals |
| `PlayerCharacter.h/.cpp` | OnDeath 玩家特化：FP 隐藏 + 镜头 + 输入模式 + 守卫 |
| `BasePlayerController.h/.cpp` | ShowDeathScreen + DeathWidget 管理 |
| `UI/Death/BaseDeathWidget.h` | 死亡提示 Widget C++ 基类 |
| `Config/DefaultGame.ini` | DeathWidgetClassPath 配置 |
| `BaseGameMode.cpp` | OnKill → PlayerState 计分 |

## 5. 踩过的坑

| 问题 | 根因 | 解决 |
|------|------|------|
| 死亡后输入导致编辑器崩溃 | 输入回调无死亡守卫，TryActivateAbility 后访问已置空的 CurrentWeapon | 所有输入回调加 `if (IsDead()) return;` |
| 同帧多 GE 导致 OnDeath 多次触发 | PostGameplayEffectExecute 每 GE 调用一次 | State_Dead 标签守卫，首次打标后直接 return |
| DisableInput 不生效 | 传入参数错误（传了 Controller 而非 Pawn） | 改为 `DisableInput(PC)` 配合 UIOnly 输入模式 |
| 死亡后仍能转动视角 | OnLook 无死亡守卫 | 加 `if (IsDead()) return;` |
