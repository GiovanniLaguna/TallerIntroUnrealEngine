// Copyrigh Quetz-Lab 2026 


#include "AbilitySystem/AuraAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "AuraCharacterBase.h"
#include "Enemy/Spawner/AuraEnemySpawner.h"
#include "Net/UnrealNetwork.h"

UAuraAttributeSet::UAuraAttributeSet()
{
	InitHealth(1.f);
	InitMana(0.f);
	InitMaxHealth(1.f);
	InitMaxMana(0.f);
}

void UAuraAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
}

void UAuraAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet, Health, OldHealth);
}
void UAuraAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet, MaxHealth, OldMaxHealth);
}

void UAuraAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet, Mana, OldMana);
}

void UAuraAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet, MaxMana, OldMaxMana);
}

void UAuraAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Si el atributo que cambió fue la Vida...
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Aseguramos que la vida no baje de 0 ni suba del máximo
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

		// Si la vida llegó a cero, disparamos la muerte
		if (GetHealth() <= 0.0f)
		{
			// Obtenemos al dueño de estos atributos y llamamos a su función Die()
			if (AAuraCharacterBase* AuraChar = Cast<AAuraCharacterBase>(GetOwningActor()))
			{
				AuraChar->Die();
			}
		}
	}
	// ... dentro de PostGameplayEffectExecute ...
	if (GetHealth() <= 0.f)
	{
		if (AAuraCharacterBase* AuraChar = Cast<AAuraCharacterBase>(Data.Target.GetAvatarActor()))
		{
			AuraChar->Die(); //
		}
		else if (AAuraEnemySpawner* Spawner = Cast<AAuraEnemySpawner>(Data.Target.GetAvatarActor()))
		{
			Spawner->Die(); // Llamamos a la muerte del Spawner
		}
	}
}