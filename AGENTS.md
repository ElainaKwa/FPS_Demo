# AGENTS.md

> 更新项目代码时，必须同步更新 `DESIGN.md` 和 `Docs/Design/` 下的对应文档。

## 项目规则

- 用中文回复。
- **不论何时，忽略 `Source/FPS_Demo/` 中的内容**。该目录为遗留代码，不参与当前架构，不进行维护。
- 所有基础类命名使用 `Base` 前缀，不使用 `My`/`MyFps` 前缀。
- GA 能力继承 `UBaseGameplayAbility`，不直接继承 `UGameplayAbility`。
- `ABaseWeapon` 是数据容器 + 可视化，不包含开火/换弹逻辑。
- 弹药消耗统一为：先 `--Weapon->CurrentBullets`，GE 和 AttributeSet 同步为附加。
- 蓝图子类的 `UPROPERTY` 容器（如 `ActivationBlockedTags`）会被 BP 默认值覆盖，不要依赖 C++ 构造函数设的 UPROPERTY 容器值。
- UE 5.7 中 `UAbilityTask_WaitDelay` 委托为 `OnFinish`，不是 `OnFinished`。
- UE 5.7 中 `AbilityTags` 已废弃，使用 `SetAssetTags()`。
- `ApplyGameplayEffectSpecToTarget` 最后一个参数是 `FGameplayAbilityTargetDataHandle&`，不直接传 ASC。对目标 ASC 的 GE 用 `TargetASC->ApplyGameplayEffectSpecToSelf()`。
- 类重命名后需在 `Config/DefaultEngine.ini` 添加 `ActiveClassRedirects`。
- 所有 Widget 子类中，局部变量名 `Slot` 会隐藏 `UWidget::Slot` 成员，触发 C4458。统一用 `CanvasSlot`。
- **修改完 C++ 代码后，必须执行构建验证有无编译错误，有错误则直接修复。**
- **编写网络复制相关代码时，必须先阅读 `DESIGN.md` → `## 8. 网络架构` 和 `## 9. 踩坑记录：网络复制`，避免重复踩坑。**

## 文件索引

| 文件 | 内容 |
|------|------|
| `README.md` | 项目介绍 |
| `DESIGN.md` | 架构总览、目录结构、GAS 组件、能力设计、数据流、设计模式、类图、**网络架构**、**踩坑记录**、扩展指南、构建说明 |
| `Docs/Design/Fire/FireDesign.md` | 开火能力的设计过程与踩坑记录 |
| `Docs/Design/Reload/ReloadDesign.md` | 换弹能力的设计过程与踩坑记录 |
| `Docs/Design/UnLua/UnLuaDesign.md` | UnLua 集成设计、兼容性修复与绑定方式 |
| `Docs/Design/UI/UIDesign.md` | 弹药 HUD 设计与 UnLua + UMG 踩坑记录 |
| `Docs/Design/Crosshair/CrosshairDesign.md` | 动态准星设计：十字+点、屏幕中心、移动/开火散度 |
| `Docs/Design/Character/CharacterDesign.md` | 角色类层次结构重构：基类 + 派生类设计 |
| `Docs/Design/Settings/SettingsDesign.md` | 游戏设置系统设计：中央单例管理、Apply/Revert、原子化 Save/Load |
| `Docs/Design/Movement/MovementDesign.md` | 角色移动 GAS 化：下蹲、疾跑、跳跃的能力设计与网络同步 |
| `AGENTS.md` | 本文件 — 项目规则与规范 |
