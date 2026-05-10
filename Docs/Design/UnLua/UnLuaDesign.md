# UnLua 集成设计文档

## 1. 设计目标

将 UnLua（Tencent 开源的 Lua 脚本插件）集成到 FPS_Demo 项目中，实现：

- **热重载**：游戏逻辑（武器、角色、UI）可在不重启编辑器的情况下修改即生效
- **C++ / Lua 混合开发**：性能关键路径保留 C++，灵活的逻辑层用 Lua
- **接口绑定**：通过 `IUnLuaInterface` 将 C++ 类绑定到 Lua 脚本

## 2. 插件来源

| 版本 | 分支 | 说明 |
|------|------|------|
| 2.3.6（废弃） | 本地 `UnLua_Demo` 项目 | 仅支持 UE 5.3，在 UE 5.7 上无法编译 |
| **develop**（当前） | GitHub `Tencent/UnLua` develop 分支 | 已适配 UE 5.5+，需少量修补 |

## 3. UE 5.7 兼容性修复

develop 分支自带了 UE 5.5/5.6 部分适配，但仍有两处需手动修正：

### 3.1 `LuaOverridesClass.cpp` — `TObjectPtr<UField>` 类型变更

UE 5.5+ 中 `UField::Next` 从 `UField*` 改为 `TObjectPtr<UField>`。`AddToOwner()` 和 `RemoveFromOwner()` 中遍历链表时使用 `&(*Field)->Next` 取得地址，类型从 `UField**` 变为 `TObjectPtr<UField>*`，导致编译错误。

**修复**：在 `UE_VERSION_NEWER_THAN(5,5,0)` 分支中，`Field` 变量类型改为 `TObjectPtr<UField>*`，条件判断使用 `Field->Get() == this` 替代 `*Field == this`。

### 3.2 `Lua.Build.cs` — 已由 develop 分支修复

- `bEnableUndefinedIdentifierWarnings` → `UndefinedIdentifierWarningLevel`（UE 5.5+）
- `ShadowVariableWarningLevel` → `CppCompileWarningSettings.ShadowVariableWarningLevel`（UE 5.6+）
- `WindowsCompiler.VisualStudio2019` 已用 `!UE_5_4_OR_LATER` 宏包裹

## 4. 项目配置

### 4.1 `FPS_Demo.uproject`

```json
{
    "Name": "UnLua",
    "Enabled": true
}
```

### 4.2 `MyFps_Demo.Build.cs`

```csharp
PrivateDependencyModuleNames.AddRange(new string[] {
    "UnLua",
    "Lua"
});
```

`UnLua` 和 `Lua` 使用私有依赖，不暴露到头文件包含中。

### 4.3 Lua 脚本路径

Lua 脚本存放在 `Content/Script/` 下。`GetModuleName()` 返回的路径相对于此目录。

## 5. C++ 绑定方式

### 5.1 实现 `IUnLuaInterface`

```cpp
// BaseGameMode.h
#include "UnLuaInterface.h"

UCLASS()
class ABaseGameMode : public AGameModeBase, public IUnLuaInterface
{
    virtual FString GetModuleName_Implementation() const override;
};

// BaseGameMode.cpp
FString ABaseGameMode::GetModuleName_Implementation() const
{
    return TEXT("BaseGameMode");
}
```

### 5.2 Lua 脚本模板

```lua
-- Content/Script/BaseGameMode.lua
local M = UnLua.Class()

function M:Initialize()
    print("UnLua bound successfully!")
end

function M:ReceiveBeginPlay()
    -- 覆写蓝图事件
end

return M
```

`UnLua.Class()` 返回一个类表，绑定后所有实例方法即被注入到 C++/蓝图对象中。`Initialize()` 在绑定完成时调用，是 Lua 侧的构造函数。

## 6. UnLua 配置

项目根目录下可创建 `Config/DefaultUnLuaEditor.ini`：

```ini
[/Script/UnLuaEditor.UnLuaEditorSettings]
bAutoStartup=True
bEnableDebug=False
HotReloadMode=Manual
```

| 配置项 | 说明 |
|--------|------|
| `bAutoStartup` | 启动时自动初始化 Lua 环境 |
| `bEnableDebug` | 启用 Lua 调试支持 |
| `HotReloadMode` | `Manual` / `Auto` / `Never` |

## 7. 扩展方向

- **武器逻辑 Lua 化**：将 `ABaseWeapon` 的部分数据逻辑迁移到 Lua
- **UI Lua 化**：HUD 弹药计数通过 `BlueprintNativeEvent` 暴露纯显示逻辑给 Lua 覆写，C++ 侧完成委托绑定和引擎 API 调用
- **能力 Lua 化**：通过动态绑定让 GAS 能力也能绑定 Lua 脚本
- **输入绑定**：使用 `UnLua.Input` 模块在 Lua 中处理 EnhancedInput

## 8. 踩坑记录

| 问题 | 原因 | 解决 |
|------|------|------|
| UnLua 2.3.6 UHT 报 `MetaClass="Object"` 无效 | UE 5.7 要求 MetaClass 用完整路径 | 改用 develop 分支（已修复） |
| Lua.Build.cs 编译报 `VisualStudio2019` 不存在 | UE 5.7 移除了 VS2019 枚举 | develop 分支已用宏包裹 |
| LuaOverridesClass.cpp 编译报 `TObjectPtr` 类型转换错误 | UE 5.5+ `UField::Next` 改为 `TObjectPtr` | 手动添加 `UE_5_5_OR_LATER` 分支适配 |
| Lua `ReceiveBeginPlay()` + `Super` 崩溃 | `BlueprintImplementableEvent` 无 C++ 原生实现，Super 链断裂 | 不覆盖蓝图事件，改用 `Initialize()` |
| Lua `Construct()` + `Super` 崩溃 | UE 5.7 UHT 变更导致 TMap 查询失败 | 同上 |
| Widget `Initialize()` 中 `GetWorld()` 崩溃 | 绑定时 Widget 尚未 AddToViewport，World 上下文无效 | 避免在 `Initialize()` 中调用任何需要 World 的方法 |
| `GetTimerManager()` 在 Lua 侧为 nil | 返回 `FTimerManager&`（非 UObject），UnLua 无法绑定 | 延迟逻辑改用 C++ `SetTimerForNextTick` |
| `GetPawn()` 在 Lua 侧为 nil | `AController::GetPawn()` 反射绑定缺失 | C++ 封装 `GetOwnerCharacter()` UFUNCTION 供 Lua 调用 |
| 委托 `Add(self, "FuncName")` 断言崩 | `DelegateRegistry` 要求 `lua_type == LUA_TFUNCTION`，字符串为 `LUA_TSTRING` | 委托绑定留在 C++ 侧，不通过 Lua `Add` |

### 8.1 UnLua 可行与不可行操作总结

**可行**：
- `UnLua.Class()` 绑定 + `Initialize()` 占位
- 调用项目自定义 `UFUNCTION` 方法
- 访问 `UPROPERTY` 字段（包括 `BindWidget`）
- 覆写 `BlueprintNativeEvent`（C++ 调用，Lua 实现）

**不可行**：
- 覆写 `BlueprintImplementableEvent`（Super 链断裂，断言崩溃）
- 在 `Initialize()` 中访问 World 上下文
- 调用返回非 UObject 引用的引擎方法（如 `GetTimerManager()`）
- 调用部分引擎 `UFUNCTION`（如 `GetPawn()`，反射缺失）
- 动态多播委托的 Lua 侧 `Add` 绑定

### 8.2 `BlueprintImplementableEvent` 覆盖崩溃根因分析

**现象**：Lua 覆盖任何 `BlueprintImplementableEvent`（如 `Construct`、`ReceiveBeginPlay`）后调用时，在 `Map.h.inl:648` 触发 `FindChecked(Pair != nullptr)` 断言崩溃。

**崩溃栈特征**：
```
LUA_OVERRIDES_<ClassName>_<N>.<EventHandlerName>
Map.h.inl:648 → FindChecked → Pair != nullptr assertion
```

**调用链路**：
```
UUserWidget::NativeConstruct()
  → 引擎调用蓝图事件 Construct
    → UnLua 已将其替换为 ULuaFunction
      → ULuaFunction::execCallLua()
        → FunctionRegistry::Invoke()
          → ? → TMap::FindChecked 失败
```

**嫌疑点**（`Plugins/UnLua/Source/UnLua/Private/`）：

| 文件 | 行 | 代码 | 可能性 |
|------|-----|------|--------|
| `Registries/ClassRegistry.cpp` | 256 | `Name2Classes.FindChecked(Key)` | **高** — Override 类通过名称查找时 key 未注册 |
| `Registries/DelegateRegistry.cpp` | 172 | `Delegates.FindChecked(Delegate)` | 中 — 委托注册缺失 |
| `Registries/DelegateRegistry.cpp` | 240 | `Delegates.FindChecked(Delegate)` | 中 — 多播委托注册缺失 |
| `LuaEnv.cpp` | 225 | `AllEnvs.FindChecked(G(L)->mainthread)` | 低 |

**环境信息**：UnLua 2.3.6 已停止维护（Tencent 最后发布版本，仅官方支持到 UE 5.3）。当前使用 develop 分支，在 UE 5.7 上该机制不工作。

### 8.3 解决方案

#### 方案 A：绕过覆盖（已实施，当前使用）

**原理**：Lua 不覆盖任何蓝图事件，委托绑定全部在 C++ 侧完成。Lua 仅通过 `Initialize()` 证明绑定成功，作为未来的扩展入口。

**当前项目中的应用**：
- `BaseGameMode.lua` — `Initialize()` print
- `BaseBulletCounterWidget.lua` — `Initialize()` print
- 委托绑定：C++ `NativeConstruct` → `SetTimerForNextTick` → `TryBindDelegate()` → `AddDynamic()`
- UI 更新：C++ `OnAmmoUpdated()` → `UpdateAmmoDisplay()`

**已可用的 UnLua 能力**：

| 能力 | 状态 |
|------|------|
| `IUnLuaInterface` 绑定 | 可用 |
| `Initialize()` 脚本初始化 | 可用 |
| Lua 调 C++ `UFUNCTION(BlueprintCallable)` | 可用 |
| Lua 读写 `UPROPERTY` | 可用 |
| `UnLua.Input` 模块绑 EnhancedInput | 可用 |
| Lua 热重载 | 可用 |
| C++ 调 Lua 全局函数（`UnLua::Call(L, ...)`） | 可用 |
| 覆盖 `BlueprintImplementableEvent` | **不可用** |

#### 方案 B：自修 UnLua 源码（待实施）

**原理**：UnLua 是 MIT 协议且嵌入在 `Plugins/UnLua/` 下，可直接修改源码。崩溃点在 3 个 `FindChecked` 调用之一，改法是将 `FindChecked` 替换为 `Find` 加 null 守卫。

**实施步骤**：
1. 在 `Map.h.inl:648` 设断点，复现崩溃，定位具体是哪个 TMap 的 `FindChecked`
2. 追溯调用栈，找到 UnLua 源码中对应的 `FindChecked` 调用
3. 改为 `Find` + 空值检查，找不到时跳过而非断言
4. 预估改动量：5 行以内

**风险**：修了一个 `FindChecked` 可能暴露下一个。需要多次定位-修复-测试循环。

#### 方案 C：换用其他 Lua 插件（备选）

| 插件 | UE 5.7 支持 | 特点 |
|------|------------|------|
| [slua-unreal](https://github.com/Tencent/sluaunreal) | 最新版声称支持 | Tencent 维护，同样 MIT 协议，更轻量但生态不如 UnLua |
| [LuaMachine](https://github.com/rdeioris/LuaMachine) | 不确定 | 第三方，非 Tencent 系，需要评估 |
| 手写 Lua 绑定 | — | 工作量大，不推荐 |

### 8.4 推荐架构模式（适用于当前限制）

```
C++ 层（接线层）
  ├── 实现 IUnLuaInterface → 绑定 Lua 脚本
  ├── 负责所有委托绑定、引擎 API 调用
  ├── 调用 Lua 函数：UnLua::Call(L, "FuncName", args...)
  └── 提供 UFUNCTION(BlueprintCallable) 供 Lua 调用

Lua 层（逻辑层）
  ├── Initialize() → 绑定确认
  ├── 调用 C++ UFUNCTION → 数据读写、UI 更新
  └── 实现纯逻辑函数，由 C++ 调用
```
