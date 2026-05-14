# 角色类重构设计文档

## 1. 设计目标

将当前单一的 `ABaseCharacter` 重构为 **基类 + 派生类** 层次结构：

- **职责分离**：玩家与 AI 敌人共享核心 GAS/武器逻辑，各自拥有特化功能
- **精简性**：Enemy 不携带不需要的体力属性和第一人称组件
- **可扩展性**：新增角色类型（NPC 队友、Boss）只需继承 `ABaseCharacter`
- **一致性**：保持现有 GAS 流程和 `IBaseWeaponHolder` 接口不变

---

## 2. 当前架构问题

```
ABaseCharacter（单体类，承担所有职责）
  ├── 第一人称渲染（FirstPersonMesh + Camera）   ← Enemy 不需要
  ├── Enhanced Input（MoveAction / FireAction…）  ← Enemy 不需要
  ├── 体力系统（Stamina / MaxStamina）            ← Enemy 不需要
  ├── 血量系统（Health / MaxHealth）              ← 两者都需要
  ├── 武器系统（IBaseWeaponHolder + 库存）        ← 两者都需要
  ├── GAS（ASC + AttributeSet）                   ← 两者都需要
  └── HUD 更新（屏幕空间 UI）                     ← Enemy 用世界空间 UI

ABaseEnemy（空壳）
  └── 仅有 ASC + CharacterAttributeSet，无武器无 AI
```

核心矛盾：`ABaseCharacter` 把玩家专属逻辑（输入、第一人称、体力）和通用逻辑（GAS、武器、血量）混在一起，Enemy 要复用通用部分就必须携带一堆空壳成员。

---

## 3. 重构后架构

### 3.1 类层次

```
ACharacter
  └── ABaseCharacter (Abstract)          ← 通用：GAS + 血量 + 武器 + 死亡
        ├── APlayerCharacter             ← 玩家：体力 + 第一人称 + 输入 + 屏幕 HUD
        └── ABaseEnemy                   ← 敌人：AI + 简单开火 + 简单移动 + 世界空间 HUD
```

### 3.2 组件分配

| 组件 | ABaseCharacter | APlayerCharacter | ABaseEnemy |
|------|:-:|:-:|:-:|
| `UBaseAbilitySystemComponent` | **创建** | 继承 | 继承 |
| `UBaseHealthAttributeSet` | **创建** | 继承 | 继承 |
| `UBaseWeaponAttributeSet` | **创建** | 继承 | 继承 |
| `UBaseStaminaAttributeSet` | — | **创建** | — |
| `USkeletalMeshComponent`（ThirdPerson / GetMesh） | **创建** | 继承 | 继承 |
| `USkeletalMeshComponent`（FirstPersonMesh） | — | **创建** | — |
| `UCameraComponent`（FirstPersonCamera） | — | **创建** | — |
| 武器库存 `OwnedWeapons` | **持有** | 继承 | 继承 |
| `UWidgetComponent`（头顶血条） | — | — | **创建** |

### 3.3 功能分配

| 功能 | ABaseCharacter | APlayerCharacter | ABaseEnemy |
|------|:-:|:-:|:-:|
| GAS 初始化 & 能力授予 | **虚函数** `InitAbilitySystem()` | 继承 | 继承（简化授予） |
| `IBaseWeaponHolder` 接口 | **实现** | 覆写 2 个方法 | 覆写 3 个方法 |
| 武器生成/切换/弹仓同步 | **实现** | 继承 | 继承 |
| 血量 & 伤害处理 | **实现** | 继承 | 继承 |
| 死亡处理 | **虚函数** `OnDeath()` | 覆写：禁输入→通知 GameMode | 覆写：延迟销毁→通知 GameMode |
| 体力系统 | — | **实现** | — |
| Enhanced Input | — | **实现** | — |
| 第一人称武器挂载 | — | **覆写** `AttachWeaponMeshes` | — |
| 后坐力（Controller Pitch） | — | **覆写** `AddWeaponRecoil` | 空实现 |
| 瞄准目标（Camera vs AI） | **虚函数** `GetWeaponTargetLocation` | Camera LineTrace | Actor 前方 + 目标方向 |
| HUD 更新 | **虚函数** `UpdateHealthHUD` | 屏幕 UI 委托 | 世界空间血条 |
| AI 行为 | — | — | **实现** |

---

## 4. AttributeSet 拆分

当前 `UBaseCharacterAttributeSet` 包含 Health + Stamina。Enemy 只需要 Health，拆分为两个 AttributeSet：

### 4.1 UBaseHealthAttributeSet（BaseCharacter 持有）

```cpp
UCLASS()
class UBaseHealthAttributeSet : public UAttributeSet
{
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health")
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS_BASIC(UBaseHealthAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health")
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS_BASIC(UBaseHealthAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Health")
    FGameplayAttributeData IncomingDamage;
    ATTRIBUTE_ACCESSORS_BASIC(UBaseHealthAttributeSet, IncomingDamage)

    UPROPERTY(BlueprintReadOnly, Category = "Health")
    FGameplayAttributeData IncomingHeal;
    ATTRIBUTE_ACCESSORS_BASIC(UBaseHealthAttributeSet, IncomingHeal)

    // OnRep_*, PostGameplayEffectExecute, GetLifetimeReplicatedProps
};
```

**`PostGameplayEffectExecute` 逻辑**：

```
IncomingDamage  → Damage = GetIncomingDamage(); SetIncomingDamage(0);
                  NewHealth = Health - Damage; Clamp [0, MaxHealth]
                  Health == 0 → HandleDeath()
IncomingHeal    → Heal = GetIncomingHeal(); SetIncomingHeal(0);
                  NewHealth = Health + Heal; Clamp [0, MaxHealth]
MaxHealth       → Clamp ≥ 1
所有健康变更后  → Cast<ABaseCharacter>(Owner)->UpdateHealthHUD()
```

### 4.2 UBaseStaminaAttributeSet（PlayerCharacter 持有）

```cpp
UCLASS()
class UBaseStaminaAttributeSet : public UAttributeSet
{
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Stamina")
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS_BASIC(UBaseStaminaAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Stamina")
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS_BASIC(UBaseStaminaAttributeSet, MaxStamina)

    UPROPERTY(BlueprintReadOnly, Category = "Stamina")
    FGameplayAttributeData IncomingStaminaCost;
    ATTRIBUTE_ACCESSORS_BASIC(UBaseStaminaAttributeSet, IncomingStaminaCost)

    // OnRep_*, PostGameplayEffectExecute, GetLifetimeReplicatedProps
};
```

**`PostGameplayEffectExecute` 逻辑**：

```
IncomingStaminaCost → Cost = GetIncomingStaminaCost(); SetIncomingStaminaCost(0);
                      NewStamina = Stamina - Cost; Clamp [0, MaxStamina]
Stamina             → Clamp [0, MaxStamina]
MaxStamina          → Clamp ≥ 1
所有体力变更后      → Cast<APlayerCharacter>(Owner)->UpdateStaminaHUD()
```

---

## 5. 详细类设计

### 5.1 ABaseCharacter（重构后）

```cpp
UCLASS(Abstract, Blueprintable)
class ABaseCharacter : public ACharacter, public IBaseWeaponHolder, public IAbilitySystemInterface
{
    GENERATED_BODY()

    // ---- Components ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess))
    TObjectPtr<UBaseAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess))
    TObjectPtr<UBaseHealthAttributeSet> HealthAttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess))
    TObjectPtr<UBaseWeaponAttributeSet> WeaponAttributeSet;

    // ---- Weapon ----
    UPROPERTY(EditAnywhere, Category = "Weapon")
    TSubclassOf<ABaseWeapon> DefaultWeaponClass;

    UPROPERTY(EditAnywhere, Category = "Weapon")
    FName ThirdPersonWeaponSocket = FName("HandGrip_R");

    UPROPERTY()
    TArray<TObjectPtr<ABaseWeapon>> OwnedWeapons;

    UPROPERTY()
    TObjectPtr<ABaseWeapon> CurrentWeapon;

    // ---- Delegates ----
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnHealthUpdated OnHealthUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBulletCountUpdated OnBulletCountUpdated;

public:
    ABaseCharacter();

    // ---- IAbilitySystemInterface ----
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // ---- IBaseWeaponHolder (部分虚函数供子类覆写) ----
    virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) override;
    virtual FVector GetWeaponTargetLocation() const override;       // ← virtual, 子类覆写
    virtual void PlayFiringMontage(UAnimMontage* Montage) override;
    virtual void PlayReloadMontage(UAnimMontage* Montage) override;
    virtual void AddWeaponRecoil(float RecoilAmount) override;     // ← 默认空实现
    virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MaxAmmo) override;
    virtual ABaseWeapon* GetCurrentWeapon() const override;

    // ---- HUD (虚函数) ----
    virtual void UpdateHealthHUD();    // Player → 委托广播, Enemy → WidgetComponent
    virtual void UpdateWeaponAmmoHUD();

    // ---- Death ----
    virtual void OnDeath();            // ← 子类覆写：Player 禁输入/通知重生, Enemy 延迟销毁

    // ---- Weapon Management ----
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SwitchToWeapon(ABaseWeapon* NewWeapon);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type) override;

    // GAS 初始化钩子
    virtual void InitAbilitySystem();

    void SpawnDefaultWeapon();
    ABaseWeapon* FindWeaponOfClass(TSubclassOf<ABaseWeapon> WeaponClass) const;
    void SyncAttributeSetFromWeapon() const;
};
```

**构造函数**（仅创建三方面具 + 通用组件）：

```cpp
ABaseCharacter::ABaseCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

    // 第三人称网格（所有角色共享）
    GetMesh()->SetRelativeLocation(FVector(0, 0, -96));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));

    // GAS
    AbilitySystemComponent = CreateDefaultSubobject<UBaseAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    HealthAttributeSet = CreateDefaultSubobject<UBaseHealthAttributeSet>(TEXT("HealthAttributeSet"));
    WeaponAttributeSet  = CreateDefaultSubobject<UBaseWeaponAttributeSet>(TEXT("WeaponAttributeSet"));
}
```

**`InitAbilitySystem()` 虚函数**：

```cpp
void ABaseCharacter::InitAbilitySystem()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        AbilitySystemComponent->GrantDefaultAbilities();
    }
}
```

子类可覆写此函数来控制授予哪些能力（Player 授予全部，Enemy 只授予 Fire）。

**`BeginPlay()`**：

```cpp
void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();
    InitAbilitySystem();
    SpawnDefaultWeapon();

    if (!GetMesh()->GetSkeletalMeshAsset())
    {
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationCustomMode);
    }
}
```

**`GetWeaponTargetLocation()` 默认实现**（Actor 前方，供 Enemy 使用）：

```cpp
FVector ABaseCharacter::GetWeaponTargetLocation() const
{
    const FVector Start = GetActorLocation() + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const FVector End = Start + (GetActorForwardVector() * 10000.0f);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    return Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
}
```

**`OnDeath()` 默认实现**：

```cpp
void ABaseCharacter::OnDeath()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->AddLooseGameplayTag(BaseGameplayTags::State_Dead);
        AbilitySystemComponent->CancelAllAbilities();
    }

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (GetMesh()->GetSkeletalMeshAsset())
    {
        GetMesh()->SetSimulatePhysics(true);
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsBody);
    }

    if (CurrentWeapon)
    {
        CurrentWeapon->SetActorHiddenInGame(true);
    }
}
```

---

### 5.2 APlayerCharacter（新增）

```cpp
UCLASS(Blueprintable)
class APlayerCharacter : public ABaseCharacter
{
    GENERATED_BODY()

    // ---- First Person Components ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess))
    TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess))
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    // ---- Stamina ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess))
    TObjectPtr<UBaseStaminaAttributeSet> StaminaAttributeSet;

    // ---- Input ----
    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> FireAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> ReloadAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> SwitchWeaponAction;

    // ---- Aim ----
    UPROPERTY(EditAnywhere, Category = "Aim")
    float MaxAimDistance = 10000.0f;

    // ---- Weapon Socket ----
    UPROPERTY(EditAnywhere, Category = "Weapon")
    FName FirstPersonWeaponSocket = FName("HandGrip_R");

    // ---- Delegates ----
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStaminaUpdated OnStaminaUpdated;

public:
    APlayerCharacter();

    // ---- Getters ----
    USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
    UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

    // ---- Overrides ----
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    virtual void AttachWeaponMeshes(ABaseWeapon* Weapon) override;
    virtual FVector GetWeaponTargetLocation() const override;
    virtual void PlayFiringMontage(UAnimMontage* Montage) override;
    virtual void PlayReloadMontage(UAnimMontage* Montage) override;
    virtual void AddWeaponRecoil(float RecoilAmount) override;
    virtual void UpdateHealthHUD() override;
    virtual void OnDeath() override;

    void UpdateStaminaHUD();

protected:
    // ---- Input Handlers ----
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnJumpStarted();
    void OnJumpEnded();
    void OnStartFiring();
    void OnStopFiring();
    void OnReload();
    void OnSwitchWeapon();
};
```

**构造函数**：

```cpp
APlayerCharacter::APlayerCharacter()
{
    bUseControllerRotationPitch = true;
    bUseControllerRotationYaw = true;

    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->RotationRate = FRotator(0, 540, 0);
    GetCharacterMovement()->JumpZVelocity = 420.0f;
    GetCharacterMovement()->AirControl = 0.2f;

    // 第一人称网格：仅拥有者可见
    GetMesh()->bOwnerNoSee = true;
    GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

    FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
    FirstPersonMesh->SetupAttachment(GetCapsuleComponent());
    FirstPersonMesh->SetRelativeLocation(FVector(0, 0, -90));
    FirstPersonMesh->SetRelativeRotation(FRotator(0, -90, 0));
    FirstPersonMesh->bOnlyOwnerSee = true;
    FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
    FirstPersonMesh->bCastDynamicShadow = false;
    FirstPersonMesh->bCastHiddenShadow = false;

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0, 10, 64));
    FirstPersonCamera->bUsePawnControlRotation = true;

    // 体力 AttributeSet
    StaminaAttributeSet = CreateDefaultSubobject<UBaseStaminaAttributeSet>(TEXT("StaminaAttributeSet"));
}
```

**`BeginPlay()`**：

```cpp
void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();  // ABaseCharacter::BeginPlay() 会调用 InitAbilitySystem + SpawnDefaultWeapon

    // 注册输入映射上下文
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (DefaultMappingContext)
            {
                Subsystem->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }
}
```

**`AttachWeaponMeshes()`**（Player 覆写 — 挂载 FP + TP）：

```cpp
void APlayerCharacter::AttachWeaponMeshes(ABaseWeapon* Weapon)
{
    if (!Weapon) return;

    const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);

    // 第一人称武器网格
    Weapon->GetFirstPersonMesh()->AttachToComponent(
        FirstPersonMesh, AttachRules, FirstPersonWeaponSocket);
    Weapon->GetFirstPersonMesh()->AddLocalRotation(Weapon->FirstPersonMeshRotationOffset);

    // 第三人称武器网格
    if (GetMesh()->GetSkeletalMeshAsset())
    {
        Weapon->GetThirdPersonMesh()->AttachToComponent(
            GetMesh(), AttachRules, ThirdPersonWeaponSocket);
    }
    else
    {
        Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachRules);
    }
}
```

**`GetWeaponTargetLocation()`**（Player — Camera LineTrace）：

```cpp
FVector APlayerCharacter::GetWeaponTargetLocation() const
{
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + (FirstPersonCamera->GetForwardVector() * MaxAimDistance);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    return Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
}
```

**`AddWeaponRecoil()`**（Player — 控制器俯仰输入）：

```cpp
void APlayerCharacter::AddWeaponRecoil(float RecoilAmount)
{
    AddControllerPitchInput(RecoilAmount);
}
```

**`OnDeath()`**（Player — 禁输入 + 通知 GameMode）：

```cpp
void APlayerCharacter::OnDeath()
{
    Super::OnDeath();  // State.Dead, 取消能力, Ragdoll, 隐藏武器

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->DisableInput(PC);
    }
    // TODO: 通知 GameMode 触发重生逻辑
}
```

**`UpdateHealthHUD()` / `UpdateStaminaHUD()`**：

```cpp
void APlayerCharacter::UpdateHealthHUD()
{
    if (HealthAttributeSet)
    {
        OnHealthUpdated.Broadcast(HealthAttributeSet->GetHealth(), HealthAttributeSet->GetMaxHealth());
    }
}

void APlayerCharacter::UpdateStaminaHUD()
{
    if (StaminaAttributeSet)
    {
        OnStaminaUpdated.Broadcast(StaminaAttributeSet->GetStamina(), StaminaAttributeSet->GetMaxStamina());
    }
}
```

---

### 5.3 ABaseEnemy（重构后）

```cpp
UCLASS(Blueprintable)
class ABaseEnemy : public ABaseCharacter
{
    GENERATED_BODY()

    // ---- AI Config ----
    UPROPERTY(EditAnywhere, Category = "AI")
    float SightRange = 3000.0f;

    UPROPERTY(EditAnywhere, Category = "AI")
    float SightHalfAngle = 45.0f;

    UPROPERTY(EditAnywhere, Category = "AI")
    float FireRange = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "AI")
    float LoseSightTime = 5.0f;     // 目标脱离视线后保留记忆的秒数

    // ---- Movement ----
    UPROPERTY(EditAnywhere, Category = "Movement")
    float PatrolSpeed = 200.0f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float ChaseSpeed = 600.0f;

    // ---- World-space Health Bar ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess))
    TObjectPtr<UWidgetComponent> HealthBarWidget;

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> HealthBarWidgetClass;

    // ---- AI State ----
    UPROPERTY()
    TObjectPtr<AActor> TargetActor;

    UPROPERTY()
    float LastSeenTime = 0.0f;

    UPROPERTY()
    bool bIsFiring = false;

public:
    ABaseEnemy();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // ---- Overrides ----
    virtual FVector GetWeaponTargetLocation() const override;
    virtual void PlayFiringMontage(UAnimMontage* Montage) override;
    virtual void AddWeaponRecoil(float RecoilAmount) override;  // 空实现
    virtual void UpdateHealthHUD() override;
    virtual void OnDeath() override;

protected:
    virtual void InitAbilitySystem() override;

    // ---- AI Logic ----
    bool CanSeeTarget() const;
    void UpdateTarget();
    void MoveTowardTarget(float DeltaSeconds);
    void FaceTarget(float DeltaSeconds);

public:
    // ---- AI Interface (也可供 BehaviorTree Task 调用) ----
    UFUNCTION(BlueprintCallable, Category = "AI")
    void StartFiring();

    UFUNCTION(BlueprintCallable, Category = "AI")
    void StopFiring();

    UFUNCTION(BlueprintCallable, Category = "AI")
    void Reload();
};
```

**构造函数**：

```cpp
ABaseEnemy::ABaseEnemy()
{
    // Enemy 使用 AIController 而非 PlayerController
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 540, 0);

    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // 头顶血条 WidgetComponent
    HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
    HealthBarWidget->SetupAttachment(GetCapsuleComponent());
    HealthBarWidget->SetRelativeLocation(FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20));
    HealthBarWidget->SetWidgetSpace(EWidgetSpace::World);
    HealthBarWidget->SetDrawSize(FVector2D(100, 10));
    HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
```

**`InitAbilitySystem()` 覆写**（Enemy 只授予开火能力，不授予换弹能力——Enemy 换弹用定时器简化处理）：

```cpp
void ABaseEnemy::InitAbilitySystem()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        // 只授予开火能力，换弹由 AI 简化触发
        AbilitySystemComponent->GrantDefaultAbilities();
    }
}
```

> 如果后续想让 Enemy 走标准换弹能力流程，`GrantDefaultAbilities()` 也会授予换弹能力，只需在 `StartFiring()` 中处理 `Event.OutOfAmmo` 事件即可自动换弹。

**`GetWeaponTargetLocation()`**（Enemy — 朝向目标 Actor 的简易瞄准）：

```cpp
FVector ABaseEnemy::GetWeaponTargetLocation() const
{
    if (TargetActor)
    {
        return TargetActor->GetActorLocation() + FVector(0, 0, 90.0f);  // 大致胸口高度
    }
    // 无目标时朝 Actor 前方
    return GetActorLocation() + GetActorForwardVector() * 5000.0f;
}
```

**AI 逻辑（Tick 驱动，简单版）**：

```cpp
void ABaseEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BaseGameplayTags::State_Dead))
    {
        return;
    }

    UpdateTarget();

    if (TargetActor)
    {
        FaceTarget(DeltaSeconds);

        const float Distance = FVector::Dist(GetActorLocation(), TargetActor->GetActorLocation());

        if (Distance <= FireRange && CanSeeTarget())
        {
            if (!bIsFiring)
            {
                StartFiring();
            }
        }
        else
        {
            if (bIsFiring)
            {
                StopFiring();
            }
            MoveTowardTarget(DeltaSeconds);
        }
    }
    else
    {
        if (bIsFiring)
        {
            StopFiring();
        }
        // 简单巡逻：原地待命或按巡逻点移动（后续扩展）
    }
}
```

**`StartFiring()` / `StopFiring()`**：

```cpp
void ABaseEnemy::StartFiring()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->TryActivateAbility(AbilitySystemComponent->GetFireAbilityHandle());
        bIsFiring = true;
    }
}

void ABaseEnemy::StopFiring()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Fire));
        bIsFiring = false;
    }
}

void ABaseEnemy::Reload()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->CancelFireAbility();
        if (FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GetReloadAbilityHandle(); Handle.IsValid())
        {
            AbilitySystemComponent->TryActivateAbility(Handle);
        }
    }
}
```

**`UpdateHealthHUD()`**（Enemy — 直接调用血条 Widget）：

```cpp
void ABaseEnemy::UpdateHealthHUD()
{
    if (HealthBarWidget && HealthAttributeSet)
    {
        if (UBaseHealthBarWidget* Widget = Cast<UBaseHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
        {
            Widget->UpdateHealth(HealthAttributeSet->GetHealth(), HealthAttributeSet->GetMaxHealth());
        }
    }
}
```

**`UpdateTarget()`** — 寻找最近玩家：

```cpp
void ABaseEnemy::UpdateTarget()
{
    if (TargetActor)
    {
        if (UAbilitySystemComponent* TargetASC =
                UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
        {
            if (TargetASC->HasMatchingGameplayTag(BaseGameplayTags::State_Dead))
                TargetActor = nullptr;
        }
    }
    if (!TargetActor)
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC && PC->GetPawn())
        {
            if (FVector::Dist(GetActorLocation(), PC->GetPawn()->GetActorLocation()) <= SightRange)
                TargetActor = PC->GetPawn();
        }
    }
}
```

**`OnDeath()`**（Enemy — 延迟销毁）：

```cpp
void ABaseEnemy::OnDeath()
{
    Super::OnDeath();

    // 隐藏头顶血条
    if (HealthBarWidget)
    {
        HealthBarWidget->SetVisibility(false);
    }

    // 延迟销毁（留出死亡动画/ Ragdoll 时间）
    SetLifeSpan(5.0f);

    // TODO: 通知 GameMode 更新击杀分数
}
```

**`CanSeeTarget()` 视线检测**：

```cpp
bool ABaseEnemy::CanSeeTarget() const
{
    if (!TargetActor) return false;

    const FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    const float Dot = FVector::DotProduct(GetActorForwardVector(), Direction);
    const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));

    if (Dot < CosHalfAngle) return false;

    // 射线检测遮挡
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(TargetActor);

    const FVector Start = GetActorLocation() + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const FVector End = TargetActor->GetActorLocation() + FVector(0, 0, 90.0f);

    GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    return !Hit.bBlockingHit;  // 无遮挡 = 可见
}
```

---

### 5.4 死亡处理流

```
GE_Damage → HealthAttributeSet::PostGameplayEffectExecute()
  │
  ├─ IncomingDamage → NewHealth = Health - Damage
  │   └─ Health == 0?
  │       └─ Yes → Cast<ABaseCharacter>(Owner)->OnDeath()
  │
  ▼
 ABaseCharacter::OnDeath()                     ← 通用
  ├─ ASC->AddLooseGameplayTag(State_Dead)
  ├─ ASC->CancelAllAbilities()
  ├─ Capsule → NoCollision
  ├─ Mesh → SimulatePhysics (Ragdoll)
  └─ CurrentWeapon → DropToGround() (分离 + 物理掉落, 解除 Owner 绑定)

APlayerCharacter::OnDeath()                   ← 玩家特化
  ├─ Super::OnDeath()
  └─ PlayerController->DisableInput()

ABaseEnemy::OnDeath()                         ← 敌人特化
  ├─ Super::OnDeath()
  ├─ HealthBarWidget → Hidden
  └─ SetLifeSpan(5.0f)  → 5秒后自动销毁
```

---

## 6. 数据流

### 6.1 伤害流（Player → Enemy / Enemy → Player）

```
GA_WeaponFire::PerformFire()
  │
  ├─ LineTrace → 命中 Actor
  │
  ├─ 命中目标有 ASC?
  │   ├─ Yes → 创建 GE_Damage Spec
  │   │        SetByCaller(Data.Damage, Damage)
  │   │        TargetASC->ApplyGameplayEffectSpecToSelf(Spec)
  │   │        │
  │   │        ▼
  │   │   Target UBaseHealthAttributeSet::PostGameplayEffectExecute()
  │   │        ├─ IncomingDamage → Health -= Damage
  │   │        ├─ Health == 0 → OnDeath()
  │   │        └─ UpdateHealthHUD()
  │   │
  │   └─ No  → UGameplayStatics::ApplyDamage() (回退兼容)
  │
  └─ (Player/Enemy 伤害流完全一致，因为都经过 HealthAttributeSet)
```

### 6.2 Enemy 开火流

```
ABaseEnemy::Tick()
  │
  ├─ UpdateTarget()  → 更新 TargetActor
  │
  ├─ Target 在射程内 && CanSeeTarget()?
  │   ├─ Yes → StartFiring()
  │   │        │
  │   │        ▼
  │   │   ASC->TryActivateAbility(FireHandle)
  │   │        │
  │   │        ▼
  │   │   GA_WeaponFire::ActivateAbility()
  │   │     ├─ 读取 IBaseWeaponHolder::GetCurrentWeapon() → 武器数据
  │   │     ├─ 读取 IBaseWeaponHolder::GetWeaponTargetLocation() → Enemy 覆写版本
  │   │     ├─ LineTrace → 命中检测 → GE_Damage
  │   │     ├─ PlayFiringMontage() → 第三人称蒙太奇
  │   │     └─ 弹药消耗 → AttributeSet 同步
  │   │
  │   └─ No  → StopFiring() + MoveTowardTarget()
```

### 6.3 Enemy AI 简易状态机

```
                  ┌──────────┐
                  │  Idle    │ ← 初始/无目标
                  └────┬─────┘
                       │ CanSeeTarget()
                       ▼
                  ┌──────────┐
            ┌────►│  Chase   │ ← 向目标移动
            │     └────┬─────┘
            │          │ Distance ≤ FireRange && CanSeeTarget()
            │          ▼
            │     ┌──────────┐
            │     │  Combat  │ ← 停止移动 + 开火
            │     └────┬─────┘
            │          │ !CanSeeTarget() && 记忆超时
            │          ▼
            │     ┌──────────┐
            └─────│  Lose   │ ← 失去目标，回到 Idle
                  └──────────┘
```

---

## 7. 接口变更

### 7.1 IBaseWeaponHolder（无变更）

现有接口方法已足够，Player 和 Enemy 通过覆写各自的实现来差异化行为。**不需要新增或删除接口方法。**

### 7.2 UBaseAbilitySystemComponent

保持不变。Enemy 通过同一个 `GrantDefaultAbilities()` 获得开火和换弹能力。如果后续需要 Enemy 只授予部分能力，可添加一个 `GrantCombatAbilities()` 方法并让 `InitAbilitySystem()` 覆写调用。

### 7.3 UBaseCharacterAttributeSet → 拆分

| 原属性 | 归属新 AttributeSet | 所在类 |
|--------|---------------------|--------|
| Health, MaxHealth | `UBaseHealthAttributeSet` | ABaseCharacter |
| IncomingDamage, IncomingHeal | `UBaseHealthAttributeSet` | ABaseCharacter |
| Stamina, MaxStamina | `UBaseStaminaAttributeSet` | APlayerCharacter |

> 原 `UBaseCharacterAttributeSet` 文件保留但标记为废弃，添加 `ActiveClassRedirects` 确保旧 BP 自动重定向。

---

## 8. 编辑器迁移指南

### 8.1 第一步：C++ 编译

1. 重构 `ABaseCharacter` — 将第一人称组件、输入、体力相关代码移除
2. 新建 `APlayerCharacter` — 从 `ABaseCharacter` 移入上述代码
3. 重构 `ABaseEnemy` — 改为继承 `ABaseCharacter`
4. 拆分 `UBaseCharacterAttributeSet` → `UBaseHealthAttributeSet` + `UBaseStaminaAttributeSet`
5. 更新 `BaseCharacterAttributeSet.cpp` 中 `Cast<ABaseCharacter>` → `Cast<ABaseCharacter>`（HealthSet）/ `Cast<APlayerCharacter>`（StaminaSet）
6. 更新 UI Widget 的 `TryBindDelegate()` 中 `Cast<ABaseCharacter>` → `Cast<APlayerCharacter>`（体力条）/ 保持 `Cast<ABaseCharacter>`（血量条）
7. 编译成功

### 8.2 第二步：Blueprint 迁移

**玩家 BP 迁移（关键步骤）**：

1. 打开编辑器，找到现有玩家 Blueprint（如 `BP_BaseCharacter`）
2. 右键 → **Asset Actions → Reparent Blueprint** → 选择 `APlayerCharacter`
3. 重新配置以下属性（从 ABaseCharacter 移到 APlayerCharacter 的属性值会丢失）：
   - `Default Mapping Context` → 重新指定输入映射
   - `MoveAction / LookAction / JumpAction / FireAction / ReloadAction / SwitchWeaponAction` → 重新指定 InputAction 资产
   - `MaxAimDistance` → 重新设为 10000
   - `FirstPersonWeaponSocket` → 设为 `HandGrip_R`
4. FirstPersonMesh 和 FirstPersonCamera 组件已在 C++ 构造函数中创建，BP 中会自动出现
5. 重新指定 FirstPersonMesh 的骨骼网格资产
6. 编译 BP，检查无报错

**Enemy BP 创建**：

1. 右键 Content Browser → **Blueprint Class** → 选择 `ABaseEnemy`
2. 命名为 `BP_BaseEnemy`
3. 配置：
   - `Default Weapon Class` → 指定武器 BP
   - `Sight Range / Fire Range / Sight Half Angle` → 按需调整
   - `Patrol Speed / Chase Speed` → 按需调整
   - `Health Bar Widget Class` → 指定血条 Widget BP
   - 第三人称网格 → 指定骨骼网格
4. 设置 `Auto Possess AI` = `PlacedInWorldOrSpawned`
5. 设置 `AI Controller Class` = `AAIController`（默认已在 C++ 构造函数中设置）

### 8.3 第三步：GameMode 配置

1. 打开 GameMode Blueprint（如 `BP_BaseGameMode`）
2. **Default Pawn Class** → 从 `ABaseCharacter` 改为 `APlayerCharacter`
3. 确保玩家生成时使用的是新 BP

### 8.4 第四步：ActiveClassRedirects（处理旧 BP 引用）

在 `Config/DefaultEngine.ini` 的 `[/Script/Engine.Engine]` 段添加：

```ini
+ActiveClassRedirects=(OldName="/Script/MyFps_Demo.BaseCharacterAttributeSet",NewName="/Script/MyFps_Demo.BaseHealthAttributeSet")
```

如果旧 BP 中有引用 `UBaseCharacterAttributeSet` 类型的节点，此重定向确保它们自动指向新的 `UBaseHealthAttributeSet`。

### 8.5 第五步：验证清单

- [ ] 玩家：WASD 移动 / 鼠标视角 / 跳跃 / 开火 / 换弹 / 切枪 / 血条 / 体力条 / 弹药计数器
- [ ] Enemy：AI 自动检测玩家 / 追踪移动 / 射程内开火 / 头顶血条 / 死亡后销毁
- [ ] 伤害流：玩家射击 Enemy → Enemy 血量下降 → 血条更新 → 死亡
- [ ] 伤害流：Enemy 射击玩家 → 玩家血量下降 → HUD 更新 → 死亡

---

## 9. 踩坑预警

| 问题 | 根因 | 预防 |
|------|------|------|
| BP Reparent 后属性值丢失 | 从 ABaseCharacter 移到 APlayerCharacter 的 UPROPERTY 不再属于原父类 | 迁移前截图记录所有属性值；Reparent 后逐一恢复 |
| AttributeSet Cast 失败 | `UBaseStaminaAttributeSet::PostGameplayEffectExecute` 中 `Cast<APlayerCharacter>` 对 Enemy 返回 nullptr | 只在 Cast 成功时调用 `UpdateStaminaHUD()`，Enemy 没有 StaminaSet 所以不会触发 |
| Enemy 的 `GetWeaponTargetLocation()` 打偏 | 默认实现用 ActorForwardVector，Enemy 朝向和射击方向可能不一致 | Enemy 覆写时直接用 `TargetActor->GetActorLocation()` 作为目标点 |
| Enemy 开火时 `AddWeaponRecoil()` 崩溃 | `AddControllerPitchInput()` 需要 PlayerController，Enemy 用 AIController | Enemy 的 `AddWeaponRecoil()` 保持空实现（基类默认就是空实现） |
| FirstPersonMesh 的 `bOnlyOwnerSee` 在多玩家时互相看不到第三人称 | 旧代码中 `GetMesh()->bOwnerNoSee = true` 被移到 PlayerCharacter 构造函数 | 确保 Enemy 不设置 `bOwnerNoSee`，Enemy 的第三人称网格对所有客户端可见 |
| 旧 `UBaseCharacterAttributeSet` 的 BP 引用断裂 | 拆分后旧类被删除 | 添加 `ActiveClassRedirects`；或保留旧文件为空壳并标记 `UE_DEPRECATED` |
| Enemy 无 `SetupPlayerInputComponent` | `ABaseCharacter` 不再 override 此函数，`APlayerCharacter` 才 override | 这是正确行为，Enemy 不需要输入绑定 |

---

## 10. 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `BaseCharacter.h/.cpp` | **重构** | 移除 FP 组件、输入、体力；添加 `OnDeath()`、虚函数 |
| `PlayerCharacter.h/.cpp` | **新建** | 从旧 BaseCharacter 移入 FP + 输入 + 体力 |
| `BaseEnemy.h/.cpp` | **重构** | 改继承 ABaseCharacter；添加 AI、射击、头顶血条 |
| `BaseHealthAttributeSet.h/.cpp` | **新建** | 从 CharacterAttributeSet 拆出 Health 部分 |
| `BaseStaminaAttributeSet.h/.cpp` | **新建** | 从 CharacterAttributeSet 拆出 Stamina 部分 |
| `BaseCharacterAttributeSet.h/.cpp` | **废弃** | 添加 ActiveClassRedirect 指向 BaseHealthAttributeSet |
| `BasePlayerController.cpp` | **修改** | `TryBindDelegate` 中 Cast 改为 `APlayerCharacter`（体力条） |
| `BaseHealthBarWidget.cpp` | **修改** | TryBindDelegate 保持 `Cast<ABaseCharacter>` |
| `BaseStaminaBarWidget.cpp` | **修改** | TryBindDelegate 改为 `Cast<APlayerCharacter>` |
| `BaseGameplayTags.h/.cpp` | **无变更** | 已有 Event_Death / State_Dead |
| `BaseAbilitySystemComponent.h/.cpp` | **无变更** | 已有 Fire / Reload 句柄 |
| `IBaseWeaponHolder.h` | **无变更** | 接口方法覆盖完整 |
| `Config/DefaultEngine.ini` | **修改** | 添加 ActiveClassRedirects |

---

## 11. 后续扩展

当前 Enemy 使用简单 Tick 驱动 AI。当 AI 复杂度增加时，可迁移到 BehaviorTree：

```
1. 创建 UBehaviorTree + UBlackboardData
2. 创建 ABaseAIController（继承 AAIController）
3. Enemy 改用 ABaseAIController 并设置 BehaviorTree
4. 将 Tick 逻辑迁移到 BTTask / BTService / BTDecorator
```

这种渐进式设计确保当前简单 AI 可用，未来不破坏现有代码即可升级。
