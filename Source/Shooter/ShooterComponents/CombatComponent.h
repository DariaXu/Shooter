// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shooter/HUD/ShooterHUD.h"
#include "CombatComponent.generated.h"

#define TRACE_LENGTH 80000.f

class AWeapon;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SHOOTER_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// because combat is made for character,  
	// give the character full access including all private, protected and public.
	friend class AShooterCharacter;
	// Sets default values for this component's properties
	UCombatComponent();
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void EquipWeapon(AWeapon* WeaponToEquip);

protected: // for child class to inherence
	// Called when the game starts
	virtual void BeginPlay() override;

	/**
	* Weapon
	*/
	UFUNCTION()
	void OnRep_EquippedWeapon();

	/**
	* HUD
	*/
	void TraceUnderCrosshairs(FHitResult& TraceHitResult);
	void SetHUDCrosshairs(float DeltaTime);

	/**
	* Aiming
	*/
	void SetAiming(bool bIsAiming);
	// server RPC, can have input parameter
	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bIsAiming);

	/**
	* Firing 
	*/
	void FireBtnPressed(bool bPressed);
	void Fire();
	// server RPC
	// FVector_NetQuantize a more effective way to send vector through network
	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize& TraceHitTarget);
	// calling netmulticast on sever side will execute on both client and sever (Server broadcasting)
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);	

private:
	UPROPERTY()
	class AShooterCharacter* Character;
	UPROPERTY()
	class AShooterPlayerController* Controller;
	UPROPERTY()
	class AShooterHUD* HUD;

	/**
	* Weapon
	*/
	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
	AWeapon* EquippedWeapon;

	/**
	* HUD and crosshairs
	*/
	float CrosshairVelocityFactor;
	float CrosshairInAirFactor;
	float CrosshairAimFactor;
	float CrosshairShootingFactor;

	FHUDPackage HUDPackage;

	UPROPERTY(EditAnywhere)
	float BaseWalkSpeed;

	UPROPERTY(EditAnywhere)
	float AimWalkSpeed;

	/**
	* Aiming
	*/
	UPROPERTY(Replicated)
	bool bAiming;
	// Aiming and FOV
	// Field of view when not aiming; set to the camera's base FOV in BeginPlay
	float DefaultFOV;
	float CurrentFOV;
	// default value 
	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomedFOV = 30.f;

	UPROPERTY(EditAnywhere, Category = Combat)
	float ZoomInterpSpeed = 20.f;

	void InterpFOV(float DeltaTime);

	/**
	* Firing 
	*/
	bool bFiredBtnPressed;
	FVector HitTarget;
	// Automatic Fire
	FTimerHandle FireTimer;
	bool bCanFire = true;

	void StartFireTimer();
	void FireTimerFinished();
};
