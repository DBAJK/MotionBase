#include "Core/Defense/Cover/CoverPawn.h"
#include "Core/MotionBaseGameMode.h"
#include "MotionBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Core/Defense/Cover/CoverHUD.h"
#include "UI/ModeSelectHUD.h"
#include "UI/VRInfoPanel.h"

ACoverPawn::ACoverPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	SetRootComponent(Capsule);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Capsule);
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = false;

	// 겨눔 포인터 = 오른손 컨트롤러.
	PointerController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("PointerController"));
	PointerController->SetupAttachment(Capsule);
	PointerController->MotionSource = FName(TEXT("Right"));

	// VR 3D 패널 — 카메라(머리)가 아니라 캡슐 루트에 붙여 **월드 고정**한다.
	// (헤드락은 고개를 돌리면 패널이 따라와 부자연스럽고 멀미를 유발한다. #3)
	VrPanel = CreateDefaultSubobject<UVRInfoPanel>(TEXT("VrPanel"));
	VrPanel->SetupAttachment(Capsule);
	VrPanel->SetPlacement(MenuDistanceCm, 60.0f); // 캡슐 중심 기준 눈높이 부근
}

void ACoverPawn::BeginPlay()
{
	Super::BeginPlay();

	SetActorRotation(FRotator::ZeroRotator);

	bVR = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
	if (bVR)
	{
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);
	}

	if (VrPanel) { VrPanel->BuildPanel(); }

	BuildQuizPool();
	InitVRMenu();
	StartSession();
}

void ACoverPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 숫자키 1~4 로 바로 선택.
	PlayerInputComponent->BindKey(EKeys::One,   IE_Pressed, this, &ACoverPawn::PickOption0);
	PlayerInputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &ACoverPawn::PickOption1);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACoverPawn::PickOption2);
	PlayerInputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &ACoverPawn::PickOption3);

	// 커서 이동 + 확정 (키보드 보조).
	PlayerInputComponent->BindKey(EKeys::W,  IE_Pressed, this, &ACoverPawn::SelectPrev);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ACoverPawn::SelectPrev);
	PlayerInputComponent->BindKey(EKeys::S,    IE_Pressed, this, &ACoverPawn::SelectNext);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ACoverPawn::SelectNext);
	PlayerInputComponent->BindKey(EKeys::Enter,    IE_Pressed, this, &ACoverPawn::ConfirmSelection);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACoverPawn::ConfirmSelection);

	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ACoverPawn::ReturnToModeSelect);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(ACoverHUD::StaticClass());
	}
}

// ── 문제 은행 ──

void ACoverPawn::BuildQuizPool()
{
	auto Add = [this](const TCHAR* Sit, const TCHAR* InRole, int32 Correct, const TCHAR* Explain)
	{
		FBackupQuiz Q;
		Q.Situation = Sit;
		Q.Role = InRole;
		Q.Options = { TEXT("1B"), TEXT("2B"), TEXT("3B"), TEXT("Home") };
		Q.Correct = Correct;
		Q.Explain = Explain;
		QuizPool.Add(Q);
	};

	// Correct: 0=1B, 1=2B, 2=3B, 3=Home. (standard baseball backup/cover rules)
	Add(TEXT("No outs, bases empty - grounder to infield, throw to 1B"), TEXT("Catcher"), 0,
		TEXT("With no runners on, the catcher backs up the throw to first base."));
	Add(TEXT("Single to RF - runner heading to 3B, throw to 3B"), TEXT("Pitcher"), 2,
		TEXT("On a throw from the outfield to third, the pitcher backs up 3B."));
	Add(TEXT("Extra-base hit to CF - runner scoring, throw home"), TEXT("Pitcher"), 3,
		TEXT("On a throw from the outfield to home, the pitcher backs up the plate."));
	Add(TEXT("Surprise bunt - 1B charges in to field it"), TEXT("2nd baseman"), 0,
		TEXT("When the first baseman fields the bunt, the second baseman covers first."));
	Add(TEXT("Bunt - 3B charges in to field it"), TEXT("Shortstop"), 2,
		TEXT("When the third baseman fields the bunt, the shortstop covers third."));
	Add(TEXT("Runner steals - catcher throws to 2B"), TEXT("Center fielder"), 1,
		TEXT("On a throw to second, the center fielder backs up the bag."));
}

// ── 세션 ──

void ACoverPawn::StartSession()
{
	TrialIndex = 0;
	SuccessCount = 0;
	bSessionOver = false;
	bWaitingNext = false;
	IntervalTimer = 0.0f;

	SpawnNextTrial();
}

void ACoverPawn::SpawnNextTrial()
{
	if (QuizPool.Num() == 0)
	{
		EndSession();
		return;
	}

	CurrentQuiz = QuizPool[FMath::RandRange(0, QuizPool.Num() - 1)];
	SelectedIndex = 0;
	ChosenIndex = -1;
	bAnswered = false;
	VrHoverIndex = INDEX_NONE;
	VrDwellTimer = 0.0f;
	VrCooldown = 0.5f; // 새 문제 직후 오선택 방지

	RefreshVRTexts();
}

void ACoverPawn::Answer(int32 OptionIndex)
{
	if (bAnswered || bSessionOver || bWaitingNext)
	{
		return;
	}
	if (!CurrentQuiz.Options.IsValidIndex(OptionIndex))
	{
		return;
	}

	ChosenIndex = OptionIndex;
	SelectedIndex = OptionIndex;
	bAnswered = true;

	const bool bCorrect = (OptionIndex == CurrentQuiz.Correct);
	LastResult = FCoverResult();
	LastResult.Outcome = bCorrect ? ECoverOutcome::Covered : ECoverOutcome::WrongBase;
	if (bCorrect)
	{
		++SuccessCount;
	}

	++TrialIndex;
	bWaitingNext = true;
	IntervalTimer = IntervalBetweenTrials;

	UE_LOG(LogMotionBase, Log, TEXT("[Cover] %s / %s → 선택 %d (정답 %d) %s"),
		*CurrentQuiz.Situation, *CurrentQuiz.Role, OptionIndex, CurrentQuiz.Correct,
		bCorrect ? TEXT("O") : TEXT("X"));

	RefreshVRTexts();
}

void ACoverPawn::EndSession()
{
	bSessionOver = true;
	RefreshVRTexts();
}

// ── 입력 핸들러 ──

void ACoverPawn::SelectPrev()
{
	if (bAnswered || bSessionOver) { return; }
	const int32 N = GetOptionCount();
	if (N > 0) { SelectedIndex = (SelectedIndex - 1 + N) % N; }
}

void ACoverPawn::SelectNext()
{
	if (bAnswered || bSessionOver) { return; }
	const int32 N = GetOptionCount();
	if (N > 0) { SelectedIndex = (SelectedIndex + 1) % N; }
}

void ACoverPawn::ConfirmSelection() { Answer(SelectedIndex); }
void ACoverPawn::PickOption0() { Answer(0); }
void ACoverPawn::PickOption1() { Answer(1); }
void ACoverPawn::PickOption2() { Answer(2); }
void ACoverPawn::PickOption3() { Answer(3); }

void ACoverPawn::ReturnToModeSelect()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(AModeSelectHUD::StaticClass());
	}
	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->ReturnToModeSelect();
	}
}

// ── 매 프레임 ──

void ACoverPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bVR)
	{
		// 뒤로가기 — 컨트롤러를 위로 들고 유지하면 모드 선택으로 복귀.
		if (PointerController)
		{
			bool bExit = false;
			ExitGesture.Update(PointerController->GetForwardVector(), PointerController->IsTracked(), DeltaSeconds, bExit);
			if (bExit)
			{
				ReturnToModeSelect();
				return; // 폰이 곧 교체된다 — 이 프레임 종료.
			}
		}

		UpdateVRMenu(DeltaSeconds);
	}

	// 다음 문제 대기.
	if (bWaitingNext && !bSessionOver)
	{
		IntervalTimer -= DeltaSeconds;
		if (IntervalTimer <= 0.0f)
		{
			bWaitingNext = false;
			if (TrialIndex >= TotalTrials) { EndSession(); }
			else { SpawnNextTrial(); }
		}
	}
}

// ── VR 인메뉴 ──

void ACoverPawn::InitVRMenu()
{
	if (!VrPanel) { return; }
	if (!bVR) { VrPanel->HideAll(); return; }
	RefreshVRTexts();
}

int32 ACoverPawn::PickHoveredCard() const
{
	if (!PointerController || !PointerController->IsTracked() || !VrPanel)
	{
		return INDEX_NONE;
	}
	const FVector Origin = PointerController->GetComponentLocation();
	const FVector Aim    = PointerController->GetForwardVector();
	const float   CosThresh = FMath::Cos(FMath::DegreesToRadians(DwellAngleDeg));

	int32 Best = INDEX_NONE;
	float BestCos = CosThresh;

	const int32 N = GetOptionCount();
	for (int32 i = 0; i < N; ++i)
	{
		UTextRenderComponent* Row = VrPanel->GetRowText(i);
		if (!Row) { continue; }
		const FVector Dir = (Row->GetComponentLocation() - Origin).GetSafeNormal();
		const float C = FVector::DotProduct(Aim, Dir);
		if (C > BestCos) { BestCos = C; Best = i; }
	}
	return Best;
}

void ACoverPawn::UpdateVRMenu(float DeltaSeconds)
{
	if (VrCooldown > 0.0f) { VrCooldown = FMath::Max(0.0f, VrCooldown - DeltaSeconds); }

	// 포인터 광선.
	if (PointerController && PointerController->IsTracked() && GetWorld())
	{
		const FVector O = PointerController->GetComponentLocation();
		const FVector E = O + PointerController->GetForwardVector() * (MenuDistanceCm + 60.0f);
		DrawDebugLine(GetWorld(), O, E, FColor(80, 200, 255), false, -1.0f, 0, 0.4f);
	}

	// 답을 냈거나 대기 중이면 겨눔 비활성.
	const bool bCanPick = !bAnswered && !bSessionOver && !bWaitingNext && VrCooldown <= 0.0f;
	const int32 Hover = bCanPick ? PickHoveredCard() : INDEX_NONE;

	if (Hover != VrHoverIndex)
	{
		VrHoverIndex = Hover;
		VrDwellTimer = 0.0f;
	}

	if (Hover != INDEX_NONE)
	{
		SelectedIndex = Hover;
		VrDwellTimer += DeltaSeconds;
		if (VrDwellTimer >= DwellTimeSec)
		{
			VrHoverIndex = INDEX_NONE;
			VrDwellTimer = 0.0f;
			Answer(Hover);
			return;
		}
	}
	else
	{
		VrDwellTimer = 0.0f;
	}

	RefreshVRTexts();
}

void ACoverPawn::RefreshVRTexts()
{
	if (!bVR || !VrPanel) { return; }

	// 제목 = 상황·역할 (또는 종료 집계). 3D 텍스트는 영어로 표기.
	const FString Head = bSessionOver
		? FString::Printf(TEXT("Session over   %d / %d correct"), SuccessCount, TotalTrials)
		: FString::Printf(TEXT("[%s]   You: %s  -  which base to back up?"),
			*CurrentQuiz.Situation, *CurrentQuiz.Role);
	VrPanel->SetTitle(Head, FColor(228, 233, 244));

	const int32 N = GetOptionCount();
	const float Progress = (DwellTimeSec > 0.0f)
		? FMath::Clamp(VrDwellTimer / DwellTimeSec, 0.0f, 1.0f) : 0.0f;

	if (bSessionOver)
	{
		VrPanel->HideRowsFrom(0);
	}
	else
	{
		for (int32 i = 0; i < N; ++i)
		{
			FString Label = FString::Printf(TEXT("%d. %s"), i + 1, *CurrentQuiz.Options[i]);
			FColor Color(228, 233, 244);

			if (bAnswered)
			{
				// 정답=초록, 내가 고른 오답=빨강.
				if (i == CurrentQuiz.Correct) { Color = FColor(90, 220, 110); Label += TEXT("  (correct)"); }
				else if (i == ChosenIndex)    { Color = FColor(230, 90, 90);  Label += TEXT("  (your pick)"); }
				else                          { Color = FColor(120, 124, 134); }
			}
			else if (VrHoverIndex == i)
			{
				// 겨누는 중 — 진행바 + 앰버.
				const int32 Cells = 6;
				const int32 Filled = FMath::Clamp(FMath::RoundToInt(Progress * Cells), 0, Cells);
				Label += FString::Printf(TEXT("   [%s%s]"),
					*FString::ChrN(Filled, TEXT('=')), *FString::ChrN(Cells - Filled, TEXT('.')));
				Color = FColor(255, 190, 90);
			}
			VrPanel->SetRow(i, Label, Color);
		}
		VrPanel->HideRowsFrom(N);
	}

	// 결과·해설·힌트 → 푸터 (영어).
	if (bSessionOver)
	{
		VrPanel->SetFooter(TEXT("Raise the controller up to return to menu"), FColor(150, 156, 168));
	}
	else if (bAnswered)
	{
		const bool bCorrect = (LastResult.Outcome == ECoverOutcome::Covered);
		VrPanel->SetFooter((bCorrect ? TEXT("Correct!  ") : TEXT("Wrong.  ")) + CurrentQuiz.Explain,
			bCorrect ? FColor(90, 220, 110) : FColor(230, 130, 90));
	}
	else
	{
		VrPanel->SetFooter(TEXT("Aim an option with the controller and hold to pick"), FColor(150, 156, 168));
	}

	// 힌트: 뒤로가기 안내 (컨트롤러를 위로 드는 중이면 진행바).
	if (ExitGesture.IsHolding())
	{
		VrPanel->SetHint(FString::Printf(TEXT("Raise controller to exit  %s"), *ExitGesture.ProgressBar()),
			FColor(255, 190, 90));
	}
	else
	{
		VrPanel->SetHint(TEXT("Raise the controller up to exit to menu"), FColor(110, 116, 128));
	}
}

// ── HUD(평면) 접근자 ──

FString ACoverPawn::GetOptionText(int32 Index) const
{
	return CurrentQuiz.Options.IsValidIndex(Index) ? CurrentQuiz.Options[Index] : FString();
}

bool ACoverPawn::GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const
{
	if (bSessionOver)
	{
		OutText  = FString::Printf(TEXT("Session over!  %d / %d correct"), SuccessCount, TotalTrials);
		OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f);
		return true;
	}
	if (!bAnswered)
	{
		return false;
	}

	switch (LastResult.Outcome)
	{
	case ECoverOutcome::Covered:
		OutText = TEXT("Correct!");  OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case ECoverOutcome::WrongBase:
		OutText = TEXT("Wrong");     OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	default:
		return false;
	}
}
