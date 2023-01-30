// Fill out your copyright notice in the Description page of Project Settings.


#include "OverheadWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"

void UOverheadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	// ENetRole RemoteRole = InPawn->GetRemoteRole();
	// FString Role;
	// switch (RemoteRole)
	// {
    //     // On the client, the player's "Remote Role" is its Local Role on the server. 
    //     // On the server, the player's "Remote Role" is its Local Role on the Clien
	//     case ENetRole::ROLE_Authority:
    //         // my player on server (local) 
    //         // client side (remote)
	// 	    Role = FString("Authority");
	// 	    break;
	//     case ENetRole::ROLE_AutonomousProxy:
    //         // other player character (local)
    //         //  my player on server side (remote)
	// 	    Role = FString("Autonomous Proxy");
	// 	    break;
	//     case ENetRole::ROLE_SimulatedProxy:
    //         // my player on client side (local)
    //         // other player on server side (remote)
	// 	    Role = FString("Simulated Proxy");
	// 	    break;
	//     case ENetRole::ROLE_None:
	// 	    Role = FString("None");
	// 	    break;
	// }
	// FString RemoteRoleString = FString::Printf(TEXT("Remote Role: %s"), *Role);
	// SetDisplayText(RemoteRoleString);


	// show name
	FString playerName;
	APlayerState* PlayerState = InPawn->GetPlayerState();
	if(PlayerState)
	{
		playerName = PlayerState->GetPlayerName();
	}
	else
	{
		playerName = "Unknown";
	}

	playerName = "";
	SetDisplayText(playerName);
}

void UOverheadWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	RemoveFromParent();
	Super::OnLevelRemovedFromWorld(InLevel, InWorld);
}

