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
	MenuDifficulties = UModeManager::GetMenuDifficulties();
	MenuStances = UModeManager::GetMenuStances();

	Stage = EStage::Mode;
	// 커서는 플레이 가능한 모드 위에서 시작한다 (0번은 미구현이라 첫인상이 나쁘다).
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

EGameModeId AModeSelectPawn::ModeAt(int32 Index) const
{
	return MenuModes.IsValidIndex(Index) ? MenuModes[Index] : EGameModeId::Batting;
}

EDifficultyLevel AModeSelectPawn::DifficultyAt(int32 Index) const
{
	return MenuDifficulties.IsValidIndex(Index) ? MenuDifficulties[Index] : EDifficultyLevel::Amateur;
}

EBattingStance AModeSelectPawn::StanceAt(int32 Index) const
{
	return MenuStances.IsValidIndex(Index) ? MenuStances[Index] : EBattingStance::Right;
}

// ── HUD 용 일반 행 데이터 ──

int32 AModeSelectPawn::GetRowCount() const
{
	switch (Stage)
	{
	case EStage::Mode:       return MenuModes.Num();
	case EStage::Difficulty: return MenuDifficulties.Num();
	case EStage::Stance:     return MenuStances.Num();
	default:                 return 0;
	}
}

FText AModeSelectPawn::GetRowLabel(int32 Index) const
{
	switch (Stage)
	{
	case EStage::Mode:       return UModeManager::GetModeDisplayName(ModeAt(Index));
	case EStage::Difficulty: return UModeManager::GetDifficultyDisplayName(DifficultyAt(Index));
	case EStage::Stance:     return UModeManager::GetStanceDisplayName(StanceAt(Index));
	default:                 return FText::GetEmpty();
	}
}

bool AModeSelectPawn::IsRowAvailable(int32 Index) const
{
	// 난이도·스탠스는 전부 선택 가능. 모드는 구현된 것만.
	return (Stage == EStage::Mode) ? UModeManager::IsModeImplemented(ModeAt(Index)) : true;
}

FText AModeSelectPawn::GetRowTag(int32 Index) const
{
	if (Stage != EStage::Mode)
	{
		return FText::GetEmpty(); // 난이도·스탠스 행은 태그 없음
	}
	return IsRowAvailable(Index)
		? FText::FromString(TEXT("플레이 가능"))
		: FText::FromString(TEXT("준비 중"));
}

FText AModeSelectPawn::GetHeaderSubtitle() const
{
	switch (Stage)
	{
	case EStage::Mode:
		return FText::FromString(TEXT("SporTrack : Baseball    모드를 선택하세요"));
	case EStage::Difficulty:
		return FText::FromString(FString::Printf(TEXT("%s — 난이도를 선택하세요"),
			*UModeManager::GetModeDisplayName(PendingMode).ToString()));
	case EStage::Stance:
		return FText::FromString(FString::Printf(TEXT("%s · %s — 타석을 선택하세요 (좌타/우타)"),
			*UModeManager::GetModeDisplayName(PendingMode).ToString(),
			*UModeManager::GetDifficultyDisplayName(PendingDifficulty).ToString()));
	default:
		return FText::GetEmpty();
	}
}

FText AModeSelectPawn::GetSelectedDescription() const
{
	switch (Stage)
	{
	case EStage::Mode:       return UModeManager::GetModeDescription(ModeAt(SelectedIndex));
	case EStage::Difficulty: return UModeManager::GetDifficultyDescription(DifficultyAt(SelectedIndex));
	case EStage::Stance:     return UModeManager::GetStanceDescription(StanceAt(SelectedIndex));
	default:                 return FText::GetEmpty();
	}
}

FText AModeSelectPawn::GetFooterStatus() const
{
	switch (Stage)
	{
	case EStage::Mode:
	{
		int32 Ready = 0;
		for (const EGameModeId M : MenuModes)
		{
			if (UModeManager::IsModeImplemented(M)) { ++Ready; }
		}
		return FText::FromString(FString::Printf(TEXT("구현 %d / %d 모드"), Ready, MenuModes.Num()));
	}
	case EStage::Difficulty:
		return FText::FromString(FString::Printf(TEXT("난이도 %d단계"), MenuDifficulties.Num()));
	case EStage::Stance:
		return FText::FromString(TEXT("타석 2종 (우타 / 좌타)"));
	default:
		return FText::GetEmpty();
	}
}

// ── 입력 ──

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

	// 뒤로 (난이도 → 모드).
	PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &AModeSelectPawn::Back);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AModeSelectPawn::Back);

	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &AModeSelectPawn::OpenViveBringup);
}

void AModeSelectPawn::OpenViveBringup()
{
	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->StartViveBringup();
	}
}

void AModeSelectPawn::MoveSelection(int32 Delta)
{
	const int32 Count = GetRowCount();
	if (Count <= 0)
	{
		return;
	}

	// 위/아래 순환. 미구현 모드도 커서는 올라간다 — 감추면 전체 구성이 안 보인다.
	SelectedIndex = (SelectedIndex + Delta + Count) % Count;

	NoticeText.Reset();
	NoticeTimer = 0.0f;
}

void AModeSelectPawn::SelectPrev() { MoveSelection(-1); }
void AModeSelectPawn::SelectNext() { MoveSelection(1); }

void AModeSelectPawn::Confirm()
{
	if (Stage == EStage::Mode)
	{
		const EGameModeId Mode = ModeAt(SelectedIndex);
		if (!UModeManager::IsModeImplemented(Mode))
		{
			NoticeText = FString::Printf(TEXT("%s — 아직 준비 중인 모드입니다."),
				*UModeManager::GetModeDisplayName(Mode).ToString());
			NoticeTimer = NoticeDurationSec;
			return;
		}

		// 난이도 단계로 진입. 커서는 아마추어(가운데)에서 시작.
		PendingMode = Mode;
		Stage = EStage::Difficulty;
		const int32 AmateurIdx = MenuDifficulties.IndexOfByKey(EDifficultyLevel::Amateur);
		SelectedIndex = (AmateurIdx != INDEX_NONE) ? AmateurIdx : 0;
		NoticeText.Reset();
		return;
	}

	if (Stage == EStage::Difficulty)
	{
		ConfirmDifficulty();
		return;
	}

	// 스탠스 확정 → 게임 시작 (타석 포함).
	AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogMotionBase, Warning, TEXT("ModeSelect: AMotionBaseGameMode 를 찾지 못해 시작할 수 없습니다."));
		return;
	}
	GM->StartMode(PendingMode, PendingDifficulty, StanceAt(SelectedIndex));
}

void AModeSelectPawn::ConfirmDifficulty()
{
	PendingDifficulty = DifficultyAt(SelectedIndex);

	// 타격 모드만 타석(좌타/우타)을 고른다. 그 외 모드는 난이도 확정 = 바로 시작.
	if (PendingMode == EGameModeId::Batting)
	{
		Stage = EStage::Stance;
		const int32 RightIdx = MenuStances.IndexOfByKey(EBattingStance::Right);
		SelectedIndex = (RightIdx != INDEX_NONE) ? RightIdx : 0;
		NoticeText.Reset();
		return;
	}

	AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogMotionBase, Warning, TEXT("ModeSelect: AMotionBaseGameMode 를 찾지 못해 시작할 수 없습니다."));
		return;
	}
	GM->StartMode(PendingMode, PendingDifficulty);
}

void AModeSelectPawn::Back()
{
	if (Stage == EStage::Stance)
	{
		// 스탠스 → 난이도. 방금 고른 난이도 위로 커서를 돌려놓는다.
		Stage = EStage::Difficulty;
		const int32 Idx = MenuDifficulties.IndexOfByKey(PendingDifficulty);
		SelectedIndex = (Idx != INDEX_NONE) ? Idx : 0;
		NoticeText.Reset();
		NoticeTimer = 0.0f;
		return;
	}

	if (Stage == EStage::Difficulty)
	{
		// 난이도 → 모드. 방금 고른 모드 위로 커서를 돌려놓는다.
		Stage = EStage::Mode;
		const int32 Idx = MenuModes.IndexOfByKey(PendingMode);
		SelectedIndex = (Idx != INDEX_NONE) ? Idx : FindFirstImplementedIndex();
		NoticeText.Reset();
		NoticeTimer = 0.0f;
		return;
	}

	// 모드 단계에서는 되돌아갈 곳이 없다.
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
