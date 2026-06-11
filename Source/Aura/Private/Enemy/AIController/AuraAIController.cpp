#include "Enemy/AIController/AuraAIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "AuraEnemy.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

AAuraAIController::AAuraAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));

	if (AIPerceptionComponent && SightConfig && HearingConfig)
	{
		// Configurar Vista (Sight)
		SightConfig->SightRadius = 1500.f;
		SightConfig->LoseSightRadius = 1700.f;
		SightConfig->PeripheralVisionAngleDegrees = 45.f; // 45 a cada lado = 90 grados total
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		
		AIPerceptionComponent->ConfigureSense(*SightConfig);

		// Configurar Oído (Hearing)
		HearingConfig->HearingRange = 3000.f;
		HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
		HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
		HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

		AIPerceptionComponent->ConfigureSense(*HearingConfig);
		AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

		// Enlazar evento de actualización
		AIPerceptionComponent->OnPerceptionUpdated.AddDynamic(this, &AAuraAIController::OnPerceptionUpdated);
	}
}

void AAuraAIController::BeginPlay()
{
	Super::BeginPlay();
}

void AAuraAIController::OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
	for (AActor* Actor : UpdatedActors)
	{
		// Verificamos si realmente es el jugador controlado por humanos
		if (Actor == UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			FActorPerceptionBlueprintInfo Info;
			AIPerceptionComponent->GetActorsPerception(Actor, Info);

			for (const FAIStimulus& Stimulus : Info.LastSensedStimuli)
			{
				// Estímulo visual (Vista)
				if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
				{
					if (Stimulus.WasSuccessfullySensed())
					{
						bIsAlerted = true;
						LastKnownTargetLocation = Actor->GetActorLocation();
					}
				}
				// Estímulo auditivo (Oído - Ej. disparo)
				else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
				{
					if (Stimulus.WasSuccessfullySensed())
					{
						LastKnownTargetLocation = Stimulus.StimulusLocation;
						bIsAlerted = true; // Hotline Miami: Cualquier disparo alerta de inmediato
					}
				}
			}
		}
	}
}

void AAuraAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AAuraEnemy* ControlledEnemy = Cast<AAuraEnemy>(GetPawn());
	if (!ControlledEnemy) return;

	// Si está derribado, detenemos la IA y su movimiento
	if (ControlledEnemy->bIsKnockedDown)
	{
		StopMovement();
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn && bIsAlerted)
	{
		// Moverse hacia el objetivo o última localización conocida del ruido
		MoveToLocation(LastKnownTargetLocation, 5.f);

		float DistanceToPlayer = FVector::Dist(ControlledEnemy->GetActorLocation(), PlayerPawn->GetActorLocation());
			
		if (DistanceToPlayer <= ControlledEnemy->AttackRadius + 50.f)
		{
			// El Hechicero usará esta, el Esbirro la ignorará
			ControlledEnemy->FireRangedAttack(); 
    
			// El Esbirro usará esta, el Hechicero la ignorará
			ControlledEnemy->FireMeleeAttack(); 
		}
	}
}