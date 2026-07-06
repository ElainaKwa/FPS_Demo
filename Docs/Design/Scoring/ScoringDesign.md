# 计分 & 敌人重生系统设计文档

## 1. 功能清单

| 功能 | 说明 |
|------|------|
| 击杀计分 | 玩家每击杀一个敌人 +1 分，分数通过 PlayerState 网络复制到所有客户端 |
| 分数 HUD | 屏幕左上角实时显示当前分数，通过委托绑定 PlayerState 自动刷新 |
| 敌人重生 | 死亡后延迟 N 秒自动复活（默认 10 秒），可配置 |
| 随机重生位置 | NavMesh 随机可达点，保证在地图可行走区域内 |
| 安全检测 | 胶囊体重叠检测 + 离玩家最小距离 + 头顶空间检测 |
| 防重复死亡 | State_Dead 守卫，防止多帧伤害导致 OnDeath 被重复调用 |
| 死后恢复 | Ragdoll → 站立姿态 + 重装备武器 + 满血 |

---

## 2. 架构

### 2.1 类图

```
APlayerState (Engine)
  └── ABasePlayerState
        ├── int32 Kills       (Replicated)
        ├── int32 Deaths      (Replicated)
        ├── float Score       (继承自父类, Replicated)
        └── FOnScoreUpdated   (BlueprintAssignable)

AGameModeBase (Engine)
  └── ABaseGameMode
        ├── PlayerStateClass = ABasePlayerState
        └── OnKill(AActor* Killer, ABaseCharacter* Victim)

ABaseCharacter
  └── OnDeath()
        ├── State_Dead 守卫（防重复）
        ├── OwnedWeapons.Remove + DropToGround
        └── MulticastDeathVisuals
              ├── APlayerCharacter::OnDeath → DisableInput
              └── ABaseEnemy::OnDeath → SetTimer → Respawn

UUserWidget (Engine)
  └── UBaseScoreWidget
        └── BindWidget: ScoreText (UTextBlock)
```

### 2.2 数据流

```
[开火] GA_WeaponFire::MakeOutgoingGameplayEffectSpec
         └── EffectContext 自动记录 Instigator = 开枪角色

[命中] TargetASC->ApplyGameplayEffectSpecToSelf

[执行] BaseHealthAttributeSet::PostGameplayEffectExecute
         └── Health = Clamp(Health - IncomingDamage, 0, MaxHealth)
         └── Health <= 0 ?
               ├── Killer = EffectContext.GetOriginalInstigator()
               ├── Character->OnDeath()
               └── GameMode->OnKill(Killer, Victim)

[计分] ABaseGameMode::OnKill
         ├── Controller = Cast<APawn>(Killer)->GetController()
         ├── PlayerState->AddKill() → Score++, OnScoreUpdated.Broadcast
         └── Victim->GetPlayerState()->AddDeath()

[HUD]  UBaseScoreWidget::OnScoreUpdated → ScoreText->SetText()
```

---

## 3. 关键实现细节

### 3.1 击杀者追踪

GAS 在创建 EffectSpec 时自动将 Ability 的 AvatarActor（即开火角色）设为 EffectContext 的 Instigator。因此只需在 PostGameplayEffectExecute 中读取：

```cpp
AActor* Killer = Data.EffectSpec.GetContext().GetOriginalInstigator();
```

参数类型：`Killer` 是 Pawn（ABaseCharacter），在 GameMode 中通过 `GetController()` 获取 PlayerController，再取 PlayerState。

### 3.2 复用父类 Score

`APlayerState` 已自带 `float Score` 和 `OnRep_Score()`，不能重复定义。子类 `ABasePlayerState` 只新增 Kills/Deaths，Score 通过 `SetScore(GetScore() + 1.0f)` 操作。

### 3.3 重生位置选择

```
SelectRespawnLocation()
  ├── 搜索原点: RespawnOrigins[Random] 或 InitialLocation
  ├── 循环 10 次:
  │     ├── NavMesh.GetRandomReachablePointInRadius(Origin, Radius)
  │     └── IsLocationSafe(Point)
  │           ├── 离每个玩家 > MinRespawnDistanceToPlayer (1500cm)
  │           ├── 胶囊体重叠检测（在 CapsuleCenter = Point + HalfHeight 处）
  │           └── 头顶空间射线检测（向上 HalfHeight + 50cm）
  ├── 兜底: NavMesh.GetRandomPoint (放宽限制)
  └── 最终兜底: InitialLocation
```

**关键修复**：Overlap 检测必须在胶囊体中心高度（地面点 + HalfHeight），而非 NavMesh 返回的地面点，否则胶囊体下半段嵌在地里不会被检测到。

### 3.4 TeleportTo 替代 SetActorLocation

角色有 CharacterMovementComponent 时，`SetActorLocation` 可能被 CMC 的地面检测逻辑干扰。改用 `TeleportTo(Location, Rotation, false, true)` 是正确做法。

### 3.5 Ragdoll 恢复

```
GetMesh()->SetSimulatePhysics(false);      // 停止物理
GetMesh()->SetRelativeLocation(0,0,-96);   // 重置相对位置
GetMesh()->SetRelativeRotation(0,-90,0);   // 重置相对旋转
GetMesh()->AttachToComponent(Capsule, ...); // 重新挂接
GetMesh()->InitAnim(true);                  // 重新初始化动画蓝图 → 待机姿态
```

### 3.6 防重复死亡

`ABaseCharacter::OnDeath()` 开头检查 `State_Dead` 标签，已死亡则直接返回。`ABaseEnemy::OnDeath()` 在 Super 调用前先用 `bAlreadyDead` 缓存状态，防止重复设置重生计时器。

### 3.7 武器重新装备

`DropToGround()` 不清理 `OwnedWeapons` 数组，导致 `SpawnDefaultWeapon()` → `FindWeaponOfClass()` 找到地上旧武器。修复：OnDeath 中 `OwnedWeapons.Remove(CurrentWeapon)` 后再 drop。

---

## 4. 配置参数

### 4.1 ABaseEnemy UPROPERTY

| 属性 | 默认值 | 说明 |
|------|--------|------|
| `RespawnDelay` | 10.0s | 死亡到重生的等待时间 |
| `RespawnSearchRadius` | 5000cm | NavMesh 搜索半径 |
| `MinRespawnDistanceToPlayer` | 1500cm | 离玩家最小距离 |
| `bUseCustomRespawnOrigins` | false | 是否手动指定重生区域中心 |
| `RespawnOrigins` | [] | 手动配置的重生区域原点列表 |

### 4.2 DefaultGame.ini

```ini
[/Script/MyFps_Demo.ABasePlayerController]
ScoreWidgetClassPath=/Game/FPSContent/Blueprint/UI/Score/UMG_ScoreWidget.UMG_ScoreWidget_C
```

### 4.3 蓝图资产

| 资产 | 父类 | 说明 |
|------|------|------|
| `BP_PlayerState` | `ABasePlayerState` | 网络复制的玩家计分数据 |
| `UMG_ScoreWidget` | `UBaseScoreWidget` | 左上角分数显示 |

---

## 5. 踩坑记录

| 问题 | 根因 | 解决 |
|------|------|------|
| `Score` 编译报错 | 父类 `APlayerState` 已有 `float Score` | 删除子类定义，复用 `SetScore()`/`GetScore()` |
| `OnRep_Score` 编译报错 | 父类已有 UFUNCTION | 删除子类声明 |
| `FAttachmentTransformRules::SnapToTarget` 不存在 | UE 5.7 移除此静态工厂 | 改用构造 `FAttachmentTransformRules(EAttachmentRule::SnapToTarget, false)` |
| 复活后倒在地上 | `SetSimulatePhysics(false)` 不重置骨骼变换 | 手动恢复 RelativeLocation/Rotation + `InitAnim(true)` |
| 复活后没有武器 | 旧武器仍在 `OwnedWeapons` 中 | OnDeath 中 `OwnedWeapons.Remove(CurrentWeapon)` |
| 偶尔不复活 | 同帧多 GE 导致 OnDeath 多次调用 | `State_Dead` 守卫 + `bAlreadyDead` 前置检查 |
| 复活在墙里 | Overlap 检测在地面高度而非胶囊体中心，头顶没检测 | 偏移 HalfHeight + 加天花板射线 |

---

## 6. 编辑器配置步骤

1. **关卡中放置 `NavMeshBoundsVolume`**，缩放覆盖整个可玩区域
2. **Build → Build Paths** 生成 NavMesh（按 P 预览）
3. **创建 `BP_PlayerState`**（继承 `ABasePlayerState`）
4. **创建 `UMG_ScoreWidget`**（继承 `UBaseScoreWidget`）：
   - 拖入 TextBlock 命名为 `ScoreText`
   - 锚点设左上角，位置 (20, 20)
5. **修改 `BP_GameMode`**：PlayerStateClass = `BP_PlayerState`
6. **修改 `BP_Enemy`**：调整 RespawnDelay / RespawnSearchRadius 等参数
