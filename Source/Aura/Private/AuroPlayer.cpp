// Fill out your copyright notice in the Description page of Project Settings.


#include "AuroPlayer.h"

#include "AuraPlayerController.h"
#include "Player/AuraPlayerState.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/HUD/AuraHUD.h"
#include "Actor/AuraWeapon.h"

AAuroPlayer::AAuroPlayer()
{

	//Rotacion del personaje
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;


	//Rotacion de eje cancelada
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
}

void AAuroPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	//Init Ability Actor Info for the server
	InitAbilityActorInfo();
}

void AAuroPlayer::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	//Init Ability Actor Info for the client
	InitAbilityActorInfo();
}

void AAuroPlayer::InitAbilityActorInfo()
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(AuraPlayerState, this);
	AbilitySystemComponent = AuraPlayerState->GetAbilitySystemComponent();
	AttributeSet = AuraPlayerState->GetAttributeSet();
	
 	if (AAuraPlayerController* AuraPlayerController = Cast<AAuraPlayerController>(GetController()))
 	{
 		if (AAuraHUD* AuraHUD =	Cast<AAuraHUD>(AuraPlayerController->GetHUD()))
 		{
 			AuraHUD->InitOverlay(AuraPlayerController, AuraPlayerState, AbilitySystemComponent, AttributeSet);
 		}
 	}
	
	// Otorgar habilidades iniciales (Solo en el servidor para evitar desincronización)
	if (HasAuthority() && AbilitySystemComponent)
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
		{
			// Le damos la habilidad nivel 1
			FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
			AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
	}
	
	// --- AÑADE ESTO AL FINAL DE LA FUNCIÓN ---
	if (AbilitySystemComponent && AttributeSet)
	{
		// Escuchamos los cambios de vida directamente desde el personaje
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			Cast<UAuraAttributeSet>(AttributeSet)->GetHealthAttribute()).AddUObject(this, &AAuroPlayer::OnHealthChanged);
	}
}

void AAuroPlayer::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// Si la nueva vida es 0 o menos, y el personaje aún no está "muerto" (para no llamar a Die 20 veces)
	if (Data.NewValue <= 0.0f)
	{
		Die();
	}
}

void AAuroPlayer::Die()
{
	// 1. Llamamos al Die() original de AuraCharacterBase para que haga el Ragdoll y suelte Loot (si quieres)
	Super::Die();

	// 2. Quitamos el control al jugador para que no siga moviendo la cámara o intentando atacar
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->DisableInput(PC);
	}
	
	// Llamamos al evento que diseñaremos en el Blueprint
	ShowGameOverScreen();

	// 3. Imprimimos el Game Over en pantalla (Próximamente aquí llamaremos a tu Menú de Derrota)
	/*if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("¡GAME OVER! Has sido derrotado."));
	}*/

	// OJO: Cancelamos la autodestrucción del cadáver. 
	// A los enemigos les pusimos SetLifeSpan(5.f) en la clase base, pero el cuerpo del jugador debe quedarse en el piso.
	SetLifeSpan(0.f); 
}

void AAuroPlayer::Interact()
{
	// If we already have a weapon, we can't pick up another one right now (or maybe we drop/throw it first).
	// In this design, Right Click does both: Pick Up if empty, Throw if equipped.
	if (EquippedWeapon)
	{
		ThrowEquippedWeapon();
		return;
	}

	// Try to find a weapon on the floor to pick up
	FVector Start = GetActorLocation();
	FCollisionShape Sphere = FCollisionShape::MakeSphere(150.f);
	
	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	
	if (GetWorld()->OverlapMultiByObjectType(OverlapResults, Start, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldDynamic), Sphere, QueryParams))
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			if (AAuraWeapon* FoundWeapon = Cast<AAuraWeapon>(Result.GetActor()))
			{
				FoundWeapon->PickUp(this);
				EquippedWeapon = FoundWeapon;
				break; // Pick up the first one we find
			}
		}
	}
}

void AAuroPlayer::ThrowEquippedWeapon()
{
	if (!EquippedWeapon) return;

	// Calculate direction towards the mouse cursor
	FVector ThrowDirection = GetActorForwardVector();
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FHitResult Hit;
		if (PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			ThrowDirection = (Hit.ImpactPoint - GetActorLocation());
			ThrowDirection.Z = 0.f; // Keep it horizontal
			ThrowDirection.Normalize();
		}
	}

	EquippedWeapon->ThrowWeapon(ThrowDirection);
	EquippedWeapon = nullptr;
}
