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
#include "UI/VRInfoPanel.h"

namespace
{
	// 드웰 진행바 (ASCII — 폰트 글리프 걱정 없음). 예: "   [===...]"
	FString MsDwellBar(float Progress)
	{
		const int32 Cells = 6;
		const int32 Filled = FMath::Clamp(FMath::RoundToInt(Progress * Cells), 0, Cells);
		return FString::Printf(TEXT("   [%s%s]"),
			*FString::ChrN(Filled, TEXT('=')),
			*FString::ChrN(Cells - Filled, TEXT('.')));
	}

	// 행 색: 준비중=회색, 일반=흰색, 호버중=앰버→초록(진행도).
	FColor MsRowColor(bool bAvail, bool bHovered, float Progress)
	{
		if (!bAvail)   { return FColor(110, 110, 122); }
		if (!bHovered) { return FColor(228, 233, 244); }
		const FLinearColor A(1.00f, 0.70f, 0.35f);
		const FLinearColor B(0.40f, 0.86f, 0.47f);
		return FLinearColor::LerpUsingHSV(A, B, Progress).ToFColor(true);
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
		FText::FromString(TEXT("타구를 받아내는 포구 동작 훈련 (컨트롤러로 글러브)")),
		FText::FromString(TEXT("포구 후 정확한 송구 동작 훈련 (컨트롤러 스윙)")),
		FText::FromString(TEXT("랜덤 타구 상황에서 백업 위치를 고르는 판단 훈련"))
	};
	return Descs.IsValidIndex(Index) ? Descs[Index] : FText::GetEmpty();
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
	case EStage::Mode:         return MenuModes.Num();
	case EStage::Difficulty:   return MenuDifficulties.Num();
	case EStage::Stance:       return MenuStances.Num();
	case EStage::DefenseDrill: return DefenseDrills.Num();
	default:                   return 0;
	}
}

FText AModeSelectPawn::GetRowLabel(int32 Index) const
{
	switch (Stage)
	{
	case EStage::Mode:         return UModeManager::GetModeDisplayName(ModeAt(Index));
	case EStage::Difficulty:   return UModeManager::GetDifficultyDisplayName(DifficultyAt(Index));
	case EStage::Stance:       return UModeManager::GetStanceDisplayName(StanceAt(Index));
	case EStage::DefenseDrill: return DefenseDrillNameAt(Index);
	default:                   return FText::GetEmpty();
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
	default:
		return FText::GetEmpty();
	}
}

FText AModeSelectPawn::GetSelectedDescription() const
{
	switch (Stage)
	{
	case EStage::Mode:         return UModeManager::GetModeDescription(ModeAt(SelectedIndex));
	case EStage::Difficulty:   return UModeManager::GetDifficultyDescription(DifficultyAt(SelectedIndex));
	case EStage::Stance:       return UModeManager::GetStanceDescription(StanceAt(SelectedIndex));
	case EStage::DefenseDrill: return DefenseDrillDescAt(SelectedIndex);
	default:                   return FText::GetEmpty();
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
		// 수비 세부 종목 확정 → 해당 훈련 폰으로 진입.
		// SelectedIndex: 0=포구, 1=송구, 2=풋워크/반응속도, 3=백업
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

void AModeSelectPawn::UpdateVRMenu(float DeltaSeconds)
{
	if (VrCooldown > 0.0f) { VrCooldown = FMath::Max(0.0f, VrCooldown - DeltaSeconds); }

	// 포인터 광선 표시 (컨트롤러 → 정면).
	if (PointerController && PointerController->IsTracked() && GetWorld())
	{
		const FVector Origin = PointerController->GetComponentLocation();
		const FVector End = Origin + PointerController->GetForwardVector() * (MenuDistanceCm + 60.0f);
		DrawDebugLine(GetWorld(), Origin, End, FColor(80, 200, 255), false, -1.0f, 0, 0.4f);
	}

	const int32 Hover = (VrCooldown > 0.0f) ? INDEX_NONE : PickHoveredCard();

	if (Hover != VrHoverIndex)
	{
		VrHoverIndex = Hover;
		VrDwellTimer = 0.0f;
	}

	if (Hover != INDEX_NONE)
	{
		if (Hover < GetRowCount()) { SelectedIndex = Hover; } // 설명 표시를 커서와 동기화.

		VrDwellTimer += DeltaSeconds;
		if (VrDwellTimer >= DwellTimeSec)
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
}

void AModeSelectPawn::RefreshVRMenuTexts()
{
	if (!bVRMenu || !VrPanel) { return; }

	VrPanel->SetTitle(GetHeaderSubtitle().ToString(), FColor(228, 233, 244));

	const int32 RowCount = GetRowCount();
	const float Progress = (DwellTimeSec > 0.0f)
		? FMath::Clamp(VrDwellTimer / DwellTimeSec, 0.0f, 1.0f) : 0.0f;

	for (int32 i = 0; i < RowCount && i < UVRInfoPanel::MaxRows; ++i)
	{
		const bool bAvail   = IsRowAvailable(i);
		const bool bHovered = (VrHoverIndex == i);

		FString Label = GetRowLabel(i).ToString();
		if (!bAvail)   { Label += TEXT("  (준비 중)"); }
		if (bHovered)  { Label += MsDwellBar(Progress); }

		VrPanel->SetRow(i, Label, MsRowColor(bAvail, bHovered, Progress));
	}
	VrPanel->HideRowsFrom(RowCount);

	// 뒤로 카드 (모드 단계 외에서만) — 행 바로 아래에 배치.
	if (Stage != EStage::Mode)
	{
		const bool bHovered = (VrHoverIndex == RowCount);
		FString Label = TEXT("◀ 뒤로");
		if (bHovered) { Label += MsDwellBar(Progress); }
		VrPanel->SetBackBelowRows(RowCount, Label, MsRowColor(true, bHovered, Progress), true);
	}
	else
	{
		VrPanel->SetBackBelowRows(RowCount, FString(), FColor::White, false);
	}

	// 설명 / 안내 문구 → 푸터.
	const FString Desc = !NoticeText.IsEmpty() ? NoticeText : GetSelectedDescription().ToString();
	VrPanel->SetFooter(Desc, !NoticeText.IsEmpty() ? FColor(255, 180, 90) : FColor(150, 156, 168));

	// 힌트.
	VrPanel->SetHint(
		TEXT("컨트롤러로 카드를 겨누고 잠시 유지하면 선택  ·  (키보드 W/S · Enter 도 가능)"),
		FColor(110, 116, 128));
}
