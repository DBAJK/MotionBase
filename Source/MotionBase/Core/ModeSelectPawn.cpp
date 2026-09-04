#include "Core/ModeSelectPawn.h"
#include "MotionBase.h"
#include "Core/ModeManager.h"
#include "Core/MotionBaseGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/VRInfoPanel.h"
#include "Core/Defense/Backup/BackupPlaybook.h"

namespace
{
	// 행 색: 준비중=회색, 일반=흰색, 호버중=앰버→초록(진행도).
	// (드웰 진행은 ASCII 막대가 아니라 카드 채움 + 조준점 링으로 보여준다 — UVRInfoPanel.)
	FColor MsRowColor(bool bAvail, bool bHovered, float Progress)
	{
		if (!bAvail)   { return FColor(110, 110, 122); }
		if (!bHovered) { return FColor(228, 233, 244); }
		const FLinearColor A(1.00f, 0.70f, 0.35f);
		const FLinearColor B(0.40f, 0.86f, 0.47f);
		return FLinearColor::LerpUsingHSV(A, B, Progress).ToFColor(true);
	}

	// ── VR 3D 텍스트용 영어 라벨 (TextRender 는 한글 폰트가 없어 깨지므로 영어로 표기) ──
	FString MsEnMode(EGameModeId M)
	{
		switch (M)
		{
		case EGameModeId::Batting:    return TEXT("Batting");
		case EGameModeId::Defense:    return TEXT("Defense");
		case EGameModeId::AICoaching: return TEXT("AI Coaching");
		default:                      return TEXT("Mode");
		}
	}
	FString MsEnDifficulty(EDifficultyLevel D)
	{
		switch (D)
		{
		case EDifficultyLevel::Beginner: return TEXT("Beginner");
		case EDifficultyLevel::Amateur:  return TEXT("Amateur");
		case EDifficultyLevel::Pro:      return TEXT("Pro");
		default:                         return TEXT("Difficulty");
		}
	}
	FString MsEnStance(EBattingStance S)
	{
		return (S == EBattingStance::Left) ? TEXT("Left (LHH)") : TEXT("Right (RHH)");
	}
	FString MsEnDrill(int32 Index)
	{
		switch (Index)
		{
		case 0:  return TEXT("Catch");
		case 1:  return TEXT("Throw");
		case 2:  return TEXT("Backup");
		default: return TEXT("Drill");
		}
	}
}

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

	// ── VR 인메뉴 컴포넌트 (HMD 없으면 BeginPlay 에서 숨긴다) ──
	// 겨눔 포인터 = 컨트롤러(오른손). 배트와 같은 방식으로 추적된다.
	PointerController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("PointerController"));
	PointerController->SetupAttachment(SceneRoot);
	PointerController->MotionSource = FName(TEXT("Right"));

	// VR 3D 패널 — 트래킹 원점(SceneRoot)에 붙여 **월드 고정**한다. 카메라(머리)에
	// 붙이면 고개를 돌려도 따라와 "시점이 안 움직인다"고 느껴지고 멀미를 유발한다.
	// 패널 내부 레이아웃(제목·행·설명·힌트 Z)이 기존 배치와 동일하다 (UVRInfoPanel).
	VrPanel = CreateDefaultSubobject<UVRInfoPanel>(TEXT("VrPanel"));
	VrPanel->SetupAttachment(SceneRoot);
	VrPanel->SetPlacement(MenuDistanceCm, MenuHeightCm);
}

void AModeSelectPawn::BeginPlay()
{
	Super::BeginPlay();

	MenuModes = UModeManager::GetMenuModes();
	MenuDifficulties = UModeManager::GetMenuDifficulties();
	MenuStances = UModeManager::GetMenuStances();

	// 수비 세부 종목 3개 (포구 / 송구 / 백업 위치 판단). 풋워크·반응속도는 제외.
	DefenseDrills = {
		FText::FromString(TEXT("포구")),
		FText::FromString(TEXT("송구")),
		FText::FromString(TEXT("백업 위치 판단"))
	};

	Stage = EStage::Mode;
	// 커서는 플레이 가능한 모드 위에서 시작한다 (0번은 미구현이라 첫인상이 나쁘다).
	SelectedIndex = FindFirstImplementedIndex();

	// HMD 가 켜져 있으면 헤드셋 안 3D 메뉴 활성화 (없으면 기존 키보드+평면 HUD).
	if (VrPanel) { VrPanel->BuildPanel(); }
	InitVRMenu();
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

FText AModeSelectPawn::DefenseDrillNameAt(int32 Index) const
{
	return DefenseDrills.IsValidIndex(Index) ? DefenseDrills[Index] : FText::GetEmpty();
}

FText AModeSelectPawn::DefenseDrillDescAt(int32 Index) const
{
	static const TArray<FText> Descs = {
		FText::FromString(TEXT("땅볼·뜬공·라인드라이브를 받아내는 포구 훈련 (타입별 성공률 측정)")),
		FText::FromString(TEXT("포구 → 지정된 베이스로 송구 (정확도·구속·전환시간 측정)")),
		FText::FromString(TEXT("타구 방향 + 주자 상황으로 백업 위치를 고르는 판단 훈련"))
	};
	return Descs.IsValidIndex(Index) ? Descs[Index] : FText::GetEmpty();
}

FText AModeSelectPawn::PositionGroupNameAt(int32 Index) const
{
	return (Index == 0) ? FText::FromString(TEXT("내야 (1루·2루·유격·3루)"))
	                     : FText::FromString(TEXT("외야 (좌익·중견·우익)"));
}

FText AModeSelectPawn::PositionGroupDescAt(int32 Index) const
{
	return (Index == 0)
		? FText::FromString(TEXT("내야 포지션에서 백업 위치 판단을 훈련합니다."))
		: FText::FromString(TEXT("외야 포지션에서 백업 위치 판단을 훈련합니다 — 이동 거리가 더 깁니다."));
}

TArray<EFieldPosition> AModeSelectPawn::PositionsInGroup() const
{
	// 7개를 한 목록에 넣으면 UVRInfoPanel::MaxRows(6)를 넘는다 — 내야(4)/외야(3)로 쪼갠 이유.
	static const TArray<EFieldPosition> Infield = { EFieldPosition::First, EFieldPosition::Second, EFieldPosition::Short, EFieldPosition::Third };
	static const TArray<EFieldPosition> Outfield = { EFieldPosition::Left, EFieldPosition::Center, EFieldPosition::Right };
	return (PendingPositionGroup == 0) ? Infield : Outfield;
}

FText AModeSelectPawn::FieldPositionNameAt(int32 Index) const
{
	const TArray<EFieldPosition> Positions = PositionsInGroup();
	if (!Positions.IsValidIndex(Index)) { return FText::GetEmpty(); }

	switch (Positions[Index])
	{
	case EFieldPosition::First:  return FText::FromString(TEXT("1루수"));
	case EFieldPosition::Second: return FText::FromString(TEXT("2루수"));
	case EFieldPosition::Short:  return FText::FromString(TEXT("유격수"));
	case EFieldPosition::Third:  return FText::FromString(TEXT("3루수"));
	case EFieldPosition::Left:   return FText::FromString(TEXT("좌익수"));
	case EFieldPosition::Center: return FText::FromString(TEXT("중견수"));
	case EFieldPosition::Right:  return FText::FromString(TEXT("우익수"));
	default:                     return FText::GetEmpty();
	}
}

FText AModeSelectPawn::FieldPositionDescAt(int32 Index) const
{
	const TArray<EFieldPosition> Positions = PositionsInGroup();
	if (!Positions.IsValidIndex(Index)) { return FText::GetEmpty(); }

	switch (Positions[Index])
	{
	case EFieldPosition::First:  return FText::FromString(TEXT("1루 커버·1루 뒤 백업·외야 중계를 판단합니다."));
	case EFieldPosition::Second: return FText::FromString(TEXT("1루 백업·2루 커버(도루·병살)를 판단합니다."));
	case EFieldPosition::Short:  return FText::FromString(TEXT("2루 커버·3루 커버·좌중견 중계를 판단합니다."));
	case EFieldPosition::Third:  return FText::FromString(TEXT("3루 커버·번트 처리 상황을 판단합니다."));
	case EFieldPosition::Left:   return FText::FromString(TEXT("3루 뒤 백업 상황을 판단합니다."));
	case EFieldPosition::Center: return FText::FromString(TEXT("2루 뒤 백업 + 좌우익수 뒤 광범위 백업을 판단합니다 (외야 사령탑)."));
	case EFieldPosition::Right:  return FText::FromString(TEXT("1루 뒤 백업 상황을 판단합니다."));
	default:                     return FText::GetEmpty();
	}
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
	case EStage::Mode:                 return MenuModes.Num();
	case EStage::Difficulty:           return MenuDifficulties.Num();
	case EStage::Stance:                return MenuStances.Num();
	case EStage::DefenseDrill:          return DefenseDrills.Num();
	case EStage::DefensePositionGroup:  return 2; // 내야/외야
	case EStage::DefensePosition:       return PositionsInGroup().Num();
	default:                            return 0;
	}
}

FText AModeSelectPawn::GetRowLabel(int32 Index) const
{
	switch (Stage)
	{
	case EStage::Mode:                 return UModeManager::GetModeDisplayName(ModeAt(Index));
	case EStage::Difficulty:           return UModeManager::GetDifficultyDisplayName(DifficultyAt(Index));
	case EStage::Stance:                return UModeManager::GetStanceDisplayName(StanceAt(Index));
	case EStage::DefenseDrill:          return DefenseDrillNameAt(Index);
	case EStage::DefensePositionGroup:  return PositionGroupNameAt(Index);
	case EStage::DefensePosition:       return FieldPositionNameAt(Index);
	default:                            return FText::GetEmpty();
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
	case EStage::DefenseDrill:
		return FText::FromString(TEXT("수비 훈련 — 세부 종목을 선택하세요"));
	case EStage::DefensePositionGroup:
		return FText::FromString(TEXT("백업 위치 판단 — 내야/외야를 선택하세요"));
	case EStage::DefensePosition:
		return FText::FromString(FString::Printf(TEXT("백업 위치 판단 — %s"),
			*PositionGroupNameAt(PendingPositionGroup).ToString()));
	default:
		return FText::GetEmpty();
	}
}

FText AModeSelectPawn::GetSelectedDescription() const
{
	switch (Stage)
	{
	case EStage::Mode:                 return UModeManager::GetModeDescription(ModeAt(SelectedIndex));
	case EStage::Difficulty:           return UModeManager::GetDifficultyDescription(DifficultyAt(SelectedIndex));
	case EStage::Stance:                return UModeManager::GetStanceDescription(StanceAt(SelectedIndex));
	case EStage::DefenseDrill:          return DefenseDrillDescAt(SelectedIndex);
	case EStage::DefensePositionGroup:  return PositionGroupDescAt(SelectedIndex);
	case EStage::DefensePosition:       return FieldPositionDescAt(SelectedIndex);
	default:                            return FText::GetEmpty();
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
	case EStage::DefenseDrill:
		return FText::FromString(FString::Printf(TEXT("수비 세부 종목 %d종"), DefenseDrills.Num()));
	case EStage::DefensePositionGroup:
		return FText::FromString(TEXT("내야 4 / 외야 3 포지션"));
	case EStage::DefensePosition:
		return FText::FromString(FString::Printf(TEXT("%s 포지션 %d개"),
			*PositionGroupNameAt(PendingPositionGroup).ToString(), PositionsInGroup().Num()));
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

	// 뒤로 (타석→난이도→모드, 수비종목→모드).
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

		PendingMode = Mode;

		// AI 코칭은 난이도·타석이 없는 읽기 전용 리뷰 화면 — 바로 진입한다.
		if (Mode == EGameModeId::AICoaching)
		{
			if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
			{
				GM->StartAICoaching();
			}
			return;
		}

		// 수비는 세부 종목 선택 단계로, 그 외는 난이도 단계로 진입한다.
		if (Mode == EGameModeId::Defense)
		{
			Stage = EStage::DefenseDrill;
			SelectedIndex = 0;
			NoticeText.Reset();
			return;
		}

		// 난이도 단계로 진입. 커서는 아마추어(가운데)에서 시작.
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

	if (Stage == EStage::DefenseDrill)
	{
		// 백업 위치 판단(index 2)은 포지션을 먼저 골라야 한다 — 바로 시작하지 않고
		// DefensePositionGroup 단계로 진입한다 (7 포지션이 UVRInfoPanel::MaxRows 를 넘어
		// 내야/외야 2단계로 쪼갰다 — 설계 노트).
		if (SelectedIndex == 2)
		{
			Stage = EStage::DefensePositionGroup;
			SelectedIndex = 0;
			NoticeText.Reset();
			return;
		}

		// 나머지 종목(포구/송구)은 곧바로 진입.
		AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr;
		if (GM && !GM->StartDefenseDrill(SelectedIndex))
		{
			// 아직 준비 중인 종목 — 안내만.
			NoticeText = FString::Printf(TEXT("%s — 아직 준비 중인 종목입니다."),
				*DefenseDrillNameAt(SelectedIndex).ToString());
			NoticeTimer = NoticeDurationSec;
		}
		return;
	}

	if (Stage == EStage::DefensePositionGroup)
	{
		PendingPositionGroup = SelectedIndex;
		Stage = EStage::DefensePosition;
		SelectedIndex = 0;
		NoticeText.Reset();
		return;
	}

	if (Stage == EStage::DefensePosition)
	{
		// 포지션 확정 → 백업 위치 판단 훈련 폰으로 진입 (DrillIndex 2 = 백업).
		const TArray<EFieldPosition> Positions = PositionsInGroup();
		PendingFieldPosition = Positions.IsValidIndex(SelectedIndex) ? Positions[SelectedIndex] : EFieldPosition::First;

		AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr;
		if (!GM)
		{
			UE_LOG(LogMotionBase, Warning, TEXT("ModeSelect: AMotionBaseGameMode 를 찾지 못해 시작할 수 없습니다."));
			return;
		}
		GM->StartDefenseDrill(2, PendingFieldPosition);
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

	if (Stage == EStage::Difficulty || Stage == EStage::DefenseDrill)
	{
		// 난이도/수비종목 → 모드. 방금 고른 모드 위로 커서를 돌려놓는다.
		Stage = EStage::Mode;
		const int32 Idx = MenuModes.IndexOfByKey(PendingMode);
		SelectedIndex = (Idx != INDEX_NONE) ? Idx : FindFirstImplementedIndex();
		NoticeText.Reset();
		NoticeTimer = 0.0f;
		return;
	}

	if (Stage == EStage::DefensePosition)
	{
		// 포지션 → 그룹. 방금 고른 그룹 위로 커서를 돌려놓는다.
		Stage = EStage::DefensePositionGroup;
		SelectedIndex = PendingPositionGroup;
		NoticeText.Reset();
		NoticeTimer = 0.0f;
		return;
	}

	if (Stage == EStage::DefensePositionGroup)
	{
		// 그룹 → 수비 종목(백업이 선택돼 있던 자리로).
		Stage = EStage::DefenseDrill;
		SelectedIndex = 2;
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

	if (bVRMenu)
	{
		UpdateVRMenu(DeltaSeconds);
	}
}

// ── VR 인메뉴 구현 ──────────────────────────────────────────────

void AModeSelectPawn::InitVRMenu()
{
	bVRMenu = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();

	if (VrPanel) { VrPanel->SetPlacement(MenuDistanceCm, MenuHeightCm); }

	if (!bVRMenu)
	{
		// PC(키보드) 모드 — 3D 패널은 전부 끈다.
		if (VrPanel) { VrPanel->HideAll(); }
		return;
	}

	// 바닥 기준 트래킹 → MenuHeightCm(눈높이)이 실제 높이와 맞는다.
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);

	RefreshVRMenuTexts();
	UE_LOG(LogMotionBase, Log, TEXT("ModeSelect: VR 인메뉴 활성화 (드웰 %.1fs / %.0f°)"),
		DwellTimeSec, DwellAngleDeg);
}

int32 AModeSelectPawn::PickHoveredCard() const
{
	if (!PointerController || !PointerController->IsTracked() || !VrPanel)
	{
		return INDEX_NONE; // 추적 안 되면(베이스 스테이션 꺼짐 등) 오선택 방지.
	}

	const FVector Origin = PointerController->GetComponentLocation();
	const FVector Aim    = PointerController->GetForwardVector();
	const float   CosThresh = FMath::Cos(FMath::DegreesToRadians(DwellAngleDeg));

	int32 Best = INDEX_NONE;
	float BestCos = CosThresh;

	const int32 RowCount = GetRowCount();
	for (int32 i = 0; i < RowCount && i < UVRInfoPanel::MaxRows; ++i)
	{
		UTextRenderComponent* Row = VrPanel->GetRowText(i);
		if (!Row || !IsRowAvailable(i)) { continue; } // 준비 중 카드는 겨눔 대상 아님.
		const FVector Dir = (Row->GetComponentLocation() - Origin).GetSafeNormal();
		const float C = FVector::DotProduct(Aim, Dir);
		if (C > BestCos) { BestCos = C; Best = i; }
	}

	// 뒤로 카드 (모드 단계 외에서만) — 호버 인덱스는 RowCount.
	if (Stage != EStage::Mode)
	{
		UTextRenderComponent* Back = VrPanel->GetBackText();
		if (Back && Back->IsVisible())
		{
			const FVector Dir = (Back->GetComponentLocation() - Origin).GetSafeNormal();
			const float C = FVector::DotProduct(Aim, Dir);
			if (C > BestCos) { BestCos = C; Best = RowCount; }
		}
	}

	return Best;
}

void AModeSelectPawn::UpdateMenuAnchor(float /*DeltaSeconds*/)
{
	if (!VrPanel || !Camera) { return; }

	// 편안한 배치: 정면 고정 + 크게 돌아볼 때만 재정렬(swimming 제거, 이질감 제거).
	// (예전의 데드존 lazy-follow 는 패널이 계속 헤엄쳐 멀미를 유발했다.)
	VrPanel->UpdateComfortAnchor(Camera, MenuDistanceCm, MenuHeightCm, /*RecenterDeg=*/55.0f);

	// 곡면 배치 — 눈높이 대비 위/아래 카드를 눈 쪽으로 감아 기울인다.
	if (bCurvedMenu)
	{
		const float HeadZ = Camera->GetRelativeLocation().Z;
		VrPanel->ApplyCurvedLayout(HeadZ - MenuHeightCm, MenuDistanceCm);
	}
}

void AModeSelectPawn::UpdateVRMenu(float DeltaSeconds)
{
	if (VrCooldown > 0.0f) { VrCooldown = FMath::Max(0.0f, VrCooldown - DeltaSeconds); }

	// 겨눔 판정보다 먼저 패널을 제자리에 놓는다 (카드 위치가 판정 기준이므로 순서가 중요).
	UpdateMenuAnchor(DeltaSeconds);

	const int32 Hover = (VrCooldown > 0.0f) ? INDEX_NONE : PickHoveredCard();

	if (Hover != VrHoverIndex)
	{
		VrHoverIndex = Hover;
		VrDwellTimer = 0.0f;
	}

	// 컨트롤러 트리거(아래쪽 검지 버튼) 눌림 에지 검출.
	// 카드를 겨눈 상태에서 트리거를 당기면 즉시 확정한다 (드웰을 기다릴 필요 없음).
	bool bTriggerPressedEdge = false;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		const FName Hand = (PointerController ? PointerController->MotionSource : FName(TEXT("Right")));
		const TCHAR* Side = (Hand == FName(TEXT("Left"))) ? TEXT("Left") : TEXT("Right");
		// 트리거(아래 검지 버튼)를 제네릭/Vive 두 이름으로 읽어 매핑에 관계없이 동작하게 한다.
		const float Generic = PC->GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("MotionController_%s_Trigger"), Side)));
		const float Vive    = PC->GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("Vive_%s_Trigger"), Side)));
		const bool bHeld = FMath::Max(Generic, Vive) >= TriggerPressThreshold;
		bTriggerPressedEdge = (bHeld && !bTriggerHeldPrev);
		bTriggerHeldPrev = bHeld;
	}

	if (Hover != INDEX_NONE)
	{
		if (Hover < GetRowCount()) { SelectedIndex = Hover; } // 설명 표시를 커서와 동기화.

		VrDwellTimer += DeltaSeconds;

		// 확정 조건: 트리거를 눌렀거나(즉시), 드웰 시간이 찼거나(폴백).
		const bool bCommit = (VrCooldown <= 0.0f) && (bTriggerPressedEdge || VrDwellTimer >= DwellTimeSec);
		if (bCommit)
		{
			const int32 RowCount = GetRowCount();
			VrHoverIndex = INDEX_NONE;
			VrDwellTimer = 0.0f;
			VrCooldown   = 0.6f; // 확정 직후 오선택 방지.

			if (Hover == RowCount) { Back(); }
			else { SelectedIndex = Hover; Confirm(); }
			return; // Confirm 이 폰 교체를 예약할 수 있으니 이 프레임은 종료.
		}
	}
	else
	{
		VrDwellTimer = 0.0f;
	}

	RefreshVRMenuTexts();

	// ── 공간 연출 (텍스트 갱신 뒤에 그려야 이번 프레임 상태와 어긋나지 않는다) ──
	const float Progress = (DwellTimeSec > 0.0f)
		? FMath::Clamp(VrDwellTimer / DwellTimeSec, 0.0f, 1.0f) : 0.0f;
	const int32 RowCount = GetRowCount();

	if (bCurvedMenu && VrPanel && Camera)
	{
		// 뒤로 카드는 RefreshVRMenuTexts 에서 위치가 다시 잡히므로 곡면을 한 번 더 적용한다.
		VrPanel->ApplyCurvedLayout(Camera->GetRelativeLocation().Z - MenuHeightCm, MenuDistanceCm);
	}

	if (VrPanel)
	{
		VrPanel->TickHoverAnim(DeltaSeconds, Hover, RowCount);
		VrPanel->DrawChrome(RowCount, Hover, Progress, Stage != EStage::Mode);

		if (PointerController && PointerController->IsTracked())
		{
			VrPanel->DrawPointerRay(
				PointerController->GetComponentLocation(),
				PointerController->GetForwardVector(),
				Progress, Hover != INDEX_NONE);
		}
	}
}

void AModeSelectPawn::RefreshVRMenuTexts()
{
	if (!bVRMenu || !VrPanel) { return; }

	// 제목 = 브레드크럼 (단계별, 영어 — 3D 텍스트는 한글 폰트가 없어 영어로 표기).
	// 3단계까지 들어가면 "지금 어디쯤인가"가 헷갈린다 — 지나온 선택을 제목에 남긴다.
	FString Header;
	switch (Stage)
	{
	case EStage::Mode:
		Header = TEXT("SporTrack : Baseball   >   Mode"); break;
	case EStage::Difficulty:
		Header = MsEnMode(PendingMode) + TEXT("   >   Difficulty"); break;
	case EStage::Stance:
		Header = MsEnMode(PendingMode) + TEXT("   >   ") + MsEnDifficulty(PendingDifficulty)
			+ TEXT("   >   Batter box"); break;
	case EStage::DefenseDrill:
		Header = TEXT("Defense   >   Drill"); break;
	case EStage::DefensePositionGroup:
		Header = TEXT("Defense   >   Backup   >   Infield/Outfield"); break;
	case EStage::DefensePosition:
		Header = FString::Printf(TEXT("Defense   >   Backup   >   %s   >   Position"),
			(PendingPositionGroup == 0) ? TEXT("Infield") : TEXT("Outfield")); break;
	default: break;
	}
	VrPanel->SetTitle(Header, FColor(228, 233, 244));

	const int32 RowCount = GetRowCount();
	const float Progress = (DwellTimeSec > 0.0f)
		? FMath::Clamp(VrDwellTimer / DwellTimeSec, 0.0f, 1.0f) : 0.0f;

	for (int32 i = 0; i < RowCount && i < UVRInfoPanel::MaxRows; ++i)
	{
		const bool bAvail   = IsRowAvailable(i);
		const bool bHovered = (VrHoverIndex == i);

		FString Label;
		switch (Stage)
		{
		case EStage::Mode:         Label = MsEnMode(ModeAt(i)); break;
		case EStage::Difficulty:   Label = MsEnDifficulty(DifficultyAt(i)); break;
		case EStage::Stance:       Label = MsEnStance(StanceAt(i)); break;
		case EStage::DefenseDrill: Label = MsEnDrill(i); break;
		case EStage::DefensePositionGroup:
			Label = (i == 0) ? TEXT("Infield (1B/2B/SS/3B)") : TEXT("Outfield (LF/CF/RF)");
			break;
		case EStage::DefensePosition:
		{
			const TArray<EFieldPosition> Positions = PositionsInGroup();
			Label = Positions.IsValidIndex(i) ? UBackupPlaybook::PositionName(Positions[i]) : TEXT("?");
			break;
		}
		default: break;
		}
		if (!bAvail)  { Label += TEXT("  (coming soon)"); }
		// 겨누는 카드는 앞에 표식을 붙여 텍스트만 봐도 구분되게 한다
		// (진행도 자체는 카드 채움/링이 보여주므로 여기선 막대를 쓰지 않는다).
		if (bHovered) { Label = TEXT("> ") + Label; }

		VrPanel->SetRow(i, Label, MsRowColor(bAvail, bHovered, Progress));
	}
	VrPanel->HideRowsFrom(RowCount);

	// 뒤로 카드 (모드 단계 외에서만).
	if (Stage != EStage::Mode)
	{
		const bool bHovered = (VrHoverIndex == RowCount);
		VrPanel->SetBackBelowRows(RowCount, TEXT("< Back"),
			MsRowColor(true, bHovered, Progress), true);
	}
	else
	{
		VrPanel->SetBackBelowRows(RowCount, FString(), FColor::White, false);
	}

	// 설명 → 푸터 (영어).
	FString Desc;
	if (!NoticeText.IsEmpty())
	{
		Desc = TEXT("Coming soon");
	}
	else
	{
		switch (Stage)
		{
		case EStage::Mode:
			Desc = (ModeAt(SelectedIndex) == EGameModeId::Batting)
				? TEXT("Swing at pitches with the controller")
				: TEXT("Fielding drills: catch / throw / backup");
			break;
		case EStage::Difficulty:
			switch (DifficultyAt(SelectedIndex))
			{
			case EDifficultyLevel::Beginner: Desc = TEXT("Slow pitches, no breaking balls"); break;
			case EDifficultyLevel::Amateur:  Desc = TEXT("Medium speed + some breaking balls"); break;
			case EDifficultyLevel::Pro:      Desc = TEXT("Fast pitches + many breaking balls"); break;
			default: break;
			}
			break;
		case EStage::Stance:
			Desc = (StanceAt(SelectedIndex) == EBattingStance::Left)
				? TEXT("Left-handed batter box")
				: TEXT("Right-handed batter box");
			break;
		case EStage::DefenseDrill:
			switch (SelectedIndex)
			{
			case 0: Desc = TEXT("Catch grounders, flies and liners - success rate per ball type"); break;
			case 1: Desc = TEXT("Catch, then throw to the called base - accuracy, velocity, transfer"); break;
			case 2: Desc = TEXT("Pick your position, then read the ball and move to your real backup spot"); break;
			default: break;
			}
			break;
		case EStage::DefensePositionGroup:
			Desc = (SelectedIndex == 0)
				? TEXT("Infield jobs: shorter runs, base coverage and relay cutoffs")
				: TEXT("Outfield jobs: longer runs, backing up bases and the other outfielders");
			break;
		case EStage::DefensePosition:
			Desc = TEXT("A situation is called, then hold the move button and go to your real backup spot");
			break;
		default: break;
		}
	}
	VrPanel->SetFooter(Desc, !NoticeText.IsEmpty() ? FColor(255, 180, 90) : FColor(150, 156, 168));

	VrPanel->SetHint(
		TEXT("Point at a card - trigger to pick, or just hold your aim   ·   the ring shows the hold"),
		FColor(110, 116, 128));
}
