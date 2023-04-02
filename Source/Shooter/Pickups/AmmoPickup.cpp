// Fill out your copyright notice in the Description page of Project Settings.


#include "AmmoPickup.h"
#include "Shooter/Character/ShooterCharacter.h"
#include "Shooter/ShooterComponents/CombatComponent.h"

void AAmmoPickup::OnSphereOverlap(
    UPrimitiveComponent* OverlappedComponent, 
    AActor* OtherActor, 
    UPrimitiveComponent* OtherComp, 
    int32 OtherBodyIndex, 
    bool bFromSweep, 
    const FHitResult& SweepResult
    )
{
	Super::OnSphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter)
	{
		UCombatComponent* Combat = ShooterCharacter->GetCombat();
		if (Combat)
		{
			Combat->PickupAmmo(WeaponType, AmmoAmount);
		}
	}
	Destroy();
}

