// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PunchKick04Character.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

#include "Engine.h"

//////////////////////////////////////////////////////////////////////////
// APunchKick04Character

APunchKick04Character::APunchKick04Character()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	// load the attack montage
	static ConstructorHelpers::FObjectFinder<UAnimMontage> MeleeFistAttackMontageObject(TEXT("AnimMontage'/Game/TUTORIAL_RESOURCES/Animations/MeleeFistAttackMontage.MeleeFistAttackMontage'"));
	if(MeleeFistAttackMontageObject.Succeeded()) 
	{
		MeleeFistAttackMontage = MeleeFistAttackMontageObject.Object;
	}
	
	LeftFistCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftFistCollisionBox"));
	LeftFistCollisionBox->SetupAttachment(RootComponent);
	LeftFistCollisionBox->SetCollisionProfileName("NoCollision");
	LeftFistCollisionBox->SetNotifyRigidBodyCollision(false);

	LeftFistCollisionBox->SetHiddenInGame(false);

	RightFistCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RightFistCollisionBox"));
	RightFistCollisionBox->SetupAttachment(RootComponent);
	RightFistCollisionBox->SetCollisionProfileName("NoCollision");
	RightFistCollisionBox->SetNotifyRigidBodyCollision(false);

	RightFistCollisionBox->SetHiddenInGame(false);
}

void APunchKick04Character::BeginPlay()
{
	Super::BeginPlay();

	// attach collision components to sockets based on transformations definitions
	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);

	LeftFistCollisionBox->AttachToComponent(GetMesh(), AttachmentRules, "fist_l_collision");
	RightFistCollisionBox->AttachToComponent(GetMesh(), AttachmentRules, "fist_r_collision");

	LeftFistCollisionBox->OnComponentHit.AddDynamic(this, &APunchKick04Character::OnAttackHit);
	RightFistCollisionBox->OnComponentHit.AddDynamic(this, &APunchKick04Character::OnAttackHit);
}

//////////////////////////////////////////////////////////////////////////
// Input

void APunchKick04Character::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up game play key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &APunchKick04Character::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APunchKick04Character::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &APunchKick04Character::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &APunchKick04Character::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &APunchKick04Character::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &APunchKick04Character::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &APunchKick04Character::OnResetVR);

	// attack functionality
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &APunchKick04Character::AttackInput);
	PlayerInputComponent->BindAction("Attack", IE_Released, this, &APunchKick04Character::AttackEnd);
}

void APunchKick04Character::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void APunchKick04Character::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void APunchKick04Character::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void APunchKick04Character::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void APunchKick04Character::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void APunchKick04Character::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void APunchKick04Character::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void APunchKick04Character::AttackInput()
{
	Log(ELogLevel::INFO, __FUNCTION__);

	// generate  number between 1 and 3:
	int MontageSectionIndex = rand() % 3 + 1;

	// create a montage section
	FString MontageSection = "start_" + FString::FromInt(MontageSectionIndex);

	PlayAnimMontage(MeleeFistAttackMontage, 1.f, FName(*MontageSection));
}

void APunchKick04Character::AttackStart()
{
	Log(ELogLevel::INFO, __FUNCTION__);

	LeftFistCollisionBox->SetCollisionProfileName("Weapon");
	LeftFistCollisionBox->SetNotifyRigidBodyCollision(true);

	RightFistCollisionBox->SetCollisionProfileName("Weapon");
	RightFistCollisionBox->SetNotifyRigidBodyCollision(true);
}

void APunchKick04Character::AttackEnd()
{
	Log(ELogLevel::INFO, __FUNCTION__);

	LeftFistCollisionBox->SetCollisionProfileName("NoCollision");
	LeftFistCollisionBox->SetNotifyRigidBodyCollision(false);

	RightFistCollisionBox->SetCollisionProfileName("NoCollision");
	RightFistCollisionBox->SetNotifyRigidBodyCollision(false);
}

void APunchKick04Character::OnAttackHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Log(ELogLevel::WARNING, __FUNCTION__);
	Log(ELogLevel::WARNING, Hit.GetActor()->GetName());
}

void APunchKick04Character::Log(ELogLevel LogLevel, FString Message)
{
	Log(LogLevel, Message, ELogOutput::ALL);
}

void APunchKick04Character::Log(ELogLevel LogLevel, FString Message, ELogOutput LogOutput)
{
	// only print when screen is selected and the GEngine object is available
	if ((LogOutput == ELogOutput::ALL || LogOutput == ELogOutput::SCREEN) && GEngine) 
	{
		// default color
		FColor LogColor = FColor::Cyan;
		// flip the color based on the type
		switch (LogLevel)
		{
		case ELogLevel::TRACE:
			LogColor = FColor::Green;
			break;
		case ELogLevel::DEBUG:
			LogColor = FColor::Cyan;
			break;
		case ELogLevel::INFO:
			LogColor = FColor::White;
			break;
		case ELogLevel::WARNING:
			LogColor = FColor::Yellow;
			break;
		case ELogLevel::ERROR:
			LogColor = FColor::Red;
			break;
		default:
			break;
		}
		// print the message and leave it on screen ( 4.5f controls the duration )
		GEngine->AddOnScreenDebugMessage(-1, 4.5f, LogColor, Message);
	}

	if (LogOutput == ELogOutput::ALL || LogOutput == ELogOutput::OUTPUT_LOG)
	{
		// flip the message type based on error level
		switch (LogLevel)
		{
		case ELogLevel::TRACE:
			UE_LOG(LogTemp, VeryVerbose, TEXT("%s"), *Message);
			break;
		case ELogLevel::DEBUG:
			UE_LOG(LogTemp, Verbose, TEXT("%s"), *Message);
			break;
		case ELogLevel::INFO:
			UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
			break;
		case ELogLevel::WARNING:
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
			break;
		case ELogLevel::ERROR:
			UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
			break;
		default:
			UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
			break;
		}
	}	
}