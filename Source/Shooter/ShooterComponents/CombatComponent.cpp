// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "Components/SphereComponent.h"
#include "Shooter/Weapon/Weapon.h"
#include "Shooter/Character/ShooterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

// Sets default values for this component's properties
UCombatComponent::UCombatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	//  true => tick every frame (calling TickComponent)
	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed = 600.f;
	AimWalkSpeed = 450.f;
}

// Called when the game starts
void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Character) 
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	}
}

// Called every frame
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

//================================================================================
// Replicate variable
//================================================================================

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
}

//================================================================================
// Equip Weapon
//================================================================================
void UCombatComponent::OnRep_EquippedWeapon()
{
	if (EquippedWeapon && Character)
	{
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
	}
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (Character == nullptr || WeaponToEquip == nullptr) return;

	EquippedWeapon = WeaponToEquip;
	// set to replicated at Weapon class
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket)
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
	// auto replicated
	EquippedWeapon->SetOwner(Character);

	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
}

//================================================================================
// Aiming
//================================================================================
void UCombatComponent::SetAiming(bool bIsAiming)
{
	// having this statement will avoid client to wait for responds from sever 
	bAiming = bIsAiming;
	// since this function is a rpc server function, 
	// calling this on the client who own this charater, will result in calling this function on the server
	ServerSetAiming(bIsAiming);

	if (Character) 
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;
	
	if (Character) 
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming? AimWalkSpeed : BaseWalkSpeed;
	}
}

//================================================================================
// Firing
//================================================================================
// will always call on local machine
void UCombatComponent::FireBtnPressed(bool bPressed)
{
	bFiredBtnPressed = bPressed;

	if (bFiredBtnPressed) 
	{
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		// calling sever to fire
		ServerFire(HitResult.ImpactPoint);
	}
}

// called from client and executed from the server
void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	MulticastFire(TraceHitTarget);
}

// called from sever and will execute on both client and sever
// if called from client, will only execute on client(useless)
void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	if (EquippedWeapon == nullptr) return;
	
	// since bFiredBtnPressed is only set on client side
	if (Character)
	{
		// not replicated
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		// get the view port size in screen space
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Cross hair location is the center of the view port
	FVector2D CrosshairPos(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

	// transfer 2D screen space to 3D world space
	FVector CrosshairWldPos;
	FVector CrosshairWldDir;
	// Regarding the GetPlayerController fuction, for each individual machine, player 0 is who currently playing the game 
	// the WorldContextObject is any object in this word to give the reference of which word is in
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairPos, CrosshairWldPos, CrosshairWldDir
	);

	if (bScreenToWorld) // checking if the transform was successful
	{
		FVector Start = CrosshairWldPos;
		FVector End = Start + CrosshairWldDir * TRACE_LENGTH;

		// performing line trace, ECC_Visibility hit any visible object as the end result
		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult,
			Start,
			End,
			ECollisionChannel::ECC_Visibility
		);

		if (!TraceHitResult.bBlockingHit)
		{
			// if nothing gets hit
			TraceHitResult.ImpactPoint = End;
		}

	}
}
