#include "Core/Defense/DefensePawn.h"
#include "MotionBase.h"
#include "Core/MotionBaseGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ADefensePawn::ADefensePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->SetRelativeLocation(FVector(-400.0f, 0.0f, 170.0f));
	Camera->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
}

void ADefensePawn::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogMotionBase, Log, TEXT("DefensePawn: 수비 훈련 진입"));
}

void ADefensePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Esc 는 PIE 종료라 못 씀 → M 으로 모드 선택 복귀 (SwingTestPawn 과 동일).
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ADefensePawn::ReturnToModeSelect);
}

void ADefensePawn::ReturnToModeSelect()
{
	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->ReturnToModeSelect();
	}
}

void ADefensePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 임시 진입 확인용 화면 텍스트. (정식 HUD 는 이후 단계)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Green,
			TEXT("수비 훈련 (개발 중)   |   M: 모드 선택으로"));
	}
}