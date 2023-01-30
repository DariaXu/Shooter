// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shooter/HUD/ShooterHUD.h"
#include "Shooter/Weapon/WeaponTypes.h"
#include "Shooter/ShooterTypes/CombatState.h"
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
	void Reload();

	void FireBtnPressed(bool bPressed);

	UFUNCTION(BlueprintCallable)
	void FinishReloading();

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
	void Fire();
	// server RPC
	// FVector_NetQuantize a more effective way to send vector through network
	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize& TraceHitTarget);
	// calling netmulticast on sever side will execute on both client and sever (Server broadcasting)
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);

	/**
	* Reload 
	*/
	UFUNCTION(Server, Reliable)
	void ServerReload();
	void HandleReload();

	int32 AmountToReload();

private:
	UPROPERTY()
	class AShooterCharacter* Character;
	UPROPERTY()
	class AShooterPlayerController* Controller;
	UPROPERTY()
	class AShooterHUD* HUD;

	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	ECombatState CombatState = ECombatState::ECS_Unoccupied;

	UFUNCTION()
	void OnRep_CombatState();

	void SetController();

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
	bool bCanContinueFire = true;

	void StartFireTimer();
	void FireTimerFinished();

	bool CanFire();

	/**
	* Ammo 
	*/
	// Carried ammo for the currently-equipped weapon type
	UPROPERTY(ReplicatedUsing = OnRep_CarriedAmmo)
	int32 CarriedAmmo;

	UFUNCTION()
	void OnRep_CarriedAmmo();

	// key: Ammo type, Value: Amount of carried ammo for that type
	TMap<EWeaponType, int32> CarriedAmmoMap;

	void InitializeCarriedAmmo();

	UPROPERTY(EditAnywhere)
	int32 StartingARAmmo = 30;
	
	/**
	* Reload 
	*/
	void UpdateAmmoValues();
};
