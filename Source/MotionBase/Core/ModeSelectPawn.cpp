#include "Core/ModeSelectPawn.h"
#include "MotionBase.h"
#include "Core/ModeManager.h"
#include "Core/MotionBaseGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AModeSelectPawn::AModeSelectPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	// 타자 시점과 비슷한 높이 — 모드를 고르고 바로 타격으로 넘어가도 시점이 튀지 않는다.
	Camera->SetRelativeLocation(FVector(-400.0f, 0.0f, 170.0f));
	Camera->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
}

void AModeSelectPawn::BeginPlay()
{
	Super::BeginPlay();

	MenuModes = UModeManager::GetMenuModes();

	// 커서는 플레이 가능한 모드 위에서 시작한다. 지금은 목록 5번째의 타격뿐이라
	// 0번(신체 인식)에 두면 "Enter 를 눌러도 아무 일도 안 나는" 첫인상이 된다.
	SelectedIndex = FindFirstImplementedIndex();
}

int32 AModeSelectPawn::FindFirstImplementedIndex() const
{
	for (int32 i = 0; i < MenuModes.Num(); ++i)
	{
		if (UModeManager::IsModeImplemented(MenuModes[i]))
		{
			return i;
		}
	}
	return 0;
}

EGameModeId AModeSelectPawn::GetSelectedMode() const
{
	return MenuModes.IsValidIndex(SelectedIndex) ? MenuModes[SelectedIndex] : EGameModeId::Batting;
}

void AModeSelectPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input 에셋 없이 키 직접 바인딩 (ASwingTestPawn 과 동일한 방식).
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AModeSelectPawn::SelectPrev);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &AModeSelectPawn::SelectPrev);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AModeSelectPawn::SelectNext);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &AModeSelectPawn::SelectNext);

	PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AModeSelectPawn::Confirm);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AModeSelectPawn::Confirm);
}

void AModeSelectPawn::MoveSelection(int32 Delta)
{
	if (MenuModes.Num() == 0)
	{
		return;
	}

	// 위/아래로 순환. 미구현 모드도 커서는 올라간다 — 목록에서 감추면
	// 전체 구성이 안 보여서 심사·시연 때 설명이 어렵다.
	SelectedIndex = (SelectedIndex + Delta + MenuModes.Num()) % MenuModes.Num();

	NoticeText.Reset();
	NoticeTimer = 0.0f;
}

void AModeSelectPawn::SelectPrev()
{
	MoveSelection(-1);
}

void AModeSelectPawn::SelectNext()
{
	MoveSelection(1);
}

void AModeSelectPawn::Confirm()
{
	const EGameModeId Mode = GetSelectedMode();

	if (!UModeManager::IsModeImplemented(Mode))
	{
		NoticeText = FString::Printf(TEXT("%s — 아직 준비 중인 모드입니다."),
			*UModeManager::GetModeDisplayName(Mode).ToString());
		NoticeTimer = NoticeDurationSec;
		return;
	}

	AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogMotionBase, Warning, TEXT("ModeSelect: AMotionBaseGameMode 를 찾지 못해 모드를 시작할 수 없습니다."));
		return;
	}

	GM->StartMode(Mode);
}

void AModeSelectPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (NoticeTimer > 0.0f)
	{
		NoticeTimer -= DeltaSeconds;
		if (NoticeTimer <= 0.0f)
		{
			NoticeText.Reset();
		}
	}
}
