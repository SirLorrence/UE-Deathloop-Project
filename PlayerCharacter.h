// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "ShiftAbilityComponent.h"
#include "PlayerCharacter.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class FPS_CONTROLLER_API APlayerCharacter : public ACharacter {
  GENERATED_BODY()

public:
  // Sets default values for this character's properties
  APlayerCharacter();
  // Called every frame
  virtual void Tick(float DeltaTime) override;
  // Called to bind functionality to input
  virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;



  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ShiftAB")
  float m_abilityMana = 100;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShiftAB")
  FVector m_shiftLocation;

  

protected:
  // Called when the game starts or when spawned
  virtual void BeginPlay() override;



private:
  APlayerController* m_playerController;
  UCharacterMovementComponent* m_characterMovementComponent;
  UCapsuleComponent* m_capsuleComponent;

  UPROPERTY(EditDefaultsOnly)
  UCameraComponent* m_cameraComponent;

  UPROPERTY(EditAnywhere)
  UShiftAbilityComponent* m_shiftAbility;




  // Movement Functions
  void Movement(const FInputActionValue& value);
  void LookMovement(const FInputActionValue& value);
  void Sprint(const FInputActionValue& value);
  void SetCrouch(const FInputActionValue& value);
  void HandleCrouch();
  void HandleSpeed();
  void RechargeMana(float t);
  // void SmoothCrouchHandler(float targetHeight);
  void StartSlide();
  void EndSlide();
  virtual void Jump() override;
  void Thrust();

  // Ability Interaction
  void StartAbility();
  void ExecuteAbility();
  UPROPERTY(EditAnywhere)
  TSubclassOf<class UObject> ShiftVFX;

  AActor* VFX;
  
  // TODO: move to ability interface/class
  // UPROPERTY(EditDefaultsOnly,Blueprintable, Category="ShiftAB")
  // FVector m_shiftLocation;
  
  bool m_canShift;
  bool m_testBool;
  bool m_shiftToLocation;
  FVector m_cacheLocation;
  float m_fovOffset;
  float m_cacheFOV;
  float m_elapsedTime;
  float m_desiredTime = .25f;
  float m_abilityCost = 25.f;
  float m_rechargeRate = 10.f;

  float m_coolDownTimer;
  float m_coolDownTimeAbility = .75f;
  float m_coolDownTimeRecharge = 4.f;
  


  // Movement States
  enum class EMovementState : int8 { kWalking, kRunning, kCrouching, kSliding };

  EMovementState m_movementState;


  UPROPERTY(EditAnywhere, Category="Input")
  float m_mouseSensitivity;

  UPROPERTY(EditAnywhere, Category="Player Params")
  float m_movementMeterPerSec;
  UPROPERTY(EditAnywhere, Category="Player Params")
  float m_speedMultiplier;
  UPROPERTY(EditAnywhere, Category="Player Params")
  float m_crouchSmoothValue;
  UPROPERTY(EditAnywhere, Category="Player Params")
  float m_slideBoost;
  UPROPERTY(EditAnywhere, Category="Player Params")
  float m_slideTime; 
  UPROPERTY(EditAnywhere, Category="Player Params")
  bool m_sprintToggle;
  UPROPERTY(EditAnywhere, Category="Player Params")
  bool m_crouchToggle;



  // bool state
  bool m_isSprinting;
  bool m_isCrouching;
  bool m_isSliding;

  // bool allowed
  // bool m_wantsToSlide;
  bool m_wantsToCrouch;
  
  
  bool m_slideOverride;
  

  float m_baseSpeed;
  float m_maxSprintSpeed;
  float m_currentMultiplier;

  float m_boostSpeed;

  float m_cachedStandingHeight;

  // Enhanced Input 
  UPROPERTY(EditDefaultsOnly, Category="Input")
  UInputMappingContext* InputMapping;
  UPROPERTY(EditDefaultsOnly, Category="Input")
  UInputAction* MoveAction;
  UPROPERTY(EditDefaultsOnly, Category="Input")
  UInputAction* LookAction;
  UPROPERTY(EditDefaultsOnly, Category="Input")
  UInputAction* JumpAction;
  UPROPERTY(EditDefaultsOnly, Category="Input")
  UInputAction* SprintAction;
  UPROPERTY(EditDefaultsOnly, Category="Input")
  UInputAction* CrouchAction;
  UPROPERTY(EditDefaultsOnly, Category="Input")
  UInputAction* ThrustAction;
  UPROPERTY(EditDefaultsOnly, Category="Input")
  UInputAction* AbilityAction;
};
