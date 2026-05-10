# UI 弹药显示设计文档

## 1. 设计目标

用 UnLua + UMG 实现 FPS 弹药计数器 HUD，遵循 MVC 分层：

- **Model**：`ABaseCharacter` → `OnBulletCountUpdated` 委托广播弹药变化
- **View**：`UBaseBulletCounterWidget`（C++ 类 + Blueprint Designer 构图 + Lua 绑定）
- **Controller**：`ABasePlayerController` 仅负责创建 Widget，不参与 UI 更新

## 2. 架构设计

### 2.1 分层数据流

```
Model (ABaseCharacter)          View (UMG_BulletCounter)          Controller (ABasePlayerController)
┌───────────────────┐           ┌────────────────────────┐        ┌─────────────────────┐
│ Weapon ammo change│           │ Widget Blueprint (UE)  │        │ BeginPlay()         │
│   ↓               │           │   CanvasPanel          │        │   → LoadClass(BP)   │
│ UpdateWeaponHUD() │           │     Border (AmmoBorder)│        │   → CreateWidget()  │
│   ↓               │           │       TextBlock        │        │   → AddToViewport() │
│ OnBulletCount     │──广播──→  │        (AmmoText)      │        │ (不再管 UI)         │
│   _Updated        │           │                        │        └─────────────────────┘
│   .Broadcast()    │           │ C++ NativeConstruct()  │
└───────────────────┘           │   → 样式配置           │
                                │   → SetTimerForNextTick│
                                │     → TryBindDelegate()│
                                │       → AddDynamic()   │
                                │                         │
                                │ Lua: Initialize()       │
                                │   → print("bound")      │
                                │   (预留 OnAmmoChanged)  │
                                └────────────────────────┘
```

### 2.2 MVC 职责

| 层 | 文件 | 职责 |
|---|------|------|
| **Model** | `ABaseCharacter` + `ABaseWeapon` | 数据源，通过 `OnBulletCountUpdated` 广播 |
| **View** | `UBaseBulletCounterWidget` + `BP` + `.lua` | 控件构图（BP Designer）、样式配置（C++）、Lua 绑定（UnLua） |
| **Controller** | `ABasePlayerController` | 仅 `BeginPlay` 中 `CreateWidget` + `AddToViewport` |

### 2.3 类层级

```
UUserWidget (Engine)
  └── UBaseBulletCounterWidget (C++)
        │  implements IUnLuaInterface
        │  meta = (BindWidget) UTextBlock AmmoText
        │  meta = (BindWidget) UBorder AmmoBorder
        └── UMG_BulletCounter (Widget Blueprint, 编辑器创建)
              └── Designer 构图：CanvasPanel → Border → TextBlock
```

## 3. 控件组成

### 3.1 Blueprint Designer（编辑器）

`UMG_BulletCounter` Blueprint 的 Designer 层级：

```
CanvasPanel (根)
  └─ Border (AmmoBorder)
       │  锚点: 右上角
       │  对齐: (1, 0)  右对齐
       │  位置: (-60, 40)
       │  自动大小: true
       └─ TextBlock (AmmoText)
            文字: "0 / 0"
            字号: 28
            颜色: 白色
            阴影: 黑色 #(1,1)
```

### 3.2 C++ NativeConstruct

控件树已在 Blueprint Designer 中创建，C++ 通过 `BindWidget` 获得引用后仅做样式配置：

```cpp
void UBaseBulletCounterWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Border: 半透明黑底
    AmmoBorder->SetBrushColor(FLinearColor(0, 0, 0, 0.6));
    AmmoBorder->SetPadding(FMargin(8, 4));

    // TextBlock: 白字 + 黑阴影
    AmmoText->SetFont(28);
    AmmoText->SetColorAndOpacity(White);
    AmmoText->SetShadowOffset(FVector2D(1, 1));
    AmmoText->SetText("0 / 0");

    // 下一帧绑定委托（等 Pawn 就位）
    GetWorld()->GetTimerManager().SetTimerForNextTick(
        this, &UBaseBulletCounterWidget::TryBindDelegate);
}
```

### 3.3 委托绑定（C++）

因为 UnLua 在 UE 5.7 上覆盖 `BlueprintImplementableEvent` 会崩溃（见第 6 节），委托绑定必须在 C++ 侧完成：

```cpp
void UBaseBulletCounterWidget::TryBindDelegate()
{
    APlayerController* PC = GetOwningPlayer();
    if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(PC->GetPawn()))
    {
        BaseChar->OnBulletCountUpdated.AddDynamic(
            this, &UBaseBulletCounterWidget::OnAmmoUpdated);

        // 首次同步弹药显示
        UpdateAmmoDisplay(Weapon->CurrentBullets, Weapon->GetEffectiveMagazineSize());
    }
}
```

### 3.4 Lua 绑定

Widget 实现 `IUnLuaInterface`，`GetModuleName()` 返回 `"BaseBulletCounterWidget"`。
Lua 脚本仅做占位绑定，不覆盖任何蓝图事件：

```lua
-- Content/Script/BaseBulletCounterWidget.lua
local M = UnLua.Class()

function M:Initialize()
    print("BaseBulletCounterWidget Lua: bound successfully")
end

return M
```

## 4. Widget 实例化

BP 路径通过 `FConfigFile::Read()` 直接从 `DefaultGame.ini` 读取，配置在 `DefaultGame.ini` 中：

```cpp
class ABasePlayerController : public APlayerController
{
    UPROPERTY()
    FString BulletCounterWidgetClassPath;

    void BeginPlay()
    {
        // 从 DefaultGame.ini 直接读磁盘，绕过 GConfig 缓存分层问题
        FConfigFile ConfigFile;
        ConfigFile.Read(*(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")));
        ConfigFile.GetString(TEXT("/Script/MyFps_Demo.ABasePlayerController"),
            TEXT("BulletCounterWidgetClassPath"), BulletCounterWidgetClassPath);

        UClass* Class = LoadClass<UBaseBulletCounterWidget>(nullptr, *BulletCounterWidgetClassPath);
        CreateWidget<UBaseBulletCounterWidget>(this, Class)->AddToViewport();
    }
};
```

```ini
; Config/DefaultGame.ini
[/Script/MyFps_Demo.ABasePlayerController]
BulletCounterWidgetClassPath=/Game/FPSContent/Blueprint/UI/BulletCounter/UMG_BulletCounter.UMG_BulletCounter_C
```

> `TSubclassOf<T>` 不支持 Config 延迟解析；`FConfigFile::Read()` 直接从磁盘读取，稳定可靠。不使用 `UPROPERTY(Config)` 以避免 `SaveConfig()` 覆盖空值。

## 5. 数据流（完整链路）

```
开火 / 换弹
  → ABaseWeapon / GA 能力
    → WeaponOwner->UpdateWeaponHUD(current, max)
      → ABaseCharacter::UpdateWeaponHUD()
        → OnBulletCountUpdated.Broadcast(current, max)
          → [C++] UBaseBulletCounterWidget::OnAmmoUpdated()
            → UpdateAmmoDisplay(current, max)
              → AmmoText->SetText("{current} / {max}")
```

## 6. 踩坑记录

### 6.1 UnLua 覆盖蓝图事件崩溃（核心坑）

| 尝试 | 结果 | 原因 |
|------|------|------|
| Lua `ReceiveBeginPlay()` + `Super` | GameMode: `Super` 为 nil；PlayerController: `FindChecked` 断言崩 | `BlueprintImplementableEvent` 无 C++ 原生实现，Super 链断裂 |
| Lua `Construct()` + `Super` | Widget: `FindChecked` 断言崩 | UnLua 覆盖类在 UE 5.7 UHT 变更下 TMap 查询失败 |
| Lua 仅 `Initialize()` | 正常 | `Initialize` 不走蓝图事件覆盖链路 |

**结论**：UnLua develop 分支在 UE 5.7 上不可覆盖任何 `BlueprintImplementableEvent`。委托绑定必须在 C++ 侧完成。

### 6.2 UnLua Widget 生命周期限制

| 尝试 | 结果 | 原因 |
|------|------|------|
| Lua `Initialize()` 中调 `self:GetWorld()` | `'this' pointer is misaligned` 崩溃 | `Initialize()` 在 Widget 绑定时调用，此时 Widget 尚未 AddToViewport，World 上下文无效 |
| Lua `Initialize()` 中调 `World:GetTimerManager()` | `method 'GetTimerManager' is not callable (a nil value)` | UnLua 无法调用返回非 UObject 引用的引擎方法 |
| Lua 中调 `PC:GetPawn()` | `method 'GetPawn' is not callable (a nil value)` | UnLua 对 `AController::GetPawn()` 的反射绑定缺失，方法为 nil |
| C++ `DelayedInit` → `OnWidgetReady`（BlueprintNativeEvent）→ Lua 覆写 | 成功调用，但 `GetPawn` 仍为 nil | BlueprintNativeEvent 可被 Lua 覆写，但引擎 API 限制仍存在 |
| 委托 `Add(self, "OnAmmoUpdated")` | `DelegateRegistry.cpp` 断言崩：`lua_type(L, Index) == 6` | UnLua 委托 `Add` 要求第二个参数为 Lua function（type 6），字符串（type 4）不匹配 |

**结论**：UnLua develop 分支在 UE 5.7 上存在以下限制，导致 Widget 逻辑无法完整迁移到 Lua：
1. `Initialize()` 时机过早，Widget 无有效 World 上下文
2. 引擎 API（`GetTimerManager`、`GetPawn` 等）在 Lua 侧不可调用
3. 动态多播委托的 `Add` 方法不接受字符串函数名，仅接受 Lua function 引用，但 UnLua 的委托注册机制对 Lua function 类型校验严格，易触发断言

当前 Widget 逻辑必须留在 C++ 侧，Lua 仅做占位绑定。

### 6.3 纯 C++ Widget 不渲染

`CreateWidget<UBaseBulletCounterWidget>(this, UBaseBulletCounterWidget::StaticClass())` 创建纯 C++ Widget 时，Slate 控件树不会被搭建——`NativeConstruct` 中新建的 Canvas/Border/TextBlock 发生在 `RebuildWidget()` 之后，无法进入渲染管线。

**解决**：必须通过 Widget Blueprint 子类实例化，控件树在 Blueprint Designer 中搭建（引擎负责 `RebuildWidget` 转换），C++ 用 `meta = (BindWidget)` 按名称绑定引用。

### 6.4 `ConstructorHelpers::FClassFinder` 只能放构造函数

`FClassFinder` 内部使用 `FObjectFinder`，UE 限制其只能在构造函数中使用。`BeginPlay` 中需用 `LoadClass<T>()`。

### 6.5 `UFUNCTION(BlueprintCallable)` 对 UnLua 可见性

Lua 只能调用标记了 `UFUNCTION` 的方法。普通 C++ 成员函数（如最初的 `UpdateAmmoDisplay`）Lua 侧为 nil。

### 6.6 `AController::Character` 成员名冲突

在 `ABasePlayerController` 中命名局部变量 `Character` 会隐藏 `AController::Character` 成员（`TObjectPtr<ACharacter>`），触发 `C4458` 编译错误。

### 6.7 `TSubclassOf` 作为 Config 属性失败

`UPROPERTY(Config) TSubclassOf<T>` 在 CDO 初始化时解析，此时蓝图类可能尚未加载完毕，导致属性为 nullptr。改用 `FString` 存路径 + `LoadClass<T>()` 运行时解析。另加 `FConfigFile::Read()` 直接从磁盘读取 `DefaultGame.ini`，绕过 GConfig 缓存分层问题。

### 6.8 `UWidget::Slot` 成员名冲突

在 Widget 子类方法中，局部变量名 `Slot` 会隐藏 `UWidget::Slot` 成员（`TObjectPtr<UPanelSlot>`），触发 `C4458` 编译错误。统一使用 `CanvasSlot` 作为局部变量名。

## 7. 文件清单

| 文件 | 作用 |
|------|------|
| `Source/MyFps_Demo/UI/BulletCounter/BaseBulletCounterWidget.h` | Widget C++ 类声明：BindWidget、IUnLuaInterface、委托绑定 |
| `Source/MyFps_Demo/UI/BulletCounter/BaseBulletCounterWidget.cpp` | Widget 实现：样式配置、委托绑定、弹药更新 |
| `Source/MyFps_Demo/BasePlayerController.h` | Controller 声明，`BulletCounterWidgetClassPath` 属性 |
| `Source/MyFps_Demo/BasePlayerController.cpp` | `FConfigFile::Read()` 读取 BP 路径 → `LoadClass` → `CreateWidget` |
| `Config/DefaultGame.ini` | `[/Script/MyFps_Demo.ABasePlayerController]` → `BulletCounterWidgetClassPath` |

## 8. 扩展指南

### 8.1 Lua 接管弹药更新（当前不可行）

UnLua develop 分支在 UE 5.7 上存在多处 API 限制（见 6.2 节），当前无法将 Widget 逻辑完整迁移到 Lua。需要等待以下问题解决：

1. `Initialize()` 时机问题 — Widget 绑定时无有效 World 上下文
2. 引擎 API 不可调用 — `GetTimerManager()`、`GetPawn()` 等在 Lua 侧为 nil
3. 委托 `Add` 类型校验 — 不接受字符串函数名，Lua function 引用触发断言

可行的折中方案：C++ 侧完成所有"接线"（委托绑定、引擎 API 调用），通过 `BlueprintNativeEvent` 将纯显示逻辑暴露给 Lua 覆写。待 UnLua 更新后逐步将更多逻辑迁入 Lua。

1. 新建 Widget Blueprint 子类，继承相同架构
2. 或在 `UMG_BulletCounter` 中添加更多控件
3. Lua 文件改名为全局 HUD，统一管理所有 UI 元素
