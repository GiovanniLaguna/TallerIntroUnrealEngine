// Fill out your copyright notice in the Description page of Project Settings.


#include "AuroPlayer.h"

#include "GameFramework/CharacterMovementComponent.h"

AAuroPlayer::AAuroPlayer()
{
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;
	
	bUseControllerRotationPitch = true;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
}
