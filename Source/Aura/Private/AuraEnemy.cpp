// Fill out your copyright notice in the Description page of Project Settings.


#include "AuraEnemy.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Aura/Aura.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Enemy/AIController/AuraAIController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DecalComponent.h"
#include "Actor/AuraWeapon.h"

AAuraEnemy::AAuraEnemy()
{
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	AbilitySystemComponent = CreateDefaultSubobject<UAuraAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	
	AttributeSet = CreateDefaultSubobject<UAuraAttributeSet>("AttributeSet");
	
	// Le decimos que siempre tome su cerebro, ya sea puesto a mano o por Spawner
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AAuraEnemy::HighlightActor()
{
	GetMesh()->SetRenderCustomDepth(true);
	GetMesh()->SetCustomDepthStencilValue(CUSTOM_DEPTH_RED);
	Weapon->SetRenderCustomDepth(true);
	Weapon->SetCustomDepthStencilValue(CUSTOM_DEPTH_RED);
}

void AAuraEnemy::UnHighlightActor()
{
	GetMesh()->SetRenderCustomDepth(false);
	Weapon->SetRenderCustomDepth(false);
}

void AAuraEnemy::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	// Asegúrate de tener el include arriba: #include "AbilitySystemComponent.h"

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent()) // O tu variable del ASC
	{
		// Recorremos la lista y le damos cada habilidad
		for (TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
		{
			if (AbilityClass)
			{
				FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
				ASC->GiveAbility(AbilitySpec);
			}
		}
	}

	// Spawnear y equipar arma por defecto
	if (DefaultWeaponClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		EquippedWeapon = GetWorld()->SpawnActor<AAuraWeapon>(DefaultWeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);
		if (EquippedWeapon)
		{
			EquippedWeapon->PickUp(this);
		}
	}
}

void AAuraEnemy::FireRangedAttack()
{
	// Buscamos el sistema de habilidades del enemigo
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(this))
	{
		// Le decimos que active la habilidad con la etiqueta del disparo
		// (Asegúrate de que este tag sea exactamente el mismo que pusiste en tu GA_HechiceroDisparo)
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.RangedDamage")));
        
		ASC->TryActivateAbilitiesByTag(TagContainer);
	}
}

void AAuraEnemy::FireMeleeAttack()
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(this))
	{
		FGameplayTagContainer TagContainer;
		// Esta etiqueta es exclusiva para los golpes físicos
		TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.MeleeAttack")));
        
		ASC->TryActivateAbilitiesByTag(TagContainer);
	}
}

#include "Camera/CameraComponent.h"
#include "FCTween.h"

void AAuraEnemy::Die()
{
	DropWeapon(); // Hotline Miami: Soltar arma al morir
	Super::Die();

	// Ejecutar Screen Shake mediante código usando Fresh Baked Tweens (FCTween)
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			if (UCameraComponent* CameraComp = PlayerPawn->FindComponentByClass<UCameraComponent>())
			{
				FVector OriginalLoc = CameraComp->GetRelativeLocation();
				
				// Hacemos un tween de 0 a 1 durante 0.35 segundos
				FCTweenInstanceFloat* Tween = FCTween::Play(0.0f, 1.0f, [CameraComp, OriginalLoc](float t)
				{
					if (IsValid(CameraComp))
					{
						// (1.0 - t) asegura que el temblor pierda fuerza con el tiempo
						float Intensity = (1.0f - t) * 30.0f;
						FVector RandomOffset = FVector(
							FMath::RandRange(-Intensity, Intensity),
							FMath::RandRange(-Intensity, Intensity),
							FMath::RandRange(-Intensity, Intensity)
						);
						CameraComp->SetRelativeLocation(OriginalLoc + RandomOffset);
					}
				}, 0.35f, EFCEase::OutQuad);

				// Cuando el tween termine, regresamos la cámara a su posición original intacta
				Tween->SetOnComplete([CameraComp, OriginalLoc]()
				{
					if (IsValid(CameraComp))
					{
						CameraComp->SetRelativeLocation(OriginalLoc);
					}
				});
			}
		}
	}

	PlayDeathShake(); // Aún mantenemos el evento de Blueprint por si acaso quieres agregar más cosas

	// Hotline Miami: Spawnear sangre permanente en el suelo
	if (BloodDecalMaterial)
	{
		UGameplayStatics::SpawnDecalAtLocation(
			GetWorld(),
			BloodDecalMaterial,
			FVector(200.f, 200.f, 200.f), // Tamaño
			GetActorLocation() - FVector(0.f, 0.f, 90.f), // Suelo
			FRotator(-90.f, FMath::RandRange(0.f, 360.f), 0.f), // Hacia abajo con rotación aleatoria
			0.f // Duración de 0 para que sea permanente en el nivel
		);
	}

	// Cancelamos la autodestrucción (SetLifeSpan). El cadáver permanecerá en escena.
	SetLifeSpan(0.0f);
}

void AAuraEnemy::ApplyKnockdown(float Duration, FVector LaunchForce)
{
	if (bIsKnockedDown) return;
	bIsKnockedDown = true;

	DropWeapon(); // Hotline Miami: Soltar arma al ser derribado

	// Detener movimiento y guardar el controlador
	SavedController = GetController();
	if (SavedController)
	{
		SavedController->UnPossess();
	}

	// Activar simulación de físicas en el Mesh (Ragdoll temporal)
	GetMesh()->SetAllBodiesSimulatePhysics(true);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetEnableGravity(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	// Desactivar colisión de cápsula para que no se quede flotando
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Aplicar fuerza de empuje por el lanzamiento
	GetMesh()->AddImpulse(LaunchForce, NAME_None, true);

	// Configurar temporizador para la recuperación
	GetWorldTimerManager().SetTimer(KnockdownRecoveryTimerHandle, this, &AAuraEnemy::RecoverFromKnockdown, Duration, false);
}

void AAuraEnemy::RecoverFromKnockdown()
{
	if (!bIsKnockedDown) return;
	bIsKnockedDown = false;

	// Desactivar simulación de físicas del Mesh
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetAllBodiesSimulatePhysics(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// Re-activar colisión de la cápsula
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Posicionar la cápsula donde quedó tirado el cuerpo
	FVector MeshLocation = GetMesh()->GetComponentLocation();
	SetActorLocation(MeshLocation + FVector(0.f, 0.f, 90.f)); // Altura por defecto

	// Re-acoplar el Mesh a la cápsula
	GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f)); 
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); 

	// Re-poseer con el controlador guardado
	if (SavedController)
	{
		SavedController->Possess(this);
	}
	else
	{
		SpawnDefaultController();
	}
}

void AAuraEnemy::DropWeapon()
{
	if (!EquippedWeapon) return;

	// Desacoplar el arma de forma que mantenga su posición en el mundo
	EquippedWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	
	// Habilitar simulación física en la malla del arma (que es su root component)
	if (USkeletalMeshComponent* WeaponMesh = Cast<USkeletalMeshComponent>(EquippedWeapon->GetRootComponent()))
	{
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		WeaponMesh->SetCollisionResponseToAllChannels(ECR_Block);
		WeaponMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Permitir que el player la pise/agarre
		WeaponMesh->SetSimulatePhysics(true);
		
		// Aplicar un pequeño impulso físico aleatorio para simular que sale volando al caer
		FVector RandomImpulse = FVector(FMath::RandRange(-300.f, 300.f), FMath::RandRange(-300.f, 300.f), 200.f);
		WeaponMesh->AddImpulse(RandomImpulse, NAME_None, true);
	}
	
	// Desvincular propiedad para que no se considere equipada
	EquippedWeapon = nullptr;
}