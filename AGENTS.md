# AGENTS.md

> 更新项目代码时，必须同步更新 `DESIGN.md` 和 `Docs/Design/` 下的对应文档。

## 项目规则

- **不论何时，忽略 `Source/FPS_Demo/` 中的内容**。该目录为遗留代码，不参与当前架构，不进行维护。
- 所有类命名使用 `Base` 前缀，不使用 `My`/`MyFps` 前缀。
- GA 能力继承 `UBaseGameplayAbility`，不直接继承 `UGameplayAbility`。
- `ABaseWeapon` 是数据容器 + 可视化，不包含开火/换弹逻辑。
- 弹药消耗统一为：先 `--Weapon->CurrentBullets`，GE 和 AttributeSet 同步为附加。
- 蓝图子类的 `UPROPERTY` 容器（如 `ActivationBlockedTags`）会被 BP 默认值覆盖，不要依赖 C++ 构造函数设的 UPROPERTY 容器值。
- UE 5.7 中 `UAbilityTask_WaitDelay` 委托为 `OnFinish`，不是 `OnFinished`。
- UE 5.7 中 `AbilityTags` 已废弃，使用 `SetAssetTags()`。
- `ApplyGameplayEffectSpecToTarget` 最后一个参数是 `FGameplayAbilityTargetDataHandle&`，不直接传 ASC。对目标 ASC 的 GE 用 `TargetASC->ApplyGameplayEffectSpecToSelf()`。
- 类重命名后需在 `Config/DefaultEngine.ini` 添加 `ActiveClassRedirects`。

## 文件索引

| 文件 | 内容 |
|------|------|
| `README.md` | 项目介绍 |
| `DESIGN.md` | 架构总览、目录结构、GAS 组件、能力设计、数据流、设计模式、类图、扩展指南、构建说明 |
| `Docs/Design/Fire/FireDesign.md` | 开火能力的设计过程与踩坑记录 |
| `Docs/Design/Reload/ReloadDesign.md` | 换弹能力的设计过程与踩坑记录 |
| `Docs/Design/UnLua/UnLuaDesign.md` | UnLua 集成设计、兼容性修复与绑定方式 |
| `AGENTS.md` | 本文件 — 项目规则与规范 |
