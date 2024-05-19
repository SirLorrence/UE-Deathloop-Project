// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter.h"

#include "CollisionDebugDrawingPublic.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "KismetTraceUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "bitset"
#include "Kismet/GameplayStatics.h"


// Sets default values
APlayerCharacter::APlayerCharacter() {
  // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;

  m_cameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Player Cam"));
  m_cameraComponent->SetupAttachment(GetCapsuleComponent());
  m_cameraComponent->SetRelativeLocation(FVector(0, 0, BaseEyeHeight));
  m_cameraComponent->bUsePawnControlRotation = true;

  VFX = nullptr;
  m_testBool = false;
}

// Called when the game starts or when spawned
void APlayerCharacter::BeginPlay() {
  Super::BeginPlay();
  check(GEngine != nullptr);
  GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString("Using Custom Player"));

  m_playerController = Cast<APlayerController>(GetController());
  // Setup player input subsystem
  if (const ULocalPlayer* localPlayer = Cast<ULocalPlayer>(m_playerController->GetLocalPlayer())) {
    if (UEnhancedInputLocalPlayerSubsystem* InputSystem =
      localPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) {
      if (InputMapping != nullptr) {
        InputSystem->AddMappingContext(InputMapping, 0);
      }
    }
  }
  m_characterMovementComponent = GetCharacterMovement();
  m_characterMovementComponent->MaxWalkSpeed = m_movementMeterPerSec * 100;
  m_baseSpeed = m_characterMovementComponent->MaxWalkSpeed;
  // GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
  //                               FString::Printf(
  //                                 TEXT("Movement State: %f"), m_baseSpeed));
  m_characterMovementComponent->MaxWalkSpeedCrouched = m_baseSpeed / 1.75;
  // magic number but i think this would be more flexible

  m_maxSprintSpeed = m_baseSpeed + (m_baseSpeed * m_speedMultiplier / 100);

  m_capsuleComponent = GetCapsuleComponent();

  // cached values
  m_cachedStandingHeight = m_capsuleComponent->GetScaledCapsuleHalfHeight();
  m_cacheFOV = m_cameraComponent->FieldOfView;
}


// Called every frame
void APlayerCharacter::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Blue,
                                   FString::Printf(
                                     TEXT("Movement State: %hhd"), m_movementState));
  GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Blue,
                                   FString::Printf(
                                     TEXT("Sprinting: %s"), *UKismetStringLibrary::Conv_BoolToString(m_isSprinting)));
  GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Blue,
                                   FString::Printf(
                                     TEXT("Crouching: %s"), *UKismetStringLibrary::Conv_BoolToString(m_isCrouching)));
  GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Blue,
                                   FString::Printf(
                                     TEXT("Sliding: %s"), *UKismetStringLibrary::Conv_BoolToString(m_isSliding)));
  GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Blue,
                                   FString::Printf(
                                     TEXT("Wants 2 Slide: %f"),
                                     m_capsuleComponent->GetScaledCapsuleHalfHeight()));
  GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Blue,
                                   FString::Printf(
                                     TEXT("Wants 2 Crouch: %s"),
                                     *UKismetStringLibrary::Conv_BoolToString(m_wantsToCrouch)));

  if(m_coolDownTimer < m_coolDownTimeRecharge && !m_shiftToLocation) {
    m_coolDownTimer += DeltaTime; 
  } else {
    RechargeMana(DeltaTime);
  }
  
  HandleSpeed();
  HandleCrouch(); // could go into another thread?
  // Lerp Movement + Camera Movement
  if(m_shiftToLocation) {
    m_elapsedTime += GetWorld()->DeltaTimeSeconds;
    m_cameraComponent->SetFieldOfView(FMath::Lerp( m_cacheFOV,170, m_elapsedTime/(m_desiredTime * 2)));
    SetActorLocation(FMath::Lerp(m_cacheLocation, m_shiftLocation, m_elapsedTime/m_desiredTime));
    if(m_elapsedTime >= m_desiredTime) {
      m_shiftToLocation = false;
      m_elapsedTime = 0;
      m_fovOffset = m_cameraComponent->FieldOfView;
    }
  }
  // Lerp Camera Return Movement
  if(!m_shiftToLocation && m_cameraComponent->FieldOfView > m_cacheFOV)  {
    m_elapsedTime += GetWorld()->DeltaTimeSeconds;
    m_cameraComponent->SetFieldOfView(FMath::Lerp( m_fovOffset,m_cacheFOV, m_elapsedTime/(m_desiredTime * 2)));
    if(m_elapsedTime >= (m_desiredTime * 2)) {
      m_elapsedTime = 0;
      m_cameraComponent->SetFieldOfView(m_cacheFOV);
    }
  }
}

// Called to bind functionality to input
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
  UEnhancedInputComponent* inputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);

  inputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Movement);
  inputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::LookMovement);
  inputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Jump);

  inputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Sprint);
  inputComponent->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &APlayerCharacter::SetCrouch);

  inputComponent->BindAction(AbilityAction, ETriggerEvent::Triggered, this, &APlayerCharacter::StartAbility);
  inputComponent->BindAction(AbilityAction, ETriggerEvent::Completed, this, &APlayerCharacter::ExecuteAbility);

  inputComponent->BindAction(ThrustAction, ETriggerEvent::Started, this, &APlayerCharacter::Thrust);
}

void APlayerCharacter::Movement(const FInputActionValue& value) {
  FVector2d moveVec2d = value.Get<FVector2d>();
  moveVec2d.Normalize();
  if (Controller != nullptr) {
    AddMovementInput(GetActorForwardVector(), moveVec2d.Y);
    AddMovementInput(GetActorRightVector(), moveVec2d.X);
  }
}

void APlayerCharacter::LookMovement(const FInputActionValue& value) {
  FVector2d lookVec2d = value.Get<FVector2d>();
  lookVec2d *= (m_mouseSensitivity / 10);
  if (Controller != nullptr) {
    AddControllerYawInput(lookVec2d.X);
    AddControllerPitchInput(-lookVec2d.Y);
  }
}

void APlayerCharacter::Sprint(const FInputActionValue& value) {
  // m_characterMovementComponent->MaxWalkSpeed = m_baseSpeed + (m_baseSpeed * m_speedMultiplier / 100);
  // This solves the toggle action either using Started and Completed Triggers 
  // or in IMC using the pressed and released Triggers
  if (m_sprintToggle && value.Get<bool>()) {
    m_isSprinting = !m_isSprinting;
  }
  else if (!m_sprintToggle) {
    m_isSprinting = value.Get<bool>();
  }
}

void APlayerCharacter::SetCrouch(const FInputActionValue& value) {
  // m_characterMovementComponent->MaxWalkSpeed = m_baseSpeed - ((m_baseSpeed * .01) / 2) * ((m_baseSpeed / 100) *
  //   m_speedMultiplier);
  if (m_crouchToggle && value.Get<bool>()) {
    m_wantsToCrouch = !m_wantsToCrouch;
  }
  else if (!m_crouchToggle) {
    m_wantsToCrouch = value.Get<bool>();
  }
}

void APlayerCharacter::HandleCrouch() {
  const float currentHeight = m_capsuleComponent->GetScaledCapsuleHalfHeight();
  float targetHeight = (m_wantsToCrouch)
                         ? m_characterMovementComponent->GetCrouchedHalfHeight()
                         : m_cachedStandingHeight;
  float crouchSpeedModifier = 1.0;

  if (m_isSprinting && !m_isCrouching && m_wantsToCrouch) {
    StartSlide();
    return;
  }

  if (m_slideOverride) {
    crouchSpeedModifier = 2;
    targetHeight -= 10;
  }

  GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Red, FString::Printf(TEXT("%f"), crouchSpeedModifier));

  float heightValue =
    UKismetMathLibrary::Lerp(currentHeight, targetHeight,
                             GetWorld()->DeltaTimeSeconds * (m_crouchSmoothValue * crouchSpeedModifier));
  if (UKismetMathLibrary::Abs(heightValue - targetHeight) < 0.1f) {
    if (m_slideOverride) {
      m_slideOverride = false;
      return;
    }
    heightValue = targetHeight;
    m_isCrouching = m_wantsToCrouch;
  }
  m_capsuleComponent->SetCapsuleHalfHeight(heightValue);
}

void APlayerCharacter::HandleSpeed() {
  if (!m_isSliding) {
    if (m_isSprinting) {
      if (m_isCrouching) m_wantsToCrouch = false;
      m_movementState = EMovementState::kRunning;
    }
    else if (m_wantsToCrouch) {
      m_movementState = EMovementState::kCrouching;
    }
    else {
      m_movementState = EMovementState::kWalking;
    }
  }

  switch (m_movementState) {
  case EMovementState::kWalking:
    m_characterMovementComponent->MaxWalkSpeed = m_baseSpeed;
    break;
  case EMovementState::kRunning: m_characterMovementComponent->MaxWalkSpeed = m_maxSprintSpeed;
    break;
  case EMovementState::kCrouching:
    m_characterMovementComponent->MaxWalkSpeed = m_baseSpeed / 1.75;
    break;
  case EMovementState::kSliding:
    m_characterMovementComponent->MaxWalkSpeed = GetVelocity().Length() - (m_slideTime * GetWorld()->DeltaTimeSeconds);
    if (m_characterMovementComponent->Velocity.Length() < m_baseSpeed / 1.75) {
      EndSlide();
    }
    break;
  }
}

void APlayerCharacter::RechargeMana(float t) {
  if(m_abilityMana <= 100)
  m_abilityMana += t * m_rechargeRate;
}


void APlayerCharacter::StartSlide() {
  if (m_isSliding) return;
  m_isSliding = true;
  m_isSprinting = false;


  FVector currentVelocity = GetVelocity();
  currentVelocity.Z = 0;
  FVector desiredDirectionVec = currentVelocity;
  desiredDirectionVec += GetActorForwardVector() * (m_slideBoost * 100);;
  m_characterMovementComponent->Velocity = desiredDirectionVec;
  m_slideOverride = true;
  m_movementState = EMovementState::kSliding;
}

void APlayerCharacter::EndSlide() {
  m_isSliding = false;
  m_movementState = EMovementState::kCrouching;
}


void APlayerCharacter::Jump() {
  if (m_characterMovementComponent->IsCrouching() || m_isCrouching) {
    m_isCrouching = false;
    m_characterMovementComponent->bWantsToCrouch = false;
  }
  Super::Jump();
}

void APlayerCharacter::Thrust() {

  m_testBool = true;
  m_shiftLocation = m_cameraComponent->GetComponentLocation() + m_cameraComponent->GetForwardVector() * 400;
  m_cacheLocation = GetActorLocation();
  
  // const FVector desiredForceVelocity = GetActorForwardVector() * (m_slideBoost * 100);
  // LaunchCharacter(desiredForceVelocity, false, false);
  // m_isSliding = true;
  // m_movementState = EMovementState::kSliding;
}
void APlayerCharacter::StartAbility() {
  if(m_coolDownTimer < m_coolDownTimeAbility) {
    m_canShift = false;
    return;
  }
  // TODO: Move ability to its only class/interface
  // SHIFT Ability
  FVector start = m_cameraComponent->GetComponentLocation();
  FVector end = start + m_cameraComponent->GetForwardVector() * 800;
  FVector endLocation;
  DrawDebugLine(GetWorld(), start, end, FColor::Red);
  FHitResult hitResult;
  TArray<AActor*> actorResults;
  TArray<FOverlapResult> results;
  TArray<TEnumAsByte<EObjectTypeQuery>> objectQuery;
  bool overrideLocation = false; // quick hack

  // VFX
  if(VFX == nullptr) {
    VFX = GetWorld()->SpawnActor(ShiftVFX);
  }

  
  // TODO: Create preprocessor directive 
  // to store multiple bools in on byte to save some memory
  std::bitset<8> flagChecks{0b0000'0000};

  FHitResult OutHitResult;
  objectQuery.Add(UEngineTypes::ConvertToObjectType(ECC_Visibility));

  flagChecks.set(0, GetWorld()->LineTraceSingleByChannel(hitResult, start, end, ECC_Visibility));

  // first contact with something
  if (flagChecks[0]) {
    bool isSurfaceNormalZ = (FMath::Abs(hitResult.Normal.X) < FLT_EPSILON && FMath::Abs(hitResult.Normal.Y) <
      FLT_EPSILON);

    DrawDebugSphere(GetWorld(), hitResult.Location, 10, 10, FColor::Yellow);

    // handling the wall/climbable check
    FHitResult surfaceResult;
    float scaleWallThreshold = m_capsuleComponent->GetUnscaledCapsuleHalfHeight() / 2;
    FVector lineStart = hitResult.ImpactPoint + (GetActorForwardVector() * m_capsuleComponent->
      GetUnscaledCapsuleRadius());
    FVector lineEnd = lineStart + FVector::UpVector * scaleWallThreshold;
    // TODO: need to convert this to a reusable bool
    if (!isSurfaceNormalZ && UKismetSystemLibrary::LineTraceSingle(GetWorld(), lineEnd, lineStart,
                                                                   UEngineTypes::ConvertToTraceType(ECC_Visibility),
                                                                   false, actorResults,
                                                                   EDrawDebugTrace::ForOneFrame, surfaceResult,
                                                                   false)) {
      // DrawDebugSphere(
      //   GetWorld(), surfaceResult.Location + FVector::UpVector * m_capsuleComponent->GetUnscaledCapsuleHalfHeight(), 20,
      //   20, FColor::Green);
      FVector sphereStart = surfaceResult.Location + FVector::UpVector * m_capsuleComponent->
        GetUnscaledCapsuleHalfHeight();
      FVector sphereEnd = surfaceResult.Location + FVector::UpVector * m_capsuleComponent->
        GetUnscaledCapsuleHalfHeight() * 2;


      // Checks if clipping with object from mental location
      flagChecks.set(1, UKismetSystemLibrary::SphereTraceSingle(GetWorld(), sphereStart, sphereEnd,
                                                                m_capsuleComponent->GetUnscaledCapsuleRadius(),
                                                                UEngineTypes::ConvertToTraceType(ECC_Visibility),
                                                                false,
                                                                actorResults,
                                                                EDrawDebugTrace::ForOneFrame, OutHitResult, true));

      if (!flagChecks[1]) {
        endLocation = sphereStart;
        overrideLocation = true;
      }
    }
    // ----


    // end = hitResult.Location + (hitResult.Normal * m_capsuleComponent->GetUnscaledCapsuleHalfHeight()/((mod)? 1 : 2));
    GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Red,
                                     FString::Printf(
                                       TEXT("Normal - X: %f, Y: %f, Z: %f"), hitResult.Normal.X, hitResult.Normal.Y,
                                       hitResult.Normal.Z));


    FVector capsulePositionOffset = hitResult.Location + (hitResult.Normal * m_capsuleComponent->
      GetUnscaledCapsuleHalfHeight() / (
        (isSurfaceNormalZ) ? 1 : 2) + FVector::UpVector);


    // location sweep
    flagChecks.set(2, UKismetSystemLibrary::CapsuleTraceSingle(
                     GetWorld(),
                     capsulePositionOffset,
                     capsulePositionOffset, m_capsuleComponent->GetUnscaledCapsuleRadius(),
                     m_capsuleComponent->GetUnscaledCapsuleHalfHeight(),
                     ETraceTypeQuery::TraceTypeQuery1, false, actorResults,
                     EDrawDebugTrace::ForOneFrame, hitResult, true));

    // mainly checks if contact with the wall and the capsule clips with the floor, is it still valid by check
    // the area above.
    if (flagChecks[2]) {
      // UE_LOG(LogTemp,Warning,TEXT("Clipping"));
      flagChecks.set(3, UKismetSystemLibrary::SphereTraceSingle(
                       GetWorld(), capsulePositionOffset + (FVector::UpVector * m_capsuleComponent->
                         GetUnscaledCapsuleHalfHeight() * .25f),
                       capsulePositionOffset + (FVector::UpVector * m_capsuleComponent->GetUnscaledCapsuleHalfHeight() *
                         1.75f),
                       m_capsuleComponent->GetUnscaledCapsuleRadius(),
                       UEngineTypes::ConvertToTraceType(ECC_Visibility),
                       false,
                       actorResults,
                       EDrawDebugTrace::ForOneFrame, OutHitResult, true));
    }

    // between the player and the desired location sweep
    if (flagChecks[2] && (flagChecks[1] || flagChecks[3])) {
      UKismetSystemLibrary::CapsuleTraceSingle(GetWorld(),
                                               start,
                                               end,
                                               m_capsuleComponent->GetUnscaledCapsuleRadius(),
                                               m_capsuleComponent->GetUnscaledCapsuleHalfHeight(),
                                               ETraceTypeQuery::TraceTypeQuery1, false, actorResults,
                                               EDrawDebugTrace::ForOneFrame, hitResult, true);
      overrideLocation = true;
      endLocation = hitResult.Location;

      
    }
    if (!overrideLocation) {
      endLocation = capsulePositionOffset;
    }
  }

  else {
    DrawDebugSphere(GetWorld(), end, 10, 10, FColor::Red);
    // DrawCapsuleOverlap(GetWorld(), end, m_capsuleComponent->GetUnscaledCapsuleHalfHeight(),
    //                    m_capsuleComponent->GetUnscaledCapsuleRadius(), FQuat::Identity, results, 0);

    flagChecks.set(1, UKismetSystemLibrary::CapsuleTraceSingle(GetWorld(), end, end,
                                                               m_capsuleComponent->GetUnscaledCapsuleRadius(),
                                                               m_capsuleComponent->GetUnscaledCapsuleHalfHeight(),
                                                               ETraceTypeQuery::TraceTypeQuery1, false, actorResults,
                                                               EDrawDebugTrace::ForOneFrame, hitResult, true));

    if (flagChecks[1]) {
      flagChecks.set(2, UKismetSystemLibrary::SphereTraceSingle(
                       GetWorld(), end + (FVector::UpVector * m_capsuleComponent->GetUnscaledCapsuleHalfHeight() * .5),
                       end + (FVector::UpVector * m_capsuleComponent->GetUnscaledCapsuleHalfHeight() * 2),
                       m_capsuleComponent->GetUnscaledCapsuleRadius(),
                       UEngineTypes::ConvertToTraceType(ECC_Visibility),
                       false,
                       actorResults,
                       EDrawDebugTrace::ForOneFrame, OutHitResult, true));

      if (flagChecks[2]) {
        flagChecks.set(3, UKismetSystemLibrary::CapsuleTraceSingle(GetWorld(),
                                                                   start,
                                                                   end,
                                                                   m_capsuleComponent->GetUnscaledCapsuleRadius(),
                                                                   m_capsuleComponent->GetUnscaledCapsuleHalfHeight(),
                                                                   ETraceTypeQuery::TraceTypeQuery1, false,
                                                                   actorResults,
                                                                   EDrawDebugTrace::ForOneFrame, hitResult, true));
        end = hitResult.Location; // this still works cause order of operation
      }
    }
    endLocation = end;
  }
  
  if (flagChecks.all()) {
    //UE_LOG(LogTemp, Error, TEXT("No valid location")); // AKA the last valid option
    m_canShift = false; // for now
  }
  else {
    m_canShift = true;
    m_shiftLocation = endLocation;
    DrawDebugSphere(GetWorld(), m_shiftLocation, m_capsuleComponent->GetUnscaledCapsuleRadius(), 20, FColor::Emerald);
  }


 
}

void APlayerCharacter::ExecuteAbility() {
  if(VFX != nullptr) {
    VFX->Destroy();
    VFX = nullptr;
  }
  m_testBool = true;
  m_cacheLocation = GetActorLocation();
  if (m_canShift) {
    if(m_abilityMana - m_abilityCost  < 0) {
      return;
    }
    
    m_shiftToLocation = true;
    m_abilityMana -= m_abilityCost;
    // SetActorLocation(m_shiftLocation);
    m_canShift = false;
    m_coolDownTimer = 0;
  }
}
