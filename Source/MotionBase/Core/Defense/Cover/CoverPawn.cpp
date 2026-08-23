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

	// 3D 선택 카드는 카메라 앞에 부착 (항상 시야에 보이게). 퀴즈 패널이라 헤드락 허용.
	MenuRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MenuRoot"));
	MenuRoot->SetupAttachment(Camera);
	MenuRoot->SetRelativeLocation(FVector(MenuDistanceCm, 0.0f, -10.0f));

	// 한글 폰트 (Content/Fonts/KRFont). 없으면 엔진 기본으로 폴백(한글 깨질 수 있음).
	static ConstructorHelpers::FObjectFinder<UFont> KRFontFinder(TEXT("/Game/Fonts/KRFont.KRFont"));
	UFont* MenuFont = KRFontFinder.Succeeded() ? KRFontFinder.Object : nullptr;

	auto MakeText = [this, MenuFont](const TCHAR* Name, float WorldSize) -> UTextRenderComponent*
	{
		UTextRenderComponent* T = CreateDefaultSubobject<UTextRenderComponent>(Name);
		T->SetupAttachment(MenuRoot);
		T->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // 카메라를 향하게
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(WorldSize);
		if (MenuFont) { T->SetFont(MenuFont); }
		T->SetVisibility(false);
		return T;
	};

	VrSituationText = MakeText(TEXT("VrSituation"), 8.0f);
	VrSituationText->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));

	for (int32 i = 0; i < MaxOptions; ++i)
	{
		UTextRenderComponent* Opt = MakeText(*FString::Printf(TEXT("VrOption%d"), i), 10.0f);
		Opt->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f - i * 24.0f));
		VrOptionTexts.Add(Opt);
	}

	VrResultText = MakeText(TEXT("VrResult"), 9.0f);
	VrResultText->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));
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
		Q.Options = { TEXT("1루"), TEXT("2루"), TEXT("3루"), TEXT("홈") };
		Q.Correct = Correct;
		Q.Explain = Explain;
		QuizPool.Add(Q);
	};

	// Correct: 0=1루, 1=2루, 2=3루, 3=홈. (야구 백업/커버 정석)
	Add(TEXT("무사 주자 없음 · 내야 땅볼, 1루로 송구"), TEXT("포수"), 0,
		TEXT("주자 없는 내야 땅볼의 1루 송구는 포수가 1루 뒤를 백업한다."));
	Add(TEXT("우익수 앞 안타 · 주자 3루까지, 3루로 송구"), TEXT("투수"), 2,
		TEXT("외야에서 3루로 오는 송구는 투수가 3루 뒤를 백업한다."));
	Add(TEXT("중견수 뒤 장타 · 주자 홈 쇄도, 홈으로 송구"), TEXT("투수"), 3,
		TEXT("외야에서 홈으로 오는 송구는 투수가 홈(포수) 뒤를 백업한다."));
	Add(TEXT("기습 번트 · 1루수가 앞으로 나와 처리"), TEXT("2루수"), 0,
		TEXT("1루수가 타구 처리로 베이스를 비우면 2루수가 1루를 커버한다."));
	Add(TEXT("번트 · 3루수가 앞으로 나와 처리"), TEXT("유격수"), 2,
		TEXT("3루수가 타구 처리로 비운 3루를 유격수가 커버한다."));
	Add(TEXT("1루 주자 도루 · 포수가 2루로 송구"), TEXT("중견수"), 1,
		TEXT("2루로 오는 송구는 중견수가 2루 뒤를 백업한다(악송구 대비)."));
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
	if (MenuRoot) { MenuRoot->SetRelativeLocation(FVector(MenuDistanceCm, 0.0f, -10.0f)); }

	const bool bShow = bVR;
	if (VrSituationText) { VrSituationText->SetVisibility(bShow); }
	if (VrResultText)    { VrResultText->SetVisibility(bShow); }
	for (UTextRenderComponent* Opt : VrOptionTexts)
	{
		if (Opt) { Opt->SetVisibility(false); }
	}
	if (bShow) { RefreshVRTexts(); }
}

int32 ACoverPawn::PickHoveredCard() const
{
	if (!PointerController || !PointerController->IsTracked())
	{
		return INDEX_NONE;
	}
	const FVector Origin = PointerController->GetComponentLocation();
	const FVector Aim    = PointerController->GetForwardVector();
	const float   CosThresh = FMath::Cos(FMath::DegreesToRadians(DwellAngleDeg));

	int32 Best = INDEX_NONE;
	float BestCos = CosThresh;

	const int32 N = GetOptionCount();
	for (int32 i = 0; i < N && i < VrOptionTexts.Num(); ++i)
	{
		if (!VrOptionTexts[i]) { continue; }
		const FVector Dir = (VrOptionTexts[i]->GetComponentLocation() - Origin).GetSafeNormal();
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
	if (!bVR) { return; }

	if (VrSituationText)
	{
		const FString Head = bSessionOver
			? FString::Printf(TEXT("훈련 종료   성공 %d / %d"), SuccessCount, TotalTrials)
			: FString::Printf(TEXT("[%s]  당신은 %s — 어디를 백업?"),
				*CurrentQuiz.Situation, *CurrentQuiz.Role);
		VrSituationText->SetText(FText::FromString(Head));
		VrSituationText->SetTextRenderColor(FColor(228, 233, 244));
	}

	const int32 N = GetOptionCount();
	const float Progress = (DwellTimeSec > 0.0f)
		? FMath::Clamp(VrDwellTimer / DwellTimeSec, 0.0f, 1.0f) : 0.0f;

	for (int32 i = 0; i < VrOptionTexts.Num(); ++i)
	{
		UTextRenderComponent* Opt = VrOptionTexts[i];
		if (!Opt) { continue; }

		if (bSessionOver || i >= N)
		{
			Opt->SetVisibility(false);
			continue;
		}
		Opt->SetVisibility(true);

		FString Label = FString::Printf(TEXT("%d. %s"), i + 1, *CurrentQuiz.Options[i]);
		FColor Color(228, 233, 244);

		if (bAnswered)
		{
			// 정답=초록, 내가 고른 오답=빨강.
			if (i == CurrentQuiz.Correct) { Color = FColor(90, 220, 110); Label += TEXT("  (정답)"); }
			else if (i == ChosenIndex)    { Color = FColor(230, 90, 90);  Label += TEXT("  (내 선택)"); }
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

		Opt->SetText(FText::FromString(Label));
		Opt->SetTextRenderColor(Color);
	}

	if (VrResultText)
	{
		FString RText;
		FColor  RColor(150, 156, 168);
		if (bSessionOver)
		{
			RText = TEXT("M: 모드 선택으로");
		}
		else if (bAnswered)
		{
			const bool bCorrect = (LastResult.Outcome == ECoverOutcome::Covered);
			RText = (bCorrect ? TEXT("정답!  ") : TEXT("오답  ")) + CurrentQuiz.Explain;
			RColor = bCorrect ? FColor(90, 220, 110) : FColor(230, 130, 90);
		}
		else
		{
			RText = bVR ? TEXT("컨트롤러로 보기를 겨누고 잠시 유지") : TEXT("숫자키 1~4 로 선택");
		}
		VrResultText->SetText(FText::FromString(RText));
		VrResultText->SetTextRenderColor(RColor);
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
		OutText  = FString::Printf(TEXT("훈련 종료!  성공 %d / %d"), SuccessCount, TotalTrials);
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
		OutText = TEXT("정답!");   OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case ECoverOutcome::WrongBase:
		OutText = TEXT("오답");    OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	default:
		return false;
	}
}
