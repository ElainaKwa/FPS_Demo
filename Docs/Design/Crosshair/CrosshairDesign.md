# 屏幕准星设计文档

## 1. 设计目标

实现 FPS 动态准星 HUD：经典的 **十字 + 中心点** 样式，固定在屏幕中心，准星散度响应玩家移动 / 开火 / 武器属性，提供即时瞄准反馈。

- **样式**：上下左右四条线 + 中心圆点
- **位置**：屏幕中心（CanvasPanel 由 Editor Util 自动锚定中心）
- **动态散度**：玩家移动速度、持续开火、武器基础散布 → 线条外扩
- **平滑过渡**：当前散度 lerp 到目标散度（避免跳变）
- **架构**：遵循现有 MVC + UnLua 占位模式，与 `UBaseBulletCounterWidget` 一致

## 2. 架构设计

### 2.1 分层数据流

```
Model (ABaseCharacter + ABaseWeapon)       View (UCrosshair_CrossDot)            Controller (ABasePlayerController)
┌────────────────────────┐                  ┌─────────────────────────┐           ┌─────────────────────────┐
│ ABaseCharacter:        │                  │ Widget Blueprint (UE)   │           │ BeginPlay()             │
│  Velocity (CM)         │─── Polled ───→   │   CanvasPanel           │           │   → 读 Subsystem 获取   │
│  IsFiring()            │    per Tick       │     Image_Top           │           │     当前准星类型        │
│  CharacterState (enum) │                   │     Image_Bottom        │           │   → CreateWidget()      │
│                        │                   │     Image_Left          │           │   → AddToViewport(z=-1) │
│ ABaseWeapon:           │                   │     Image_Right         │           └─────────────────────────┘
│  GetEffectiveAimVar()  │─── Polled ───→    │     Image_Dot           │
│  FiringSpread          │    per Tick        │                         │
│  (per-shot 累计)       │                   │ NativeTick():           │
└────────────────────────┘                   │   → ComputeTargetSpread │
                                             │   → Lerp CurrentSpread  │
                                             │   → OnSpreadUpdated()   │
                                             └─────────────────────────┘
```

**为什么用 Poll 而不用委托**：散度需要每帧读取角色速度/武器状态以平滑过渡。Tick 驱动更简洁，避免为每类散度源各加一个委托。

### 2.2 MVC + 设置层 职责

| 层 | 文件 | 职责 |
|---|------|------|
| **Model** | `ABaseCharacter` + `ABaseWeapon` | 提供速度、开火状态、武器散布数据 |
| **View** | `UBaseCrosshairWidget` + `BP` + `.lua` | 控件构图、散度计算与 lerp、视觉渲染 |
| **Controller** | `ABasePlayerController` | 从 Subsystem 获取准星类 → `CreateWidget`；暴露 `SetCrosshairType()` |
| **Settings** | `UCrosshairSettingsSubsystem` + `UCrosshairSaveGame` | 全局单例持久化：参数存取、准星类型注册、磁盘读写 |

### 2.3 类层级

```
UGameInstanceSubsystem (Engine)
  └── UCrosshairSettingsSubsystem
        │  跨关卡全局单例，持有当前准星参数 + 类型注册表
        └── 使用 UCrosshairSaveGame 磁盘持久化

UUserWidget (Engine)
  └── UBaseCrosshairWidget (C++, 抽象基类)
        │  implements IUnLuaInterface
        │  职责：角色绑定、散度计算、Tick 动画
        │  无 BindWidget，不包含任何形状参数
        │  暴露 BlueprintNativeEvent：
        │    GatherSettings()  → 子类把当前参数写入 FCrosshairSettings
        │    ApplySettings()   → 从 FCrosshairSettings 恢复参数
        │    OnSpreadUpdated(float)     → 散度变化时调用
        │    OnVisibilityChanged(bool)  → 显隐切换时调用
        │
        ├── UCrosshair_CrossDot (十字+点型，当前默认)
        │     BindWidget × 5：Image_Top/Bottom/Left/Right/Dot
        │     形状参数：LineLength/Thickness/BaseGap/DotSize
        │     覆写 OnSpreadUpdated → ApplySpreadToLines
        │
        ├── UCrosshair_Circle (圆形，未来)
        │     BindWidget × 1：CircleImage
        │     形状参数：Radius/RingThickness
        │     覆写 OnSpreadUpdated → SetRenderScale / 材质参数
        │
        └── UCrosshair_Chevron (V 字型，未来)
              自定义 BindWidget + 形状参数
              覆写 OnSpreadUpdated → 独立扩散逻辑
```

> **当前状态**：`UBaseCrosshairWidget` 仍包含全部 BindWidget 和形状代码，**重构待执行**（见第 11 章）。

## 3. 控件组成

### 3.1 Blueprint Designer（编辑器）布局

`UMG_Crosshair` Blueprint 的 Designer 层级：

```
CanvasPanel (根)
  │  锚点: 全屏拉伸 (0,0)→(1,1)
  │
  ├─ Image_Top (准星上横线)
  │    Size: (LineLength, LineThickness)  例: (13, 2.4) px
  │    Alignment: (0.5, 0.5) 中心对齐（代码强制）
  │    正常位置: (0, -Gap)      Gap = 6px（默认间距）
  │    颜色: 白色 RGBA(1,1,1,0.9)
  │
  ├─ Image_Bottom (准星下横线)
  │    Size: (LineLength, LineThickness)
  │    Alignment: (0.5, 0.5)
  │    正常位置: (0, +Gap)
  │    颜色: 白 85%不透明
  │
  ├─ Image_Left (准星左横线)
  │    Size: (LineThickness, LineLength)  竖向
  │    Alignment: (0.5, 0.5)
  │    正常位置: (-Gap, 0)
  │
  ├─ Image_Right (准星右横线)
  │    Size: (LineThickness, LineLength)
  │    Alignment: (0.5, 0.5)
  │    正常位置: (+Gap, 0)
  │
  └─ Image_Dot (中心点)
       Size: (DotSize, DotSize)  2.4 × 2.4
       Alignment: (0.5, 0.5)
       正常位置: (0, 0)
       颜色: 白 85%不透明
```

> Size 和 Alignment 均由 C++ 代码强制执行，不依赖 Designer 设置。

**线条样式参数**（`EditAnywhere`，蓝图可覆写；`BlueprintCallable` Setter，设置界面可运行时调节）：

| 参数 | 默认值 | Setter | 说明 |
|------|--------|--------|------|
| `LineLength` | 13 px | `SetLineLength(float)` | 线条长度 |
| `LineThickness` | 2.4 px | `SetLineThickness(float)` | 线条粗细 |
| `BaseGap` | 6 px | `SetBaseGap(float)` | 无散度时的线心间距 |
| `DotSize` | 2.4 px | `SetDotSize(float)` | 中心点直径 |
| `CrosshairColor` | 白 85%不透明 | `SetCrosshairColor(FLinearColor)` | 准星颜色（含透明度） |
| `CrosshairRotation` | 90° | `SetCrosshairRotation(float)` | 十字线旋转角度 |
| `bEnableSpread` | true | — | 是否启用动态散度（基类属性） |
| `MaxSpreadPixels` | 120 px | — | 散度最大偏移量（高级参数） |

### 3.2 C++ NativeConstruct

```cpp
void UBaseCrosshairWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ApplyVisualSettings();  // 根据默认参数初始化所有 Image 的大小和颜色

    FTimerDelegate Delegate;
    Delegate.BindUObject(this, &UBaseCrosshairWidget::TryBindCharacter);
    GetWorld()->GetTimerManager().SetTimerForNextTick(Delegate);
}
```

`ApplyVisualSettings()` 负责：
- 通过 `UCanvasPanelSlot::SetAlignment(0.5, 0.5)` 强制居中
- 通过 `UCanvasPanelSlot::SetSize()` 设置各 Image 尺寸（横线: `LineLength × LineThickness`，竖线: `LineThickness × LineLength`，中心点: `DotSize × DotSize`）
- 通过 `SetColorAndOpacity()` 设置颜色/透明度
- 通过 `SetRenderTransformAngle(CrosshairRotation)` 旋转全部线条

每个 `BlueprintCallable` Setter 调用后会立即执行 `ApplyVisualSettings()`，设置界面修改参数即刻生效。

### 3.3 角色引用绑定（C++）

```cpp
void UBaseCrosshairWidget::TryBindCharacter()
{
    APlayerController* PC = GetOwningPlayer();
    CachedCharacter = PC ? Cast<ABaseCharacter>(PC->GetPawn()) : nullptr;

    if (CachedCharacter.IsValid())
    {
        CachedWeapon = CachedCharacter->GetCurrentWeapon();
    }
}
```

> 按 AGENTS.md 约定：委托绑定必须在 C++ 侧完成；Lua 仅做占位。

### 3.4 设置持久化接口（BlueprintNativeEvent）

```cpp
// 子类可覆写以支持不同准星形状的参数序列化
UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
FCrosshairSettings GatherSettings() const;

UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
void ApplySettings(const FCrosshairSettings& Settings);
```

默认实现：
- `GatherSettings` → 把 `LineLength`、`Thickness`、`BaseGap`、`DotSize`、`Color` 等写入 `FloatParams` / `ColorParams`
- `ApplySettings` → 从 Map 恢复各参数并调用 `ApplyVisualSettings()`

未来圆形准星子类覆写这两个方法，写入/读取 `Radius`、`RingThickness` 等自己的参数。

### 3.5 全局设置层（Subsystem）

```
UCrosshairSettingsSubsystem（GameInstance 单例）
  │
  ├── CrosshairTypeRegistry  ← 存储 FName → TSubclassOf 映射
  │     "CrossDot" → UMG_Crosshair
  │     "Circle"   → UMG_Crosshair_Circle（未来注册）
  │
  ├── CurrentSettings  ← FCrosshairSettings（当前参数快照）
  │
  ├── SaveSettings()   → UCrosshairSaveGame → SaveGameToSlot("CrosshairSettings")
  ├── LoadSettings()   → LoadGameFromSlot → 恢复 CurrentSettings
  │
  ├── ApplyToWidget(Widget)   → 调 Widget->ApplySettings(CurrentSettings)
  └── GatherFromWidget(Widget) → 调 Widget->GatherSettings() → 写入 CurrentSettings
```

### 3.6 Lua 绑定

```lua
-- Content/Script/BaseCrosshairWidget.lua
local M = UnLua.Class()

function M:Initialize()
    print("BaseCrosshairWidget Lua: bound successfully")
end

return M
```

## 4. 数据流（完整链路）

```
NativeTick (每帧)
  │
  ├─ 角色有效？
  │   └─ 否 → 隐藏所有线条，返回           [角色死亡/不存在时准星消失]
  │
  ├─ ComputeTargetSpread()                  [计算目标散度 0..1]
  │   │
  │   ├─ 1. 移动散度
  │   │      Speed = CachedCharacter.Velocity.Size()
  │   │      MoveSpread = FMath::Clamp(Speed / MaxSpeedFullSpread, 0.0f, 1.0f)
  │   │      [站立 = 0, 满速 = 1]
  │   │
  │   ├─ 2. 开火散度（每发递增，冷却衰减）
  │   │      if (IsFiring): FireSpread += ShotSpreadIncrement * DeltaTime
  │   │      else:          FireSpread -= SpreadRecoveryRate * DeltaTime
  │   │      FireSpread = Clamp(FireSpread, 0.0f, 1.0f)
  │   │
  │   └─ 3. 武器基础散布因子
  │          WeaponSpread = Map(VarianceDeg, 0, MaxVarianceDeg, 0, 1)
  │          [AimVariance=2° → 低散度, 散布上限=10° → 1.0]
  │
  ├─ CurrentSpread = FMath::FInterpTo(
  │       CurrentSpread, TargetSpread, DeltaTime, InterpSpeed)
  │
  └─ ApplySpreadToLines(CurrentSpread * MaxSpread)  [MaxSpread = 最大偏移像素]
      │
      ├─ Top:    (0, -(BaseGap + Offset))
      ├─ Bottom: (0, +(BaseGap + Offset))
      ├─ Left:   (-(BaseGap + Offset), 0)
      ├─ Right:  (+(BaseGap + Offset), 0)
      └─ Dot:    (0, 0)  ← 始终不动
```

### 4.1 散度因子权重

```
TargetSpread = w1 * MoveSpread + w2 * FireSpread + w3 * WeaponSpread
```

| 因子 | 默认权重 | 说明 |
|------|---------|------|
| `MoveSpread` | 0.4 | 移动散度权重 |
| `FireSpread` | 0.35 | 连射散度权重 |
| `WeaponSpread` | 0.25 | 武器固有散布权重 |

权重之和 ≤ 1（钳制），确保 TargetSpread 在 [0, 1] 内。

### 4.2 散度可调参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `MaxSpeedFullSpread` | 600 cm/s | 达到此速度时移动散度 = 1.0 |
| `ShotSpreadIncrement` | 0.08 | 每发开火累积的散度 |
| `SpreadRecoveryRate` | 0.5 /s | 不开火时散度衰减速率 |
| `InterpSpeed` | 12 | lerp 速度（越高跟随越紧） |
| `MaxVarianceDeg` | 10° | AimVariance 映射上限 |

## 5. 武器接入变更

### 5.1 ABaseWeapon 新增

`ABaseWeapon` 新增 `bool bIsFiring` 成员：
- `GA_WeaponFire::ActivateAbility()` 中设为 `true`，`EndAbility()` 中设为 `false`
- Widget 通过 `bIsFiring` 判断开火状态，驱动 `FireSpreadAccum` 累积/衰减

已使用的现有 API：
- `GetEffectiveAimVariance()` → 武器基础散布映射
- `TimeOfLastShot` → 检测新一发开枪事件，每发叠加一次 `ShotSpreadIncrement`

### 5.2 ABaseCharacter 接入

准星直接读取 `CachedCharacter->GetVelocity()` 和 `CachedCharacter->GetCurrentWeapon()`，Character 无需新增接口。角色死亡/无效时自动隐藏准星。

## 6. Widget 实例化

```cpp
void ABasePlayerController::BeginPlay()
{
    // 弹药计数 Widget —— 从 Config（加 FConfigFile 兜底）读取 BP 路径
    // （详见 UIDesign.md 第 4 节）

    // 准星 Widget —— 从全局 Subsystem 获取当前类型
    UCrosshairSettingsSubsystem* Sub = GetGameInstance()->GetSubsystem<UCrosshairSettingsSubsystem>();
    TSubclassOf<UBaseCrosshairWidget> Class = Sub->GetCurrentCrosshairClass();

    CrosshairWidget = CreateWidget<UBaseCrosshairWidget>(this, Class);
    CrosshairWidget->AddToViewport(-1);
    Sub->ApplyToWidget(CrosshairWidget);  // 恢复保存的参数
}
```

Subsystem 本身用 `FConfigFile::Read()` 加载 BP 路径：

```cpp
class UCrosshairSettingsSubsystem : public UGameInstanceSubsystem
{
    UPROPERTY()
    FString FallbackCrosshairClassPath;

    void Initialize()
    {
        FConfigFile ConfigFile;
        ConfigFile.Read(*(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")));
        ConfigFile.GetString(TEXT("/Script/MyFps_Demo.CrosshairSettingsSubsystem"),
            TEXT("FallbackCrosshairClassPath"), FallbackCrosshairClassPath);

        FallbackCrosshairClass = LoadClass<UBaseCrosshairWidget>(nullptr, *FallbackCrosshairClassPath);
        // 仍为空则用 C++ 类兜底
    }
};
```

### 6.1 准星类型注册

```cpp
// 新准星类型在 Subsystem 中注册，ID 唯一
Sub->RegisterCrosshairType("Circle", UMG_Crosshair_Circle::StaticClass());
```

默认类型 "CrossDot" 在 `Subsystem::Initialize()` 中自动注册，映射到 `/Game/.../UMG_Crosshair`。

## 7. 散度效果表（感观参考）

| 条件 | 散度描述 |
|------|---------|
| 站立不动 + 未开火 | 线条紧贴中心点，Gap ≈ BaseGap |
| 慢速行走 | 线条略微外扩，Gap ≈ BaseGap + 10px |
| 全速奔跑 | 线条大幅外扩，Gap ≈ BaseGap + MaxSpread * 0.6 |
| 跳跃/空中 | 最大移动散度 + Gap 最大 |
| 连续开火（第 5 发） | 线条持续外扩（每发递增） |
| 停火 0.5s 后 | 线条逐渐回缩至移动散度水平 |
| ADS（未来 Phase 2） | 准星完全隐藏 |

## 8. 文件清单

| 文件 | 作用 |
|------|------|
| `Source/MyFps_Demo/UI/Crosshair/CrosshairSettingsTypes.h` | `FCrosshairSettings` 结构体 + `UCrosshairSaveGame` 持久化容器 |
| `Source/MyFps_Demo/UI/Crosshair/CrosshairSettingsSubsystem.h` | `UCrosshairSettingsSubsystem` 声明：类型注册表、参数存取、Save/Load、Config 驱动的 BP 路径 |
| `Source/MyFps_Demo/UI/Crosshair/CrosshairSettingsSubsystem.cpp` | Subsystem 实现：SaveGame 读写、Config+FConfigFile 双保险加载 BP 路径、ApplyToWidget/GatherFromWidget |
| `Source/MyFps_Demo/UI/Crosshair/BaseCrosshairWidget.h` | 抽象基类：角色绑定、散度计算、`OnSpreadUpdated`/`OnVisibilityChanged` BlueprintNativeEvent |
| `Source/MyFps_Demo/UI/Crosshair/BaseCrosshairWidget.cpp` | 基类实现：NativeTick 散度计算 + lerp |
| `Source/MyFps_Demo/UI/Crosshair/Crosshair_CrossDot.h` | 十字+点型子类：BindWidget ×5、形状参数、Setter |
| `Source/MyFps_Demo/UI/Crosshair/Crosshair_CrossDot.cpp` | 子类实现：`ApplyVisualSettings`、`ApplySpreadToLines`、覆写全部 BlueprintNativeEvent |
| `Source/MyFps_Demo/BasePlayerController.h` | `Config` 属性 `BulletCounterWidgetClassPath` + `SetCrosshairType(FName)` |
| `Source/MyFps_Demo/BasePlayerController.cpp` | Config+FConfigFile 加载弹药 BP、从 Subsystem 获取准星类、动态创建/切换 |
| `Config/DefaultGame.ini` | `[/Script/MyFps_Demo.CrosshairSettingsSubsystem]` + `[/Script/MyFps_Demo.ABasePlayerController]` 配置 BP 路径 |
| `Source/MyFps_Demo/Weapons/BaseWeapon.h` | 新增 `bool bIsFiring` |
| `Source/MyFps_Demo/GameAbilitySystem/Abilities/Fire/GA_WeaponFire.cpp` | `ActivateAbility` 设置 `bIsFiring = true`，`EndAbility` 设为 `false` |
| `Content/Script/BaseCrosshairWidget.lua` | Lua 占位绑定 |

## 9. 扩展指南

### 9.1 设置界面接入

设置界面不直接操作 Widget，而是操作全局 Subsystem：

```cpp
// 获取全局设置
UCrosshairSettingsSubsystem* Sub = GetGameInstance()->GetSubsystem<UCrosshairSettingsSubsystem>();

// 修改参数
Sub->SetFloatParam("LineLength", 20.0f);
Sub->SetColorParam("CrosshairColor", FLinearColor::Red);
Sub->SetCrosshairType("Circle");
Sub->ApplyToWidget(CrosshairWidget);  // 立刻反映到当前准星
Sub->SaveSettings();                  // 持久化到磁盘

// 切换准星类型
PlayerController->SetCrosshairType("Circle");
Sub->SaveSettings();
```

准星类型下拉框数据源：Subsystem 暴露 `RegisterCrosshairType()`，设置界面遍历 `CrosshairTypeRegistry` 生成选项。

### 9.2 命中反馈（准星变红）

在 `ABaseWeapon` 中加 `OnTargetHit` 委托，Widget 绑定后触发红色闪烁（持续 0.15s 后淡回白色）。

### 9.3 不同武器不同准星样式

新增 `ABaseWeapon::CrosshairType` 枚举（CrossDot / Circle / Chevron），Widget 根据枚举切换可见线条组合和纹理。

### 9.4 ADS 时隐藏准星

Phase 2 ADS 实现后，检查 `State.ADS` GameplayTag，true 时立即隐藏所有线条。

### 9.5 准星射线命中着色

检查中心点对准的目标是否在射程内 + 是否有 ASC（敌方），有则中心点变红（敌方指示）。

### 9.6 十字线逐条参数定制（规划中）

当前 `LineLength` / `LineThickness` / `BaseGap` 对四条线统一生效。如需支持各方向独立参数，改动点：

| 改动 | 内容 |
|------|------|
| 属性拆分 | `LineLength` → `LineLengthTop/Bottom/Left/Right`，同样拆分 Thickness、Gap |
| `ApplyVisualSettings` | 每个 Image 独立读取自己的参数 |
| `ApplySpreadToLines` | 四条线使用各自的 Gap 值 |
| `GatherSettings` / `ApplySettings` | FloatParams key 从 `"LineLength"` 扩展为 `"LineLengthTop"` 等 |
| Setter | `SetLineLength` → 四个独立 Setter 或带方向参数的统一接口 |

> 改动量大，当前不做。先记录在案，供后续圆形/V 字型准星设计时一并考虑。

## 10. 注意事项

- **Tick 启用**：`UUserWidget` 不会自动 Tick，需在 `NativeConstruct` 中确保启用（或依赖 BPCanEverTick 元数据）。
- **角色失效**：角色死亡时 `NativeTick` 检测到 `CachedCharacter` 无效 → 隐藏 Widget 或将透明度设为 0；观战模式可能需要不同准星。
- **ZOrder**：准星必须用 `AddToViewport(-1)` 放到最底层，确保弹药计数等 HUD 不被遮挡。
- **UnLua 限制**：委托绑定和每帧逻辑必须留在 C++ 侧，Lua 仅做占位（见 `UIDesign.md` 第 6 节 API 限制）。
- **性能**：NativeTick 仅做简单数学运算（lerp + 因子计算），开销可忽略。不要在 Tick 中做 Cast / 资源加载。

## 11. 重构计划（待执行）

### 问题

当前 `UBaseCrosshairWidget` 包含 5 个 BindWidget 和全部十字型形状参数，与十字型强绑定。要做圆形/V 字型准星只能 fork 整套代码，无法复用散度计算和设置层。

### 目标

基类只做散度计算与角色绑定，具体渲染由子类通过虚函数实现。

### 变更清单

| 操作 | 内容 |
|------|------|
| **重构** `UBaseCrosshairWidget` | 移除全部 BindWidget、形状参数、`ApplyVisualSettings`、`ApplySpreadToLines`、`SetAllLinesVisibility`。保留：角色绑定、散度计算、`GatherSettings`/`ApplySettings` 抽象接口。新增 `BlueprintNativeEvent`：`OnSpreadUpdated(float)`、`OnVisibilityChanged(bool)` |
| **新建** `UCrosshair_CrossDot` | 继承 `UBaseCrosshairWidget`。迁移全部十字型代码（BindWidget × 5，形状参数，`ApplySpreadToLines`，`ApplyVisualSettings` 等）。覆写 `GatherSettings`/`ApplySettings`/`OnSpreadUpdated` |
| **修改** `ABasePlayerController` | `LoadClass` 路径改为新 BP（`UMG_Crosshair_CrossDot`） |
| **影响** | BaseWeapon / GA_WeaponFire / Subsystem / SaveGame 均无需修改 |

### 新增准星类型成本（重构后）

```
1. 新建 UCrosshair_Circle : UBaseCrosshairWidget
2. 加 1 个 BindWidget Image + Radius/RingThickness
3. 覆写 OnSpreadUpdated → SetRenderScale(1.0 + Spread)
4. 覆写 GatherSettings/ApplySettings → 序列化 Radius
5. 在 Subsystem 注册: RegisterCrosshairType("Circle", UMG_Circle::StaticClass())
```

无需碰基类、Subsystem、PlayerController 或任何其他文件。
