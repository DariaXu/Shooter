// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Shooter/ShooterTypes/TurningInPlace.h"
#include "Shooter/Interfaces/CrosshairInteractionInterface.h"
#include "Components/TimelineComponent.h"
#include "Shooter/ShooterTypes/CombatState.h"
#include "ShooterCharacter.generated.h"

UCLASS()
class SHOOTER_API AShooterCharacter : public ACharacter, public ICrosshairInteractionInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AShooterCharacter();
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PostInitializeComponents() override;

	// register variable to be replicated
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void PlayFireMontage(bool bAiming);
	void PlayElimMontage();
	void PlayReloadMontage();
	void PlayThrowGrenadeMontage();
	// UFUNCTION(NetMulticast, Unreliable)
	// void MulticastHit();

	virtual void OnRep_ReplicatedMovement() override;

	void Elim();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastElim();

	UPROPERTY(Replicated)
	bool bDisableGameplay = false;

	virtual void Destroyed() override;

	UFUNCTION(BlueprintImplementableEvent)
	void ShowSniperScopeWidget(bool bShowScope);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);

	virtual void Jump() override;

	void EquipBtnPressed();
	void CrouchBtnPressed();
	void AimBtnPressed();
	void AimBtnReleased();
	void AimOffset(float DeltaTime);

	void FireBtnPressed();
	void FireBtnReleased();
	void GrenadeBtnPressed();

	void ReloadBtnPressed();

	void PlayHitReactMontage();

	void CalculateAO_Pitch();
	void SimProxiesTurn();

	// the callback of the damage delegate
	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, class AController* InstigatorController, AActor* DamageCauser);
	void UpdateHUDHealth();

	void UpdateHUDWeaponAmmo(bool bIfShow);

	// Poll for any relevant classes and initialize our HUD as soon as they are available
	void PollInit();
	void RotateInPlace(float DeltaTime);
	
private:
	//  meta = (AllowPrivateAccess = "true") means the blueprint can access private variable from c++ class
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWidgetComponent* OverheadWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCombatComponent* Combat;

	/**
	* Camera 
	*/
	UPROPERTY(VisibleAnywhere, Category = Camera)
	class USpringArmComponent* CameraBoom;
	UPROPERTY(VisibleAnywhere, Category = Camera)
	class UCameraComponent* FollowCamera;

	void HideCameraIfCharacterClose();

	// the dist from camera to charater 
	UPROPERTY(EditAnywhere)
	float CameraThreshold = 200.f;

	/**
	* Weapon
	*/
	// notify(call) whenever OverlappingWeapon replicate happen (when OverlappingWeapon get changed)
	// replication happens from server to client(one way)
	// hence, this notification function will only get called on the client side
	// and only client side will show the pick up widget
	UPROPERTY(ReplicatedUsing = OnRep_OverlappingWeapon)
	class AWeapon* OverlappingWeapon;
	// this function can only have a parameter with the type that being replicated，
	// will automatic store the last replicated variable into the parameter before current replication happen
	UFUNCTION()
	void OnRep_OverlappingWeapon(AWeapon* LastWeapon);

	// UFUNCTION(Server) is RPCs (Remote Procedure Calls) are functions that are called locally, 
	// but executed remotely on another machine (separate from the calling machine). 
	// reliable RPC is guaranty to be called: if sending information from client to server,
	// reliable allows client to get confirmation when the server receives the RPC;
	// if didn't get this confirmation, resent the RPC
	UFUNCTION(Server, Reliable)
	void ServerEquipButtonPressed();

	/**
	* Rotation and turn
	*/
	float AO_Yaw;
	float InterpAO_Yaw;
	float AO_Pitch;
	FRotator StartingAimRotation;

	bool bRotateRootBone;
	float TurnThreshold = 35.0f;
	FRotator ProxyRotationLastFrame;
	FRotator ProxyRotation;
	float ProxyYaw;
	float TimeSinceLastMovementReplication;
	float CalculateSpeed();

	ETurningInPlace TurningInPlace;
	void TurnInPlace(float DeltaTime);

	/**
	* Play Montage
	*/
	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* FireWeaponMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* HitReactMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* ElimMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* ReloadMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ThrowGrenadeMontage;

	/**
	* Player health
	*/
	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxHealth = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health, VisibleAnywhere, Category = "Player Stats")
	float Health = 100.f;

	UFUNCTION()
	void OnRep_Health();

	UPROPERTY()
	class AShooterPlayerController* ShooterPlayerController;

	/**
	* Player Eliminated and respawning
	*/
	bool bElimmed = false;
	FTimerHandle ElimTimer;
	FTimerHandle ElimAnimationTimer;

	// edit only on default character(everyone should have the same elim delay time)
	UPROPERTY(EditDefaultsOnly)
	float ElimDelay = 3.f;

	// callback function
	void ElimTimerFinished();
	void ElimAnimationTimerFinished();

	/**
	* Dissolve effect
	*/
	//timeline
	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* DissolveTimeline;

	// a dynamic delegate, design to handle timeline time track
	FOnTimelineFloat DissolveTrack;

	UPROPERTY(EditAnywhere)
	UCurveFloat* DissolveCurve;

	// callback function will be called every frame as updating the timeline, 
	// this will be the function that receives the float value corresponding to where we are on the curve.
	UFUNCTION()
	void UpdateDissolveMaterial(float DissolveValue);
	void StartDissolve();

	// Dynamic instance that we can change at runtime
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance;

	// Material instance set on the Blueprint, used with the dynamic material instance
	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance;

	/**
	* Elim bot
	*/
	UPROPERTY(EditAnywhere)
	UParticleSystem* ElimBotEffect;

	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* ElimBotComponent;

	UPROPERTY(EditAnywhere)
	class USoundCue* ElimBotSound;

	UPROPERTY()
	class AShooterPlayerState* ShooterPlayerState;

	/** 
	* Grenade
	*/

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* AttachedGrenade;

public:
	// change only when the overlapping weapon changed on the server, once it changed, it will be replicated to all clients
	void SetOverlappingWeapon(AWeapon* Weapon);
	AWeapon* GetEquippedWeapon();

	bool IsWeaponEquipped();
	bool IsAiming();

	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }
	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE bool ShouldRotateRootBone() const { return bRotateRootBone; }
	FORCEINLINE bool IsElimmed() const { return bElimmed; }
	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	FORCEINLINE UCombatComponent* GetCombat() const { return Combat; }
	FORCEINLINE bool GetDisableGameplay() const { return bDisableGameplay; }
	FORCEINLINE UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
	FORCEINLINE UStaticMeshComponent* GetAttachedGrenade() const { return AttachedGrenade; }

	FVector GetHitTarget() const;
	ECombatState GetCombatState() const;
};

