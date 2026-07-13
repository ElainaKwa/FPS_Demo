# FPS_Demo

基于 Unreal Engine 5.7 开发

> **`Source/FPS_Demo/` 中的内容是 UE 第一人称模版自带的代码，请忽略，不参与当前架构。**

FPS_Demo 是一个采用 Gameplay Ability System (GAS) 实现武器机制的第一人称射击项目，核心模块为 **`MyFps_Demo`**。

## 技术栈

| 技术 | 用途 |
|------|------|
| Gameplay Ability System (GAS) | 开火/换弹能力的激活、执行、取消、冷却管理 |
| Enhanced Input | 统一的输入映射与触发式输入处理 |
| UMG / Slate | HUD 弹药计数器、生命值等 UI 控件 |
| Native GameplayTags | 状态标记与事件广播 |
| GameplayEffect | 弹药消耗、弹药补充、伤害数值的运行时修改 |
| AttributeSet | 可网络复制的弹药属性 |
| AbilityTask | 异步等待输入释放、计时延迟等能力逻辑编排 |
| UnLua | Lua 脚本绑定、热重载、C++/Lua 混合开发 |

## 文档

- [设计文档](DESIGN.md) — 架构总览、设计模式、数据流、类图、扩展指南
- [角色设计](Docs/Design/Character/CharacterDesign.md) — 角色类层次结构重构
- [开火能力设计](Docs/Design/Fire/FireDesign.md) — 开火能力的设计过程与踩坑记录
- [换弹能力设计](Docs/Design/Reload/ReloadDesign.md) — 换弹能力的设计过程与踩坑记录
- [弹药 HUD 设计](Docs/Design/UI/UIDesign.md) — 弹药 HUD 设计与 UnLua + UMG 踩坑记录
- [动态准星设计](Docs/Design/Crosshair/CrosshairDesign.md) — 十字+点、屏幕中心、移动/开火散度
- [UnLua 集成设计](Docs/Design/UnLua/UnLuaDesign.md) — UnLua 集成、兼容性修复与绑定方式
- [游戏设置设计](Docs/Design/Settings/SettingsDesign.md) — 中央单例管理、Apply/Revert、原子化 Save/Load
- [项目规范](AGENTS.md) — 代码规范、命名约定、框架规则
