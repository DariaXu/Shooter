// Fill out your copyright notice in the Description page of Project Settings.


#include "HitScanWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Shooter/Character/ShooterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

void AHitScanWeapon::Fire(const FVector &HitTarget)
{
    Super::Fire(HitTarget);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
    
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket)
	{
        // create line trace
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector Start = SocketTransform.GetLocation();
        // passed the aiming location, if it is exactly at the hit target, 
        // we might not get a blocking hit as that location is exactly on the mesh surface.
		FVector End = Start + (HitTarget - Start) * 1.25f;

		FHitResult FireHit;
		UWorld* World = GetWorld();
		if (World)
		{
            // perform line trace on all machine
			World->LineTraceSingleByChannel(
				FireHit,
				Start,
				End,
				ECollisionChannel::ECC_Visibility
			);

			FVector BeamEnd = End;

            // if hit
			if (FireHit.bBlockingHit)
			{
				BeamEnd = FireHit.ImpactPoint;

				AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(FireHit.GetActor());
				//  controller of who fired
				// instigator controller will be null on all simulated proxies.
				// controllers only exist for the player that's using them to control their pawn.
				AController* InstigatorController = OwnerPawn->GetController();
				if (ShooterCharacter && HasAuthority() && InstigatorController)
				{
                    // apply damage
					UGameplayStatics::ApplyDamage(
						ShooterCharacter,
						Damage,
						InstigatorController,
						this,
						UDamageType::StaticClass()
					);
				}

                // spawn impact particles
				if (ImpactParticles)
				{
					UGameplayStatics::SpawnEmitterAtLocation(
						World,
						ImpactParticles,
						FireHit.ImpactPoint,
						FireHit.ImpactNormal.Rotation()
					);
				}

				// sound
				if (HitSound)
				{
					UGameplayStatics::PlaySoundAtLocation(
						this,
						HitSound,
						FireHit.ImpactPoint
					);
				}	
			}

			// smoke trail
			if (BeamParticles)
			{
				// spawn the beam
				UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
					World,
					BeamParticles,
					SocketTransform // starting point
				);

				if (Beam)
				{
					Beam->SetVectorParameter(FName("Target"), BeamEnd);
				}
			}
		}

		// casing
		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				World,
				MuzzleFlash,
				SocketTransform
			);
		}

		// fire sound
		if (FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				FireSound,
				GetActorLocation()
			);
		}
	}
}
