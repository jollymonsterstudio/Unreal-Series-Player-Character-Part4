// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PunchKick04GameMode.h"
#include "PunchKick04Character.h"
#include "UObject/ConstructorHelpers.h"

APunchKick04GameMode::APunchKick04GameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
