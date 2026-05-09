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
- **UI Lua 化**：HUD 弹药计数用 Lua 实现，支持热重载调优
- **能力 Lua 化**：通过动态绑定让 GAS 能力也能绑定 Lua 脚本
- **输入绑定**：使用 `UnLua.Input` 模块在 Lua 中处理 EnhancedInput

## 8. 踩坑记录

| 问题 | 原因 | 解决 |
|------|------|------|
| UnLua 2.3.6 UHT 报 `MetaClass="Object"` 无效 | UE 5.7 要求 MetaClass 用完整路径 | 改用 develop 分支（已修复） |
| Lua.Build.cs 编译报 `VisualStudio2019` 不存在 | UE 5.7 移除了 VS2019 枚举 | develop 分支已用宏包裹 |
| LuaOverridesClass.cpp 编译报 `TObjectPtr` 类型转换错误 | UE 5.5+ `UField::Next` 改为 `TObjectPtr` | 手动添加 `UE_5_5_OR_LATER` 分支适配 |
