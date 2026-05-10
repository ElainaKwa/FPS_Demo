# Game Settings System Design

## 1. 概述

游戏总设置系统，提供可扩展、低耦合的设置管理架构。中央单例管理所有设置类别，支持 Apply/Revert 模式和原子化 Save/Load。

**核心目标**：
- 添加新设置类别只需 5 个新文件 + 2 行改动
- 所有访问走单一入口点
- 类别之间零依赖

## 2. 审查发现与修正

### 修正 1（高）：Subsystem 便捷方法必须保持即时生效语义

**问题**：当前 `SetFloatParam/SetColorParam/SetCrosshairType` 直接修改 `CurrentSettings`，调用后立即生效。重构后若改为仅修改 pending data，现有调用方（如 `BasePlayerController::SetCrosshairType`）调用后不再立即生效，需额外 Apply，这是破坏性变更。

**修正**：Subsystem 的便捷方法（`SetFloatParam/SetColorParam/SetCrosshairType`）应同时修改 pending **并自动 Apply**，保持对调用方的即时生效语义。仅在设置 UI 中使用"先修改 pending，后 Apply"的延迟模式。

```
// 即时生效路径（Subsystem 便捷方法）
SetCrosshairType(Name) → Category->SetPendingCrosshairType(Name) → Category->Apply()

// 延迟生效路径（设置 UI）
UI slider 修改 → Category->SetPendingFloatParam(...) → 用户点 Apply → Category->Apply()
```

### 修正 2（中）：Subsystem 监听 Category 的 OnSettingsChanged 时机

**问题**：计划说"Subsystem 监听 → ApplyToWidget"，但未指定绑定时机和方式。

**修正**：在 `UCrosshairSettingsSubsystem::Initialize()` 中，获取 Category 后绑定 `OnSettingsChanged`，回调中调用 `ApplyToWidget` 刷新当前活跃的准星 Widget。

```cpp
void UCrosshairSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency(UGameSettingsSubsystem::StaticClass());
    // ... registry setup ...

    if (auto* Category = GetCategory())
    {
        Category->OnSettingsChanged.AddUObject(this, &UCrosshairSettingsSubsystem::OnCrosshairSettingsChanged);
    }
}

void UCrosshairSettingsSubsystem::OnCrosshairSettingsChanged()
{
    if (ActiveCrosshairWidget)
    {
        ApplyToWidget(ActiveCrosshairWidget);
    }
}
```

### 修正 3（中）：GatherFromWidget 应写入 CurrentSettings

**问题**：当前 `GatherFromWidget` 从 Widget 读取当前视觉状态写入 `CurrentSettings`。重构后若写入 `PendingSettings`，会覆盖用户在设置 UI 中的未提交修改。

**修正**：`GatherFromWidget` 应写入 `CurrentSettings`（表示已生效的状态），通过 Category 提供专门的 `WriteCurrentSettings()` 方法。这确保切换准星类型时不会丢失设置 UI 中的未提交修改。

### 修正 4（低）：Phase 2 无数据双写过渡期

**问题**：原计划 Phase 2 注册了 Category 但 Subsystem 仍持有 `CurrentSettings`，两处数据可能不一致。

**修正**：Phase 2 应立即将 Subsystem 的数据同步到 Category（`Subsystem->CurrentSettings → Category->CurrentSettings + PendingSettings`），然后 Subsystem 的读写全部委托到 Category。避免两份数据并存的过渡期。

### 修正 5（低）：旧存档迁移与 UCrosshairSaveGame 移除的时序

**问题**：迁移逻辑需要 `Cast<UCrosshairSaveGame>`，但 Phase 3 要移除这个类。如果先移除类再加载，迁移代码会编译失败。

**修正**：Phase 3 分两步：(a) 先把 Subsystem 委托到 Category，保留 `UCrosshairSaveGame` 供迁移逻辑使用；(b) 验证迁移完成后，再移除 `UCrosshairSaveGame`。或在 `UGameSettingsSubsystem::LoadAll()` 中用原始 `FConfigFile` 读取旧存档 slot 的数据，避免依赖 `UCrosshairSaveGame` 类型。

## 3. 设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| Category 基类 | `UObject`（非 Subsystem） | 强制所有访问走中央单例，生命周期由中央管理 |
| Save/Load | 单个 `USaveGame` + 每类 UPROPERTY struct | 类型安全，原子保存，无需额外模块依赖 |
| 注册机制 | 中央 Subsystem `Initialize()` 显式注册 | 简单可预测，加新类只加一行 |
| Apply/Revert | Pending + Current 双数据 + dirty flag | 完整撤销能力，dirty flag 避免手动比较 USTRUCT |
| 迁移策略 | 增量式，不破坏现有功能 | 先建框架 → 接入中央管理 → 重构现有代码 |

### 为什么不用 UGameInstanceSubsystem 做 Category？

如果 Category 本身是 `UGameInstanceSubsystem`，任何代码都可以通过 `GetSubsystem<UAudioSettingsCategory>()` 绕过中央管理器，破坏单例入口。用 `UObject` 由中央管理器拥有，确保所有访问都走 `GetSubsystem<UGameSettingsSubsystem>()->GetCategory<T>()`。

### 为什么不用 JSON 序列化？

`TMap<FName, FString>` + `FJsonObjectConverter` 虽然更灵活，但需要引入 `JsonUtilities` 模块依赖，且丢失类型安全。当前项目规模不大，`UPROPERTY` 方案足够。如果将来需要 mod 支持未知 Category，再迁移到 JSON。

## 4. 类层次

```
UGameInstanceSubsystem
  └── UGameSettingsSubsystem          [中央单例]
        拥有所有 Category 实例
        统一 SaveAll/LoadAll/ApplyAll/RevertAll
        GetCategory<T>() / GetCategoryByName()

UObject
  └── UGameSettingsCategory           [抽象基类]
        CategoryName, bHasPendingChanges
        Apply() / Revert() / ResetToDefaults()
        Serialize(FGameSettingsSaveData&) / Deserialize(...)
        OnSettingsChanged delegate
        │
        ├── UCrosshairSettingsCategory  [从 Subsystem 提取数据]
        ├── UAudioSettingsCategory      [计划中]
        └── UUISettingsCategory         [计划中]

USaveGame
  └── UGameSettingsSaveGame           [原子保存]
        FGameSettingsSaveData
          ├── FCrosshairSettings CrosshairSettings
          ├── FAudioSettings AudioSettings
          └── FUISettings UISettings

UUserWidget
  └── UGameSettingsWidgetBase         [设置面板基类]
        InitializePanel(Category)
        OnCategoryInitialized() / OnSettingsRefreshed()
        │
        ├── UCrosshairSettingsWidget
        ├── UAudioSettingsWidget
        └── UUISettingsWidget

UCrosshairSettingsSubsystem           [重构后 — 只保留运行时逻辑]
  移除: CurrentSettings, SaveSettings(), LoadSettings()
  保留: TypeRegistry, FallbackClass, ApplyToWidget(), GatherFromWidget()
  通过 GetCategory<UCrosshairSettingsCategory>() 读写数据
```

## 5. 关键接口

### 5.1 UGameSettingsSubsystem

```cpp
UCLASS()
class UGameSettingsSubsystem : public UGameInstanceSubsystem
{
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize(FSubsystemCollectionBase& Collection) override;

    // --- Category 访问 ---

    template<typename T> T* GetCategory() const;
    UFUNCTION(BlueprintPure) UGameSettingsCategory* GetCategoryByName(FName Name) const;
    UFUNCTION(BlueprintPure) TArray<FName> GetCategoryNames() const;

    // --- 批量操作 ---

    UFUNCTION(BlueprintCallable) void ApplyAll();
    UFUNCTION(BlueprintCallable) void RevertAll();
    UFUNCTION(BlueprintCallable) void ResetAllToDefaults();
    UFUNCTION(BlueprintCallable) void SaveAll();
    UFUNCTION(BlueprintCallable) void LoadAll();

    // --- 单 Category 操作 ---

    UFUNCTION(BlueprintCallable) void ApplyCategory(FName);
    UFUNCTION(BlueprintCallable) void RevertCategory(FName);
    UFUNCTION(BlueprintCallable) void ResetCategoryToDefaults(FName);

    // --- 全局通知 ---

    UPROPERTY(BlueprintAssignable)
    FOnSettingsApplied OnSettingsAppliedGlobal;

private:
    UPROPERTY() TMap<FName, UGameSettingsCategory*> Categories;
    UPROPERTY() TArray<FName> CategoryOrder;

    void RegisterCategory(UGameSettingsCategory* Category);

    static const FString SaveSlotName;
    static const int32 SaveSlotIndex = 0;
};
```

### 5.2 UGameSettingsCategory

```cpp
UCLASS(Abstract, Blueprintable)
class UGameSettingsCategory : public UObject
{
public:
    virtual FName GetCategoryName() const;
    virtual TSubclassOf<UGameSettingsWidgetBase> GetSettingsWidgetClass() const;
    virtual void Apply() = 0;             // Pending→Current, broadcast OnSettingsChanged
    virtual void Revert() = 0;            // Current→Pending, broadcast OnSettingsChanged
    virtual void ResetToDefaults() = 0;
    virtual bool HasPendingChanges() const { return bHasPendingChanges; }
    virtual void Serialize(FGameSettingsSaveData& Out) const = 0;
    virtual void Deserialize(const FGameSettingsSaveData& In) = 0;
    virtual void OnLoaded() {}             // 加载后推送到游戏系统

    UPROPERTY(BlueprintAssignable)
    FOnCategorySettingsChanged OnSettingsChanged;

protected:
    FName CategoryName;
    bool bHasPendingChanges = false;       // Pending mutator 设 true, Apply/Revert 清 false
};
```

### 5.3 UCrosshairSettingsCategory

```cpp
UCLASS()
class UCrosshairSettingsCategory : public UGameSettingsCategory
{
public:
    const FCrosshairSettings& GetCurrentSettings() const;
    const FCrosshairSettings& GetPendingSettings() const;
    void SetPendingCrosshairType(FName NewType);
    void SetPendingFloatParam(FName Key, float Value);
    void SetPendingColorParam(FName Key, FLinearColor Value);
    void WriteCurrentSettings(const FCrosshairSettings& Settings);  // GatherFromWidget 用，直接写 Current

    virtual void Apply() override;
    virtual void Revert() override;
    virtual void ResetToDefaults() override;
    virtual void Serialize(FGameSettingsSaveData& Out) const override;
    virtual void Deserialize(const FGameSettingsSaveData& In) override;
    virtual void OnLoaded() override;

private:
    UPROPERTY() FCrosshairSettings CurrentSettings;
    UPROPERTY() FCrosshairSettings PendingSettings;
    static const FCrosshairSettings DefaultSettings;
};
```

### 5.4 重构后的 UCrosshairSettingsSubsystem

```cpp
UCLASS()
class UCrosshairSettingsSubsystem : public UGameInstanceSubsystem
{
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override
    {
        // 保证中央管理器先初始化
        Collection.InitializeDependency(UGameSettingsSubsystem::StaticClass());
        SetupDefaultRegistry();

        // 绑定 Category 变更通知 → 刷新活跃准星 Widget
        if (auto* Category = GetCategory())
        {
            Category->OnSettingsChanged.AddUObject(this, &UCrosshairSettingsSubsystem::OnCrosshairSettingsChanged);
        }
    }

    // 保留：TypeRegistry, FallbackClass, ApplyToWidget, GatherFromWidget
    // SetFloatParam/SetColorParam/SetCrosshairType：修改 category pending + 自动 Apply（即时生效）
    // GatherFromWidget：通过 Category->WriteCurrentSettings() 写入 CurrentSettings
    // 移除：CurrentSettings, SaveSettings(), LoadSettings()

    UCrosshairSettingsCategory* GetCategory() const;

private:
    UPROPERTY() TMap<FName, TSubclassOf<UBaseCrosshairWidget>> CrosshairTypeRegistry;
    UPROPERTY() TSubclassOf<UBaseCrosshairWidget> FallbackCrosshairClass;
    UPROPERTY() FString FallbackCrosshairClassPath;
    UPROPERTY() UBaseCrosshairWidget* ActiveCrosshairWidget;  // 当前活跃准星，供 OnSettingsChanged 刷新

    void OnCrosshairSettingsChanged();  // 回调：ApplyToWidget(ActiveCrosshairWidget)
};
```

### 5.5 UGameSettingsSaveGame

```cpp
USTRUCT(BlueprintType)
struct FGameSettingsSaveData
{
    GENERATED_BODY()

    UPROPERTY() FCrosshairSettings CrosshairSettings;
    // 新类别加一行：
    // UPROPERTY() FAudioSettings AudioSettings;
    // UPROPERTY() FUISettings UISettings;
};

UCLASS()
class UGameSettingsSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() FGameSettingsSaveData SaveData;
};
```

### 5.6 UGameSettingsWidgetBase

```cpp
UCLASS(Abstract, Blueprintable)
class UGameSettingsWidgetBase : public UUserWidget
{
public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable)
    void InitializePanel(UGameSettingsCategory* InCategory);

protected:
    UPROPERTY() UGameSettingsCategory* TargetCategory;
    UPROPERTY() UGameSettingsSubsystem* SettingsSubsystem;

    UFUNCTION(BlueprintNativeEvent) void OnCategoryInitialized();
    UFUNCTION(BlueprintNativeEvent) void OnSettingsRefreshed();
};
```

## 6. 数据流

### 6.1 Save

```
SaveAll()
  → NewObject<UGameSettingsSaveGame>
  → foreach category: category->Serialize(SaveData)
  → UGameplayStatics::SaveGameToSlot(SaveObj, "GameSettings", 0)
```

### 6.2 Load

```
LoadAll()
  → LoadGameFromSlot("GameSettings", 0)
  → 若无新存档，尝试旧 "CrosshairSettings" slot 迁移
  → foreach category: category->Deserialize(SaveData)
  → foreach category: category->OnLoaded()
```

### 6.3 Apply

```
ApplyAll()
  → foreach category: category->Apply()
      → UCrosshairSettingsCategory::Apply()
          CurrentSettings = PendingSettings;
          bHasPendingChanges = false;
          OnSettingsChanged.Broadcast();  // Subsystem 监听 → ApplyToWidget
  → OnSettingsAppliedGlobal.Broadcast()
  → 可选: SaveAll()
```

### 6.4 Revert

```
RevertAll()
  → foreach category: category->Revert()
      → UCrosshairSettingsCategory::Revert()
          PendingSettings = CurrentSettings;
          bHasPendingChanges = false;
          OnSettingsChanged.Broadcast();  // SettingsWidget 刷新 slider
```

## 7. UI 发现机制

1. `UGameSettingsSubsystem::GetCategoryNames()` → UI 创建 tab 按钮
2. `category->GetSettingsWidgetClass()` → 创建对应 Widget
3. `widget->InitializePanel(category)` → 绑定数据
4. 每个类别的 Widget 路径通过 DefaultGame.ini 配置（同现有 FallbackCrosshairClassPath 模式）

总设置界面结构：
```
UMG_SettingsPanel
  ├── UMG_CategoryList (左侧 tab 按钮，动态生成)
  ├── UMG_ContentArea (右侧，显示选中 Category 的 Widget)
  ├── Apply 按钮
  ├── Revert 按钮
  └── Reset to Defaults 按钮
```

## 8. 目录结构

```
Source/MyFps_Demo/
  GameSettings/
    GameSettingsSubsystem.h/.cpp
    GameSettingsCategory.h/.cpp
    GameSettingsTypes.h                    # FGameSettingsSaveData, UGameSettingsSaveGame
    GameSettingsWidgetBase.h/.cpp
    Categories/
      Crosshair/
        CrosshairSettingsCategory.h/.cpp
        CrosshairSettingsWidget.h/.cpp
      Audio/                               # 计划中
        AudioSettingsTypes.h               # FAudioSettings USTRUCT
        AudioSettingsCategory.h/.cpp
        AudioSettingsWidget.h/.cpp
      UI/                                  # 计划中
        UISettingsTypes.h
        UISettingsCategory.h/.cpp
        UISettingsWidget.h/.cpp

  UI/Crosshair/
    CrosshairSettingsTypes.h               # 修改：移除 UCrosshairSaveGame
    CrosshairSettingsSubsystem.h/.cpp      # 修改：数据委托到 Category
    其余文件不变
```

## 9. 添加新 Category 步骤

以 Audio 为例：

1. `GameSettings/Categories/Audio/AudioSettingsTypes.h` — 定义 `FAudioSettings` USTRUCT
2. `GameSettingsTypes.h` — 在 `FGameSettingsSaveData` 中加一行 `UPROPERTY() FAudioSettings AudioSettings`
3. `AudioSettingsCategory.h/.cpp` — 继承 `UGameSettingsCategory`，实现虚函数
4. `AudioSettingsWidget.h/.cpp` — 继承 `UGameSettingsWidgetBase`
5. `GameSettingsSubsystem::Initialize()` — 加一行 `RegisterCategory(NewObject<UAudioSettingsCategory>(this))`
6. `AudioSettingsCategory::Apply()` 中推送音量到游戏音频系统

**共 5 个新文件 + 2 行改动。无需修改任何已有 Category 或中央管理器逻辑。**

## 10. 迁移计划

增量式，每步可编译可运行。

### Phase 1：搭建框架（不改现有代码）

- 创建 `GameSettings/` 目录下所有框架类
- 创建 `UCrosshairSettingsCategory`（复制现有 Subsystem 的数据）
- 编译通过，未接入任何功能

### Phase 2：接入中央管理

- `UGameSettingsSubsystem::Initialize()` 注册 Crosshair Category
- **立即同步**：将 Subsystem 的 `CurrentSettings` 写入 Category 的 `CurrentSettings + PendingSettings`，无数据双写过渡期
- Subsystem 的所有读写立即委托到 Category（`GetCategory()->GetCurrentSettings()` 等）
- 实现 `LoadAll()` 含旧存档迁移逻辑
- `BasePlayerController` 不变

### Phase 3a：重构 CrosshairSubsystem（委托）

- `GetCategory()` helper 走中央管理器
- `SetFloatParam/SetColorParam/SetCrosshairType` 改为修改 category 的 pending data **并自动 Apply**（保持即时生效语义）
- `GatherFromWidget` 通过 Category 的 `WriteCurrentSettings()` 写入 CurrentSettings（非 PendingSettings）
- 移除 `SaveSettings()/LoadSettings()/CurrentSettings`
- 绑定 `Category->OnSettingsChanged` → `ApplyToWidget`
- **保留 `UCrosshairSaveGame`** 供旧存档迁移使用
- 回归测试：Crosshair UI 仍正常工作

### Phase 3b：移除 UCrosshairSaveGame

- 确认旧存档迁移逻辑无需 `Cast<UCrosshairSaveGame>`（改用原始数据读取或其他方式）
- `CrosshairSettingsTypes.h` 移除 `UCrosshairSaveGame`
- 回归测试

### Phase 4：添加 Audio / UI Category

- 按上述 6 步模板添加

### Phase 5：总设置 UI

- 创建 `UMG_SettingsPanel`（tab list + content area + Apply/Revert 按钮）
- 动态发现 Category，创建对应 Widget

## 11. Build.cs 变更

```csharp
// 无需新模块依赖 — USaveGame/UGameInstanceSubsystem/UObject 均在已有模块中
// 仅添加 include path:
PublicIncludePaths.Add("MyFps_Demo/GameSettings");
```

如果将来需要 JSON 序列化（如 mod 支持未知 Category），再添加 `"JsonUtilities"` 和 `"Json"`。

## 12. 旧存档迁移

在 `UGameSettingsSubsystem::LoadAll()` 中：

1. 尝试加载 "GameSettings" slot（新格式）
2. 若不存在，尝试加载 "CrosshairSettings" slot（旧格式）
3. 若旧格式存在：
   - Cast 到 `UCrosshairSaveGame`，提取 `FCrosshairSettings`
   - 创建新 `UGameSettingsSaveGame`，填入 CrosshairSettings
   - 保存到 "GameSettings" slot
   - 删除旧 "CrosshairSettings" slot
   - Log 迁移确认
4. 正常加载新格式

## 13. 初始化顺序

`UCrosshairSettingsSubsystem::Initialize()` 中调用 `Collection.InitializeDependency(UGameSettingsSubsystem::StaticClass())`，确保中央管理器先于 Crosshair Subsystem 初始化。这样 `GetCategory()` 在 Subsystem 初始化期间即可使用。

## 14. 已知限制与未来方向

| 限制 | 说明 | 迁移方案 |
|------|------|---------|
| 新 Category 需修改 SaveData | `FGameSettingsSaveData` 需加一行 UPROPERTY | 可接受，改动局限在一个文件 |
| 不支持 mod 添加的 Category | 未知 Category 无法在编译期加入 SaveData | 迁移到 `TMap<FName, FString>` + JSON |
| Widget 路径需手动配置 | 每个 Category 的 WidgetClass 路径需在 DefaultGame.ini 配置 | 同现有 FallbackCrosshairClassPath 模式 |
| Pending 数据可能过时 | 外部代码直接修改游戏系统时，Pending 与实际状态不一致 | 所有修改必须走 Category 的 Pending mutator |
