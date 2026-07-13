# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

Generate project files:
```
# Right-click FPS_Demo.uproject → Generate Visual Studio project files
# Or via command line:
Engine/Build/BatchFiles/Build.bat FPS_DemoEditor Win64 Development -Project="FPS_Demo.uproject"
```

Open `FPS_Demo.sln` in Visual Studio 2022, build **Development Editor | Win64**. Then launch the editor from the .sln or run the built binary.

No automated test suite exists — validation is manual in-editor.

## Architecture

This is a UE 5.7 first-person shooter using **Gameplay Ability System (GAS)** as its core architecture. The active module is `Source/MyFps_Demo/`; `Source/FPS_Demo/` is legacy template code and must be ignored.

### GAS Flow

All weapon behaviors are GAS abilities — the character owns an `AbilitySystemComponent` and `AttributeSet`, abilities read weapon data through an interface, and communicate via GameplayTags and GameplayEffects.

```
ABaseCharacter (implements IAbilitySystemInterface + IBaseWeaponHolder)
  ├── UBaseAbilitySystemComponent   — manages abilities, holds current weapon ref
  ├── UBaseWeaponAttributeSet       — CurrentAmmo / MaxAmmo (network-replicated)
  ├── ABaseWeapon                   — data container + visualization only, no logic
  └── GA_WeaponFire / GA_WeaponReload — abilities drive behavior, read weapon data
```

**Ability activation chain**: InputAction → Character method → `ASC->TryActivateAbility()` → GA ActivateAbility → CommitAbility → perform logic → EndAbility

**Ammo consumption order**: First `--Weapon->CurrentBullets`, then apply `GE_AmmoCost` to AttributeSet, then sync AttributeSet ← Weapon. The AttributeSet is the authoritative replicated value; Weapon.CurrentBullets is the local mirror.

### Key Design Decisions

- **ABaseWeapon is data-only**: No fire/reload logic lives on the weapon. Abilities read weapon properties through `GetEffective*()` methods that aggregate attachment modifiers.
- **IBaseWeaponHolder interface**: All character↔weapon communication goes through this interface, so abilities don't depend on the concrete character class.
- **Tag-driven blocking**: `State.Reloading` blocks fire; `Event.OutOfAmmo` triggers auto-reload. No direct calls between abilities.
- **Blueprint subclasses required**: C++ ability classes (`GA_WeaponFire`, `GA_WeaponReload`) and GameplayEffects must be subclassed as Blueprints in the editor — `TSubclassOf<UGameplayAbility>` and GE assets only work as Blueprint resources.

## Critical Rules

- **Ignore `Source/FPS_Demo/`** — it is legacy code, not maintained.
- **Class naming**: Use `Base` prefix (e.g., `ABaseCharacter`), never `My` or `MyFps`.
- **All GA classes inherit `UBaseGameplayAbility`**, not `UGameplayAbility` directly. Base class sets `InstancedPerActor`.
- **ABaseWeapon is a data container + visualization** — never add behavior logic to it.
- **Ammo consumption**: decrement `Weapon->CurrentBullets` first, GE and AttributeSet sync as secondary.
- **Blueprint UPROPERTY containers** (e.g., `ActivationBlockedTags`) are overwritten by BP defaults — do not rely on C++ constructor values for these.

## UE 5.7 API Quirks

- `UAbilityTask_WaitDelay` delegate is `OnFinish`, not `OnFinished`.
- `AbilityTags` is deprecated — use `SetAssetTags()` instead.
- `ApplyGameplayEffectSpecToTarget` last parameter is `FGameplayAbilityTargetDataHandle&`, not an ASC pointer. To apply GE to a target ASC, use `TargetASC->ApplyGameplayEffectSpecToSelf()`.
- After renaming a class, add `ActiveClassRedirects` in `Config/DefaultEngine.ini`.

## Code Commenting Standard

当用户要求添加/更新注释时，遵循以下规范：

**三类注释：**
1. **分段标题** `// ====` — 将文件按职责切块（构造、网络复制、GAS、输入、RPC、死亡等），每块 3~5 个函数归为一组
2. **Doxygen 注释** `/** */` — 用于 UCLASS / UFUNCTION / 关键虚函数，解释类的定位、方法的目的、参数含义、使用场景
3. **内联注释** `//` — 用于关键代码行，解释**为什么**这样写（不是翻译代码，是解释意图/原因/陷阱），例如：
   - `// State_Dead 标签守卫（防多帧重复触发）`
   - `// bOwnerNoSee：自己不看到 TP 身体（避免遮挡 FP 视野）`
   - `// 弹药消耗先扣 Weapon，GE 作为附加（兜底）`

**风格要求：**
- 中文注释
- 注释在代码上方（不是行尾）
- 每个 .h 的 UCLASS 必须有 doxygen 注释说明类在架构中的角色
- 每个 .cpp 开头必须有分块标题
- 不要翻译代码 — 解释意图、架构决策、容易出错的地方

## Documentation Convention

When modifying code, update `DESIGN.md` and the corresponding file under `Docs/Design/`. Existing design docs:
- `Docs/Design/Fire/FireDesign.md` — fire ability design & pitfalls
- `Docs/Design/Reload/ReloadDesign.md` — reload ability design & pitfalls
- `Docs/Design/UnLua/UnLuaDesign.md` — UnLua integration & compatibility fixes

## Module Dependencies

```
Public: Core, CoreUObject, Engine, InputCore, EnhancedInput,
        GameplayAbilities, GameplayTasks, GameplayTags, UMG
Private: UnLua, Lua
```

Enabled plugins in .uproject: GameplayAbilities, UnLua, StateTree, GameplayStateTree.
