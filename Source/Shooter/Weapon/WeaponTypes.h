#pragma once

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	EWT_AssaultRifle UMETA(DisplayName = "Assault Rifle"),

    // used to check how many enum constant
	EWT_MAX UMETA(DisplayName = "DefaultMAX")
};
