# 角色移动系统 GAS 化设计文档

## 1. 设计目标

将角色移动操作（下蹲、疾跑、跳跃）从直接调用 `ACharacter` API 改为通过 **Gameplay Ability System (GAS)** 管理，实现：

- **GE 驱动移动速度**：`BaseMoveSpeed` 可通过 GameplayEffect 动态修改（如受击减速、buff 加速）
- **状态互斥**：通过 GameplayTag 管理 Crouch/Sprint/Airborne 状态，避免冲突
- **体力系统集成**：下蹲/跳跃/疾跑的体力消耗统一走 `IncomingStaminaCost`
- **网络同步**：遵循项目 Server RPC 模式，GA 设为 `ServerOnly`

---

## 2. 架构概览

```
玩家输入 (Ctrl/Shift/Space)
  └─ APlayerCharacter 输入回调
      └─ Server RPC (ServerCrouch/ServerSprint/ServerJump)
          └─ ASC->TryActivateAbility()
              └─ GA::ActivateAbility (K2_HasAuthority 守卫)
                  ├─ State Tag 管理 (AddLoose/RemoveLoose)
                  ├─ GE 应用 (体力消耗/体力流失)
                  ├─ 角色动作 (Crouch/Jump)
                  └─ ApplyMovementSpeed() → CMC->MaxWalkSpeed
```

---

## 3. 新增组件

### 3.1 BaseMovementAttributeSet

```
属性:
  BaseMoveSpeed          = 600.0f   (基础移动速度，GE 可修改)
  SprintSpeedMultiplier  = 1.5f     (疾跑速度倍率，GE 可修改)
  CrouchSpeedMultiplier  = 0.5f     (下蹲速度倍率，GE 可修改)
```

- 不进行网络复制（CMC->MaxWalkSpeed 已自行复制）
- 所有者：`APlayerCharacter`
- `PostGameplayEffectExecute`：属性变更 → 调用 `Player->ApplyMovementSpeed()`
- 所有属性钳制到有效范围

### 3.2 ApplyMovementSpeed()

`APlayerCharacter::ApplyMovementSpeed()` 是移动速度的**单一计算入口**：

```
读取 MovementAttributeSet->BaseMoveSpeed
  if (State.Sprinting)   → *= SprintSpeedMultiplier
  else if (State.Crouching) → *= CrouchSpeedMultiplier
  else                    → 直接使用

写入 CMC->MaxWalkSpeed
```

速度自动随 GE 修改 `BaseMoveSpeed` 或倍率属性而更新（通过 PostGameplayEffectExecute → ApplyMovementSpeed）。

---

## 4. 能力详细设计

### 4.1 GA_Crouch（下蹲）

```
网络策略: ServerOnly
输入模式: 按住/切换（由输入层控制）

ActivationBlockedTags: State.Dead, State.Sprinting, State.Crouching

ActivateAbility():
  ├─ K2_HasAuthority() 守卫
  ├─ 检查 Dead/Sprinting/Crouching 标签
  ├─ Character->CanCrouch()
  ├─ CommitAbility()
  ├─ ApplyGE(StaminaCostEffectClass) → 一次性体力消耗
  ├─ Character->Crouch()           → UE 原生 Crouch（缩小碰撞体）
  ├─ Add State.Crouching 标签
  └─ ApplyMovementSpeed()          → 应用 CrouchSpeedMultiplier

EndAbility():
  ├─ Character->UnCrouch()         → 恢复碰撞体
  ├─ Remove State.Crouching 标签
  └─ ApplyMovementSpeed()          → 恢复 BaseMoveSpeed
```

**与疾跑交互**：正在疾跑时按下蹲 → `ServerCrouch_Implementation` 先取消 Sprint 能力 → 再激活 Crouch。

**体力规则**：仅在触发下蹲时消耗一次体力。下蹲维持期间不影响体力自然回复。

### 4.2 GA_Sprint（疾跑）

```
网络策略: ServerOnly
输入模式: 按住/切换（由输入层控制）

ActivationBlockedTags: State.Dead, State.Crouching, State.Reloading, State.Sprinting

ActivateAbility():
  ├─ K2_HasAuthority() 守卫
  ├─ 检查 Dead/Crouching/Reloading/Sprinting 标签
  ├─ CMC->IsFalling() 检查（空中不能疾跑）
  ├─ CommitAbility()
  ├─ Add State.Sprinting 标签
  ├─ ApplyMovementSpeed()               → 应用 SprintSpeedMultiplier
  └─ ApplyGE(StaminaDrainEffectClass)   → 周期性 GE，每秒消耗体力
       └─ 手柄存于 StaminaDrainHandle

EndAbility():
  ├─ RemoveActiveGameplayEffect(StaminaDrainHandle)
  ├─ Remove State.Sprinting 标签
  └─ ApplyMovementSpeed()               → 恢复 BaseMoveSpeed
```

**疾跑中开火**（`ServerStartFiring_Implementation`）：

```
if (State.Sprinting):
  └─ Cancel Sprint 能力
  └─ 不激活 Fire（玩家需松开开火键后重新按下才能开火）
else:
  └─ TryActivateAbility(Fire)
```

> 此设计确保每次开火对应一次独立按键触发，避免"一次按住 → 连续开火"的问题。

**疾跑体力耗尽**：周期性 GE 持续消耗 `IncomingStaminaCost`。当体力归零时，可在未来的 `StaminaAttributeSet` 扩展中广播事件来取消 Sprint。

### 4.3 GA_Jump（跳跃）

```
网络策略: ServerOnly
激活方式: 按键 → 立即执行 → 立即结束

ActivationBlockedTags: State.Dead

ActivateAbility():
  ├─ K2_HasAuthority() 守卫
  ├─ 检查 Dead 标签
  ├─ CMC->IsFalling() 检查（不能空中起跳）
  ├─ CommitAbility()
  ├─ ApplyGE(JumpStaminaCostEffectClass) → 一次性体力消耗
  ├─ Add State.Airborne 标签
  ├─ Character->Jump()
  └─ EndAbility()（即刻结束）

Airborne 标签移除:
  └─ APlayerCharacter::Landed() → ASC->RemoveLooseGameplayTag(State_Airborne)
```

`State.Airborne` 用于阻塞下蹲、疾跑和二段跳（后续扩展）。

---

## 5. 新增 GameplayTags

| Tag | 类型 | 说明 |
|-----|------|------|
| `Ability.Crouch` | 能力 | 下蹲能力标签 |
| `Ability.Sprint` | 能力 | 疾跑能力标签 |
| `Ability.Jump` | 能力 | 跳跃能力标签 |
| `State.Crouching` | 状态 | 下蹲中 |
| `State.Sprinting` | 状态 | 疾跑中 |
| `State.Airborne` | 状态 | 空中（跳跃/下落） |

---

## 6. 能力互斥矩阵

|  | Crouch | Sprint | Jump | Fire | Reload |
|---|---|---|---|---|---|
| **Crouch** | Block 自身 | Block Sprint | Allow | Allow | Allow |
| **Sprint** | Cancel Sprint → Crouch | Block 自身 | Block | Delay→Cancel →Fire | Block |
| **Jump** | 先 UnCrouch 再 Jump | Cancel Sprint → Jump | Block(空中) | Allow | Block |

---

## 7. 输入配置

| InputAction | 按键 | 绑定事件 |
|---|---|---|
| `CrouchAction` | Left Ctrl | Started → `OnCrouchStarted()` / Completed → `OnCrouchEnded()` |
| `SprintAction` | Left Shift | Started → `OnSprintStarted()` / Completed → `OnSprintEnded()` |

### 7.1 输入模式

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `SprintInputMode` | `ESprintInputMode` | `Hold` | 疾跑输入模式 |
| `CrouchInputMode` | `ECrouchInputMode` | `Hold` | 下蹲输入模式 |

- **Hold（按住）**: Started → 激活动作 / Completed → 取消动作
- **Toggle（切换）**: Started → 开/关切换 / Completed → 无操作

输入回调通过 Server RPC 转发到服务端执行。

---

## 8. 网络同步设计

### 8.1 RPC 分类

| RPC | 方向 | 说明 |
|-----|------|------|
| `ServerCrouch` | 客户端 → 服务端 | 请求激活下蹲 |
| `ServerUnCrouch` | 客户端 → 服务端 | 请求取消下蹲 |
| `ServerSprint` | 客户端 → 服务端 | 请求激活疾跑 |
| `ServerUnSprint` | 客户端 → 服务端 | 请求取消疾跑 |
| `ServerJump` | 客户端 → 服务端 | 请求激活跳跃 |

### 8.2 属性复制

| 属性 | 复制方式 | 说明 |
|------|---------|------|
| `CMC->MaxWalkSpeed` | CMC 内置复制 | 服务端设置，自动同步到客户端 |
| `Crouch/UnCrouch` | ACharacter 内置复制 | `bWantsToCrouch` 标志位自动同步 |
| `Jump` | ACharacter 内置复制 | `bPressedJump` 通过 ServerMove 同步 |
| `MovementAttributeSet` | 不复制 | 仅服务端读取后写入 CMC |

### 8.3 注意事项

- GA 构造函数中设 `NetExecutionPolicy = ServerOnly`，代码中 `K2_HasAuthority()` 守卫兜底
- 状态标签统一用 `AddLooseGameplayTag`/`RemoveLooseGameplayTag`，避免 BP 覆盖 `ActivationOwnedTags`
- `ServerStartFiring_Implementation` 中的定时器在服务端执行，确保 Sprint 取消完成后才激活 Fire

---

## 9. 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `BaseGameplayTags.h/.cpp` | 修改 | 新增 6 个标签 |
| `BaseMovementAttributeSet.h/.cpp` | **新建** | 移动速度属性集 |
| `GA_Crouch.h/.cpp` | **新建** | 下蹲能力 |
| `GA_Sprint.h/.cpp` | **新建** | 疾跑能力 |
| `GA_Jump.h/.cpp` | **新建** | 跳跃能力 |
| `BaseAbilitySystemComponent.h/.cpp` | 修改 | 新增 Enum + 句柄 + GrantMovementAbilities |
| `PlayerCharacter.h/.cpp` | 修改 | 新增 MovementAttributeSet + InputAction + RPC + ApplyMovementSpeed + Landed |

---

## 10. 蓝图配置清单

实现完成后，需在编辑器中创建以下 Blueprint 资产：

- [ ] `BP_GA_Crouch` — 继承 `GA_Crouch`，配置 `StaminaCostEffectClass`
- [ ] `BP_GA_Sprint` — 继承 `GA_Sprint`，配置 `StaminaDrainEffectClass`
- [ ] `BP_GA_Jump` — 继承 `GA_Jump`，配置 `StaminaCostEffectClass`
- [ ] `GE_StaminaCost_Crouch` — Instant GE，修改 `IncomingStaminaCost`
- [ ] `GE_StaminaDrain_Sprint` — Infinite + Periodic GE，修改 `IncomingStaminaCost`
- [ ] `GE_StaminaCost_Jump` — Instant GE，修改 `IncomingStaminaCost`
- [ ] `IA_Crouch` — InputAction 资产
- [ ] `IA_Sprint` — InputAction 资产
- [ ] ASC 组件上配置 `CrouchAbilityClass` / `SprintAbilityClass` / `JumpAbilityClass`
- [ ] `APlayerCharacter` BP 上配置 `CrouchAction` / `SprintAction`
- [ ] `IMC` 映射上下文添加 Crouch/Sprint InputAction

---

## 11. 踩坑预警

| 问题 | 根因 | 预防 |
|------|------|------|
| 疾跑速度 GE 移除后速度不恢复 | `StaminaDrainHandle` 未正确 Invalidate | EndAbility 中确保 Remove + Invalidate |
| 疾跑 + 下蹲同时激活 | 输入层未检查奔跑标签就激活下蹲 | `ServerCrouch_Implementation` 先取消 Sprint 再激活 Crouch |
| 下蹲中按跳跃后速度计算错误 | 标签移除顺序导致 ApplyMovementSpeed 读到错误状态 | EndAbility 中先处理角色动作，再移除标签，再调 ApplyMovementSpeed |
| 客户端 Hold 模式输入重复触发 | Enhanced Input `Triggered` vs `Started` 混淆 | 统一用 `Started` / `Completed` |
| 下蹲无效果（`CanCrouch()` 返回 false） | CMC 的 `NavAgentProps.bCanCrouch` 默认为 false | 在 PlayerCharacter 构造函数中设为 true |
| 下蹲失败后 `EndAbility` 尝试移除未添加的 Tag | `ActivateAbility` 提前返回调用 `EndAbility`，但 Tag 未添加 | `EndAbility` 中用 `HasMatchingGameplayTag` 守卫移除操作 |
| 疾跑体力归零不打断 | 没有监听体力变化的逻辑 | `BaseStaminaAttributeSet::PostGameplayEffectExecute` 中 Stamina≤0 时 CancelAbilityHandle(SprintHandle) |
| BP `ActivationBlockedTags` 覆盖 C++ 默认值导致 `CommitAbility` 静默失败 | BP CDO 会覆盖 C++ UPROPERTY 默认值 | 覆盖 `CanActivateAbility()` 在 C++ 层手动检查标签，绕过 BP 配置 |
| `ServerOnly` GA 执行 `Crouch()`/`Jump()` 后客户端看不到动作 | `Character->Crouch()` 的服务端调用仅通过 `bIsCrouched` 复制，自主代理的 CMC 预测可能不响应 | 用 `NetMulticast` RPC 执行蹲/跳动作，确保所有客户端本地执行 `Crouch(true)`/`Jump()` |
| 客户端 `OnJumpEnded` 调用 `StopJumping()` 可能干扰服务端跳跃 | 客户端的 `bPressedJump` 开关与服务器的异步跳跃冲突 | 保持 `OnJumpEnded` 仅调用 `StopJumping()`，它在客户端为 no-op（客户端未预测跳跃） |
| 疾跑原地不动也消耗体力 | 周期性 GE 无条件扣除 `IncomingStaminaCost` | `PostGameplayEffectExecute` 中检查 `CMC->Velocity.SizeSquared2D() < 1.0f` 则跳过 |
| 疾跑/下蹲只有按住模式 | 缺少输入模式切换 | 添加 `ESprintInputMode`/`ECrouchInputMode` 枚举 + `SprintInputMode`/`CrouchInputMode` 属性，`OnStarted` 内分支处理 |
| 体力消耗后不自动回复 | 无被动体力回复机制 | 新增 `GA_StaminaRegen` 被动 GA：CD GE 控制回复延迟，Infinite Periodic GE 控制回复间隔和回复量。消耗时 `ApplyCooldown`，CD 结束后自动施加回复 GE |
| HUD 体力条跳变不流畅 | `UpdateStaminaHUD` 直接广播原始值 | 改为 Tick 中用 `FMath::FInterpTo` 平滑插值 `SmoothedStamina` 再广播 |
