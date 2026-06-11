// Fill out your copyright notice in the Description page of Project Settings.

#include "AuraPlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Interaction/EnemyInterface.h"
#include "AuroPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

AAuraPlayerController::AAuraPlayerController()
{
	bReplicates = true;
}

void AAuraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	CursorTrace();
    RotatePawnToCursor(); // Rotación constante hacia el mouse

	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn)
	{
		if (!bHasSpawnLocation)
		{
			SpawnLocation = ControlledPawn->GetActorLocation();
			bHasSpawnLocation = true;
		}

		// Sistema Anti-Caídas: si cae por debajo del umbral del Player Start
		if (ControlledPawn->GetActorLocation().Z < (SpawnLocation.Z - FallThresholdOffset))
		{
			// Resetear velocidad física si es Character para evitar inercias
			if (ACharacter* ControlledChar = Cast<ACharacter>(ControlledPawn))
			{
				if (UCharacterMovementComponent* MoveComp = ControlledChar->GetCharacterMovement())
				{
					MoveComp->Velocity = FVector::ZeroVector;
				}
			}
			
			// Teletransportar de vuelta a la posición inicial (Player Start)
			ControlledPawn->SetActorLocation(SpawnLocation);
		}
	}
}

void AAuraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	check(AuraContext);
	
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if(Subsystem)
	{
		Subsystem->AddMappingContext(AuraContext, 0);	
	}
	
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	
	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}

void AAuraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
	
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAuraPlayerController::Move);
	EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AAuraPlayerController::Attack);
	EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AAuraPlayerController::Dash);
	EnhancedInputComponent->BindAction(AreaAttackAction, ETriggerEvent::Started, this, &AAuraPlayerController::AreaAttack);
	
	if (InteractAction)
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AAuraPlayerController::Interact);
	}
	if (RestartAction)
	{
		EnhancedInputComponent->BindAction(RestartAction, ETriggerEvent::Started, this, &AAuraPlayerController::QuickRestart);
	}
}

void AAuraPlayerController::Interact()
{
	if (AAuroPlayer* AuraPlayer = Cast<AAuroPlayer>(GetPawn()))
	{
		AuraPlayer->Interact();
	}
}

void AAuraPlayerController::RotatePawnToCursor()
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn) return;

    FHitResult Hit;
    // Trazamos desde el mouse al mundo
    if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        FVector TargetLocation = Hit.ImpactPoint;
        FVector PawnLocation = ControlledPawn->GetActorLocation();
        
        FVector Direction = (TargetLocation - PawnLocation);
        Direction.Z = 0.f; // Mantener rotación solo en el eje Yaw

        if (!Direction.IsNearlyZero())
        {
            FRotator NewRotation = Direction.Rotation();
            ControlledPawn->SetActorRotation(NewRotation);
        }
    }
}

void AAuraPlayerController::Move(const FInputActionValue& InputActionValue)
{
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	// Movimiento absoluto en el plano del mundo (strafing estilo Hotline Miami)
	ControlledPawn->AddMovementInput(FVector::ForwardVector, InputAxisVector.Y);
	ControlledPawn->AddMovementInput(FVector::RightVector, InputAxisVector.X);
}

// ... Resto de funciones (CursorTrace, Attack, Dash, AreaAttack) se mantienen igual que en tu archivo original ...

void AAuraPlayerController::CursorTrace()
{
	FHitResult CursorHit;
	GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);
	if (!CursorHit.bBlockingHit) return;
	
	LastActor = ThisActor;
	ThisActor = CursorHit.GetActor();
	
	if (LastActor == nullptr)
	{
		if (ThisActor != nullptr) ThisActor->HighlightActor();
	}
	else 
	{
		if (ThisActor == nullptr) LastActor->UnHighlightActor();
		else if (LastActor != ThisActor)
		{
			LastActor->UnHighlightActor();
			ThisActor->HighlightActor();
		}
	}
}

void AAuraPlayerController::Attack()
{
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn))
		{
			FGameplayTagContainer TagContainer;
			TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Input.Attack")));
			ASC->TryActivateAbilitiesByTag(TagContainer);
		}
	}
}

void AAuraPlayerController::Dash()
{
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn))
		{
			FGameplayTagContainer TagContainer;
			TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Input.Dash")));
			ASC->TryActivateAbilitiesByTag(TagContainer);
		}
	}
}

void AAuraPlayerController::AreaAttack()
{
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn))
		{
			FGameplayTagContainer TagContainer;
			TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Input.AreaAttack")));
			ASC->TryActivateAbilitiesByTag(TagContainer);
		}
	}
}

void AAuraPlayerController::QuickRestart()
{
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()));
}