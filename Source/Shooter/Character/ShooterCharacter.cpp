// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Shooter/Weapon/Weapon.h"
#include "Shooter/ShooterComponents/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ShooterAnimInstance.h"
#include "Shooter/Shooter.h"
#include "Shooter/PlayerController/ShooterPlayerController.h"
#include "Shooter/GameMode/ShooterGameMode.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "Shooter/PlayerState/ShooterPlayerState.h"
#include "Shooter/Weapon/WeaponTypes.h"
#include "Shooter/GameState/ShooterGameState.h"


// Sets default values
AShooterCharacter::AShooterCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CameraBoom->bUsePawnControlRotation = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	// combat
	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	// components don't need to register to replicated, only set the following bool
	Combat->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	// Fix issues collision with camera
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	// set it to channel SkeletalMesh
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	// Set visibility to block for player to hit each other
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);

	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateHUDHealth();
	if (HasAuthority())
	{
		// after the projectile hit a character, it will call ApplyDamage from projectile.
		// Then the actor the projectile hit will be passed in as the parameter, 
		// and ReceiveDamage will be called with information passed in.
		OnTakeAnyDamage.AddDynamic(this, &AShooterCharacter::ReceiveDamage);
	}
}

void AShooterCharacter::Destroyed()
{
	Super::Destroyed();

	if (ElimBotComponent)
	{
		ElimBotComponent->DestroyComponent();
	}

	AShooterGameMode* ShooterGameMode = Cast<AShooterGameMode>(UGameplayStatics::GetGameMode(this));
	bool bMatchNotInProgress = ShooterGameMode && ShooterGameMode->GetMatchState() != MatchState::InProgress;
	if (Combat && Combat->EquippedWeapon && bMatchNotInProgress)
	{
		Combat->EquippedWeapon->Destroy();
	}
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RotateInPlace(DeltaTime);
	HideCameraIfCharacterClose();
	PollInit();
}

void AShooterCharacter::PollInit()
{
	if (ShooterPlayerState == nullptr)
	{
		ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
		if (ShooterPlayerState)
		{
			// initialize
			ShooterPlayerState->AddToScore(0.f);
			ShooterPlayerState->AddToDefeats(0);
		}
	}
}

void AShooterCharacter::RotateInPlace(float DeltaTime)
{
	// if (bDisableGameplay)
	// {
	// 	bUseControllerRotationYaw = false;
	// 	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	// 	return;
	// }

	if (GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled())
	{
		// Anything above Simulated Proxy is going to be something that is actively locally controlled or is on the server.
		// and locally controlled
		AimOffset(DeltaTime);
	}
	else
	{
		// make sure get call every .25s tp prevent not calling when the charater is not moving
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f) OnRep_ReplicatedMovement();
		// calculating AO_pitch every frame
		CalculateAO_Pitch();
	}
}

void AShooterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
}

void AShooterCharacter::HideCameraIfCharacterClose()
{
	if (!IsLocallyControlled()) return;
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		GetMesh()->SetVisibility(false);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			// set weapon mesh visiblity to false only for the owner
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(true);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

#pragma region Input and Action Setup 
//================================================================================
// Input and Action Setup 
//================================================================================

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AShooterCharacter::Jump);
	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUp);

	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &AShooterCharacter::EquipBtnPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &AShooterCharacter::CrouchBtnPressed);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &AShooterCharacter::AimBtnPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &AShooterCharacter::AimBtnReleased);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AShooterCharacter::FireBtnPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AShooterCharacter::FireBtnReleased);
	PlayerInputComponent->BindAction("Reload", IE_Released, this, &AShooterCharacter::ReloadBtnPressed);
}

void AShooterCharacter::MoveForward(float Value)
{
	// if (bDisableGameplay) return;
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void AShooterCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void AShooterCharacter::EquipBtnPressed()
{
	if (Combat) 
	{
		if (HasAuthority())
		{
			// on server
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else
		{
			// on client send RPC
			ServerEquipButtonPressed();
		}
		
	}
}

void AShooterCharacter::CrouchBtnPressed()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}else
	{
		Crouch();
	}
	
}

void AShooterCharacter::AimBtnPressed()
{
	if (Combat) 
	{
		Combat->SetAiming(true);
	}
}

void AShooterCharacter::AimBtnReleased()
{
	if (Combat) 
	{
		Combat->SetAiming(false);
	}
}

void AShooterCharacter::FireBtnPressed()
{
	if (Combat)
	{
		Combat->FireBtnPressed(true);
	}
}

void AShooterCharacter::FireBtnReleased()
{
	if (Combat)
	{
		Combat->FireBtnPressed(false);
	}
}

void AShooterCharacter::ReloadBtnPressed()
{
	if (Combat)
	{
		Combat->Reload();
	}
}

void AShooterCharacter::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}
#pragma endregion

#pragma region Replicate
//================================================================================
// Replicate 
//================================================================================

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// register the weapon variable; replicated class, replicated variable
	// first on server, then replicated to the client
	// using CONDITION: OverlappingWeapon will only replicated to the client own this character 
	// (to show the widget only on the client who own this character)
	DOREPLIFETIME_CONDITION(AShooterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(AShooterCharacter, Health);
	DOREPLIFETIME(AShooterCharacter, bDisableGameplay);
}
#pragma endregion

#pragma region Weapon
//================================================================================
// Weapon Overlapping for equipment
//================================================================================
// this function only get calls on the server
void AShooterCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (OverlappingWeapon)
	{
		// when set to nullptr, hide the previous weapon widget before change OverlappingWeapon 
		OverlappingWeapon->ShowPickupWidget(false);
	}

	OverlappingWeapon = Weapon;
	if (IsLocallyControlled())
	{
		// IsLocallyControlled() is true only when is called on the charater who is actually being controlled
		// (this player is the server)
		// GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Server_overlap"));
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

// called on client
void AShooterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	// GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Client_overlap"));
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}

	// hide the widget for last overlapping weapon
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

// RPC
// define what happens when the RPC is executed on the server
void AShooterCharacter::ServerEquipButtonPressed_Implementation()
{
	// will only be called on the server
	if (Combat) 
	{
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

void AShooterCharacter::UpdateHUDWeaponAmmo(bool bIfShow)
{
	ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
	if (ShooterPlayerController)
	{
		ShooterPlayerController->SetHUDWeaponAmmo(0);
		ShooterPlayerController->ShowHUDWeaponAmmo(false);
	}
}
#pragma endregion

#pragma region Aim and Turn
//================================================================================
// Aim Offset and Turning
//================================================================================

float AShooterCharacter::CalculateSpeed()
{
    FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void AShooterCharacter::CalculateAO_Pitch()
{
	// Update Pitch movement 

	// pitch regardless of running or standing
	AO_Pitch = GetBaseAimRotation().Pitch;

	// only fix this for the characters that is not locally controlled, 
	// because this compression issues will only happen when sending packets through the network
	if (!IsLocallyControlled() && AO_Pitch > 90.f)
	{
		// map pitch from [270, 360) to [-90, 0)
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void AShooterCharacter::AimOffset(float DeltaTime)
{
	// skip if the charater is not currently equipping a weapon
	if (Combat && Combat->EquippedWeapon == nullptr) return;
	// calculating speed
	
	bool bIsInAir = GetCharacterMovement()->IsFalling();
	float Speed = CalculateSpeed();

	if (Speed == 0.f && !bIsInAir) // standing still, not jumping
	{
		bRotateRootBone = true;
		// calculating the rotation
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		// The difference between the current yaw and base yaw, the order matters
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw=AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		// bUseControllerRotationYaw = false;
		TurnInPlace(DeltaTime);
	}

	if (Speed > 0.f || bIsInAir) // running, or jumping
	{
		bRotateRootBone = false;
		// store the base aim rotation, FRotator(Pitch, Yaw, Roll);
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		// When moving, AO_Yaw need to be set to zero
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	CalculateAO_Pitch();
}

void AShooterCharacter::SimProxiesTurn()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	// because are simulated proxy and not locally controlled, shouldn't rotate the root bone
	// In other words, if locally controlled either on the server or a client, 
	// should set this value to true and use the aim offset function only
	bRotateRootBone = false;
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

	// when walking, don't turn to avoid sliding
	float Speed = CalculateSpeed();
	if (Speed > 0.f) return;
	
	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	// get the difference between these two (how much rotation from last frame)
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	// UE_LOG(LogTemp, Warning, TEXT("ProxyYaw: %f"), ProxyYaw);

	if (FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		// if greater than the threshold, will play the animation
		if (ProxyYaw > TurnThreshold)
		{
			// UE_LOG(LogTemp, Warning, TEXT("Sim Turn Right: %f"), ProxyYaw);
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold)
		{
			// UE_LOG(LogTemp, Warning, TEXT("Sim Turn Left: %f"), ProxyYaw);
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
	}
}

void AShooterCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		// UE_LOG(LogTemp, Warning, TEXT("TurnInPlace Turn Left: %f"), ProxyYaw);
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		// UE_LOG(LogTemp, Warning, TEXT("TurnInPlace Turn Left: %f"), ProxyYaw);
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}

	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning) 
	{
		// interp down to 0, speed 10
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		// check if turned enough
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			// After turned enough, stop and reset the aim rotation
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void AShooterCharacter::OnRep_ReplicatedMovement()
{
	// this will be called every time when the actors movement changes (walk or jump)
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	
	// SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f;
}

#pragma endregion

#pragma region Hit and Firing
//================================================================================
// Hit and Firing
//================================================================================
void AShooterCharacter::PlayHitReactMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

// void AShooterCharacter::MulticastHit_Implementation()
// {
// 	// UE_LOG(LogTemp, Warning, TEXT("MulticastHit"));
// 	PlayHitReactMontage();
// }

void AShooterCharacter::PlayFireMontage(bool bAiming)
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		// playing montage animation
		AnimInstance->Montage_Play(FireWeaponMontage);
		// Montage section name
		FName SectionName;
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AShooterCharacter::PlayReloadMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ReloadMontage)
	{
		// playing montage animation
		AnimInstance->Montage_Play(ReloadMontage);
		// Montage section name
		FName SectionName;
		switch (Combat->EquippedWeapon->GetWeaponType())
		{
			case EWeaponType::EWT_AssaultRifle:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_RocketLauncher:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_Pistol:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_SubmachineGun:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_Shotgun:
				SectionName = FName("Rifle");
				break;
		}
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

#pragma endregion

#pragma region Player Health
void AShooterCharacter::UpdateHUDHealth()
{
	// set hud health
	ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
	if (ShooterPlayerController)
	{
		ShooterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void AShooterCharacter::OnRep_Health()
{
	// OnRep_Health will only be called on clients
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void AShooterCharacter::ReceiveDamage(AActor *DamagedActor, float Damage, const UDamageType *DamageType, AController *InstigatorController, AActor *DamageCauser)
{
	// health is replicated, every time it changed, OnRep_Health will be called (which will only be called on client not on sever)
	// (using variable replication is more efficient than sending an RPC.)

	// when at cool down state
	if(bDisableGameplay) return;

	// clamp will never go below 0 and above maxHealth
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);

	// ReceiveDamage will only be called on the server, these functions need to be called on client as well
	UpdateHUDHealth();
	if (Health > 0.f)
	{
		PlayHitReactMontage();
	}

	// when the play need to be Eliminate
	if (Health == 0.f)
	{
		AShooterGameMode* ShooterGameMode = GetWorld()->GetAuthGameMode<AShooterGameMode>();
		if (ShooterGameMode)
		{
			ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
			AShooterPlayerController* AttackerController = Cast<AShooterPlayerController>(InstigatorController);
			
			ShooterGameMode->PlayerEliminated(this, ShooterPlayerController, AttackerController);
		}
	}

}
#pragma endregion

#pragma region Player Eliminate and Dissolve effect

void AShooterCharacter::Elim()
{
	if (Combat && Combat->EquippedWeapon)
	{
		Combat->EquippedWeapon->Dropped();
	}

	MulticastElim();
	
	GetWorldTimerManager().SetTimer(
		ElimTimer,
		this,
		&AShooterCharacter::ElimTimerFinished,
		ElimDelay
	);

}

void AShooterCharacter::ElimTimerFinished()
{
	AShooterGameMode* ShooterGameMode = GetWorld()->GetAuthGameMode<AShooterGameMode>();
	if (ShooterGameMode)
	{
		ShooterGameMode->RequestRespawn(this, Controller);
	}
}

void AShooterCharacter::MulticastElim_Implementation()
{
	bElimmed = true;

	if (Combat)
	{
		Combat->FireBtnPressed(false);
	}
	// Disable character movement
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
	bDisableGameplay = true;

	if (ShooterPlayerController)
	{
		UpdateHUDWeaponAmmo(false);
		// stop firing
		DisableInput(ShooterPlayerController);
	}

	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlayElimMontage();
}

void AShooterCharacter::PlayElimMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	
	if (AnimInstance && ElimMontage)
	{
		FName SectionName;
		SectionName = IsAiming() ? FName("Ironsights") : FName("Stand");

		int32 SectionIndex = ElimMontage->GetSectionIndex(SectionName);
		float ElimAnimationDelay = ElimMontage->GetSectionLength(SectionIndex) * .85f;

		GetWorldTimerManager().SetTimer(
			ElimAnimationTimer,
			this,
			&AShooterCharacter::ElimAnimationTimerFinished,
			ElimAnimationDelay
		);

		AnimInstance->Montage_Play(ElimMontage);
		// FName SectionName("Stand");
		AnimInstance->Montage_JumpToSection(SectionName);
		
	}
}

void AShooterCharacter::ElimAnimationTimerFinished()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	// if (!HasAuthority()) 
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("On client Pause"));
	// }
	// else
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("On Server Pause"));
	// }
	if (AnimInstance && ElimMontage && AnimInstance->Montage_IsPlaying(ElimMontage))
	// if (AnimInstance && ElimMontage)
	{
		// GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Pause"));
		AnimInstance->Montage_Pause(ElimMontage);
	}

	// dissolving
	if (DissolveMaterialInstance)
	{
		// create the dynamic instance
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		// 0 as index
		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
		// Set the default, 0.55 as showing the complete material
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	StartDissolve();

	// Spawn elim bot
	if (ElimBotEffect)
	{
		// 200 unit above the character
		FVector ElimBotSpawnPoint(GetActorLocation().X-160.f , GetActorLocation().Y+35.f, GetActorLocation().Z + 200.f);
		ElimBotComponent = UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			ElimBotEffect,
			ElimBotSpawnPoint,
			GetActorRotation()
		);
	}

	// play elim bot sound
	if (ElimBotSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(
			this,
			ElimBotSound,
			GetActorLocation()
		);
	}
}

void AShooterCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	//use then passed float value to update the Dissolve parameter on the material instance
	if (DynamicDissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void AShooterCharacter::StartDissolve()
{
	// bing the callback
	DissolveTrack.BindDynamic(this, &AShooterCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline)
	{
		// once playing the timeline, it'll start using the curve and call the callback bound to the DissolveTrack
		// add the curve to the timeline
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		// start the timeline
		// As soon as we call start dissolve, it will find the callback to the dissolve track delegate.
		// And if the dissolve curve is valid, the curve will be added to the timeline and tell the timeline to use the
		// dissolve track to look for the callback function, to call every frame and pass that output value from the curve.
		DissolveTimeline->Play();
	}
}
#pragma endregion

#pragma region Getter
//================================================================================
// Get Function for Status 
//================================================================================

bool AShooterCharacter::IsWeaponEquipped()
{
	return (Combat && Combat->EquippedWeapon);
}

bool AShooterCharacter::IsAiming()
{
	return (Combat && Combat->bAiming);
}

AWeapon* AShooterCharacter::GetEquippedWeapon()
{
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

FVector AShooterCharacter::GetHitTarget() const
{
	if (Combat == nullptr) return FVector();
    return Combat->HitTarget;
}

ECombatState AShooterCharacter::GetCombatState() const
{
	if (Combat == nullptr) return ECombatState::ECS_MAX;
	return Combat->CombatState;
}
#pragma endregion