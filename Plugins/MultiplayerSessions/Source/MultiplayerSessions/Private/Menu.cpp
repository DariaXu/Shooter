// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionSubsystem.h"
#include "OnlineSessionSettings.h"


void UMenu::MenuSetup(int32 NumOfPublicConnection, FString TypeOfMatch, FString LobbyPath) 
{
	AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    bIsFocusable = true;

    UWorld* World = GetWorld();
    if (World)
    {
        APlayerController* PlayerController = World->GetFirstPlayerController();
        if (PlayerController)
        {
            FInputModeUIOnly InputModeData;
            InputModeData.SetWidgetToFocus(TakeWidget());
            // don't lock mouse to the view port
            InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PlayerController->SetInputMode(InputModeData);
            PlayerController->SetShowMouseCursor(true);
        }
    }

    NumPublicConnection = NumOfPublicConnection;
    MatchType = TypeOfMatch;
    Path2Lobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);

    UGameInstance* GameInstance = GetGameInstance();
    if(GameInstance)
    {
        MultiplayerSessionSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionSubsystem>();
    }

    if (MultiplayerSessionSubsystem)
    {
        // bind callback to the delegate
        MultiplayerSessionSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
        MultiplayerSessionSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
        MultiplayerSessionSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
        MultiplayerSessionSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
        MultiplayerSessionSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
    }
}

bool UMenu::Initialize() 
{
    // constructor, called after widgets created
	if(!Super::Initialize())
    {
        return false;
    }

    if(HostBtn)
    {
        HostBtn->OnClicked.AddDynamic(this, &UMenu::HostBtnClicked);
    }

    if(JoinBtn)
    {
        JoinBtn->OnClicked.AddDynamic(this, &UMenu::JoinBtnClicked);
    }

    return true;
}

void UMenu::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld) 
{
	MenuTearDown();
    Super::OnLevelRemovedFromWorld(InLevel, InWorld);
}

void UMenu::OnCreateSession(bool bWasSuccessful) 
{
    if(bWasSuccessful)
    {
        // PrintDebugMsg(FColor::Orange, FString(TEXT("Session created successfully!")));
        UWorld* World = GetWorld();

        if (World)
        {
            World->ServerTravel(Path2Lobby);
        }
        
    }
	else
    {
		PrintDebugMsg(FColor::Red, FString::Printf(TEXT("Failed to create session!")));
        HostBtn->SetIsEnabled(true);
	}
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResult, bool bWasSuccessful) 
{
    if (MultiplayerSessionSubsystem == nullptr)
	{
		return;
	}

	for (auto Result : SessionResult)
	{
		FString Id = Result.GetSessionIdStr();
		FString User = Result.Session.OwningUserName;

		FString TypeValue; 
		Result.Session.SessionSettings.Get(FName("MatchType"), TypeValue);

		// PrintDebugMsg(FColor::Cyan, FString::Printf(TEXT("Found Session with Id: %s, User: %s"), *Id, *User));

		if(MatchType == TypeValue)
		{
			PrintDebugMsg(FColor::Cyan, FString::Printf(TEXT("Joining Match Type: %s"), *MatchType));
            MultiplayerSessionSubsystem->JoinSession(Result);
            return;
		}
	}

    if(!bWasSuccessful || SessionResult.Num() == 0)
    {
        PrintDebugMsg(FColor::Red, FString::Printf(TEXT("Fail to find Session!")));
        JoinBtn->SetIsEnabled(true);
    }
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result) 
{
    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        PrintDebugMsg(FColor::Red, FString::Printf(TEXT("Fail to join Session!")));
        JoinBtn->SetIsEnabled(true);
        return;
    }

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem)
    {
        IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
        if (!SessionInterface.IsValid())
        {
            return;
        }

        // store the Ip address
        FString Address;
        if(SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
        {
            PrintDebugMsg(FColor::Yellow, FString::Printf(TEXT("Connect String: %s"), *Address));
            APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
            if (PlayerController)
            {
                PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
            }
        }
    }
	
}

void UMenu::OnDestroySession(bool bWasSuccessful) {
	
}

void UMenu::OnStartSession(bool bWasSuccessful) 
{
	if(bWasSuccessful)
    {
        PrintDebugMsg(FColor::Orange, FString(TEXT("Session started!")));
        // travel to start level
        // UWorld* World = GetWorld();

        // if (World)
        // {
        //     World->ServerTravel(Path2Lobby);
        // }
        
    }
	else
    {
		PrintDebugMsg(FColor::Red, FString::Printf(TEXT("Failed to start session!")));
	}
}

void UMenu::HostBtnClicked() 
{
    HostBtn->SetIsEnabled(false);
    if(MultiplayerSessionSubsystem)
    {
        MultiplayerSessionSubsystem->CreateSession(NumPublicConnection, MatchType);
    }
}

void UMenu::JoinBtnClicked() 
{
    JoinBtn->SetIsEnabled(false);
	if(MultiplayerSessionSubsystem)
    {    
        MultiplayerSessionSubsystem->FindSessions(10000);
    }
}

void UMenu::MenuTearDown() 
{
	RemoveFromParent();
    UWorld* World = GetWorld();

    if (World)
    {
        APlayerController* PlayerController = World->GetFirstPlayerController();
        if (PlayerController)
        {
            FInputModeGameOnly InputModeData;
            PlayerController->SetInputMode(InputModeData);
            PlayerController->SetShowMouseCursor(false);
        }
    }
}

void UMenu::PrintDebugMsg(FColor color, FString msg) 
{
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.f, color, msg);
	}
}
