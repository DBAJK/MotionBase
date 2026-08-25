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
#include "AI/DrillCatalog.h"
#include "AI/AIFeedbackService.h"
#include "Analysis/WeaknessDetector.h"
#include "Scoring/ScoringService.h"
#include "Core/ModeManager.h"

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

	// AI 판단 코칭 서비스 (키가 없으면 요청 시 조용히 생략됨).
	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &ACoverPawn::HandleCoachingReady);

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
	// 한 케이스 = 포지션 + 타구 방향·종류 + 주자 상황 → 정답 백업 zone.
	// 보기는 케이스마다 다르다 — 백업은 "몇 루로 가느냐"가 아니라 "무슨 행동을 하느냐"라서
	// 1B/2B/3B/Home 네 글자로는 '뒤 백업'과 '베이스 커버'와 '중계'를 구분할 수 없다.
	auto Add = [this](const TCHAR* Sit, const TCHAR* Runners, const TCHAR* InRole,
		const TCHAR* O0, const TCHAR* O1, const TCHAR* O2, const TCHAR* O3,
		int32 Correct, const TCHAR* Explain, bool bKey = false)
	{
		FBackupQuiz Q;
		Q.Situation = Sit;
		Q.Runners = Runners;
		Q.Role = InRole;
		Q.Options = { O0, O1, O2, O3 };
		Q.Correct = Correct;
		Q.Explain = Explain;
		Q.bKeyScenario = bKey;
		QuizPool.Add(Q);
	};

	// ── 투수 (P) — 핵심: 홈 뒤 / 3루 뒤 택1. 동시에 둘 다 갈 수 없다는 점이 이 종목의 난이도 ──
	Add(TEXT("Single to RF, relay throw going HOME"), TEXT("Runner on 2nd, scoring"), TEXT("P"),
		TEXT("Back up behind HOME"), TEXT("Back up behind 3B"), TEXT("Cover 1B"), TEXT("Cut off in the infield"),
		0, TEXT("The throw is going home, so the pitcher backs up the plate. You cannot cover both home and third - follow the throw."), true);

	Add(TEXT("Single to LF, relay throw going to 3B"), TEXT("Runner on 1st, taking third"), TEXT("P"),
		TEXT("Back up behind HOME"), TEXT("Back up behind 3B"), TEXT("Cover 2B"), TEXT("Cut off in the infield"),
		1, TEXT("The throw is going to third, so the pitcher backs up 3B. Read the ball's direction first, then pick one."), true);

	Add(TEXT("Bunt down the 1B line, 1B charges in"), TEXT("Runner on 1st"), TEXT("P"),
		TEXT("Cover 1B"), TEXT("Back up behind 2B"), TEXT("Back up behind HOME"), TEXT("Stay on the mound"),
		0, TEXT("When the first baseman fields the bunt, the pitcher covers first base."));

	Add(TEXT("Wild pitch, ball rolls to the backstop"), TEXT("Runner on 3rd"), TEXT("P"),
		TEXT("Cover HOME plate"), TEXT("Back up behind 3B"), TEXT("Chase the ball"), TEXT("Back up behind 1B"),
		0, TEXT("The catcher chases the ball, so the pitcher covers home to take the tag throw."));

	// ── 포수 (C) ──
	Add(TEXT("Routine grounder to SS, throw to 1B"), TEXT("Bases empty"), TEXT("C"),
		TEXT("Back up behind 1B"), TEXT("Stay at HOME"), TEXT("Back up behind 3B"), TEXT("Cover 2B"),
		0, TEXT("With nobody on, the catcher runs to back up the throw at first."));

	Add(TEXT("Grounder to 2B, throw to 1B"), TEXT("Runner on 2nd"), TEXT("C"),
		TEXT("Back up behind 1B"), TEXT("Stay at HOME"), TEXT("Cover 3B"), TEXT("Back up behind 2B"),
		1, TEXT("With a runner on second, the catcher stays home - leaving the plate lets the runner score on an overthrow."));

	// ── 1루수 (1B) ──
	Add(TEXT("Bunt in front of the plate, 1B fields it"), TEXT("Runner on 1st"), TEXT("1B"),
		TEXT("Field it and throw, 2B covers 1st"), TEXT("Field it and run to 1st yourself"), TEXT("Let the pitcher take it"), TEXT("Back up behind 2B"),
		0, TEXT("Once you leave the bag the second baseman covers first - you field and throw, then return after the play."), true);

	Add(TEXT("Double into the RF corner, relay coming in"), TEXT("Runner on 1st"), TEXT("1B"),
		TEXT("Line up as the cut-off man"), TEXT("Back up behind 3B"), TEXT("Cover 1B"), TEXT("Back up behind HOME"),
		0, TEXT("On outfield relays the first baseman is the cut-off man toward home."));

	// ── 2루수 (2B) ──
	Add(TEXT("Grounder to 3B, throw to 1B"), TEXT("Bases empty"), TEXT("2B"),
		TEXT("Back up behind 1B"), TEXT("Cover 2B"), TEXT("Cover 1B"), TEXT("Back up behind 3B"),
		0, TEXT("On a throw from the left side the second baseman backs up first base."));

	Add(TEXT("Runner steals, catcher throws to 2B"), TEXT("Runner on 1st, right-handed hitter"), TEXT("2B"),
		TEXT("Cover 2B for the tag"), TEXT("Back up behind 2B"), TEXT("Cover 1B"), TEXT("Back up behind 3B"),
		0, TEXT("With a right-handed hitter the second baseman covers the bag and the shortstop backs up."));

	Add(TEXT("Slow roller wide of 1B, the 1B leaves the bag to field it"), TEXT("Runner on 2nd"), TEXT("2B"),
		TEXT("Sprint over and cover 1B"), TEXT("Cover 2B"), TEXT("Back up behind 1B"), TEXT("Line up as cut-off"),
		0, TEXT("When the first baseman is pulled off the bag, covering first is the second baseman's job."), true);

	// ── 유격수 (SS) ──
	Add(TEXT("Single to LF, throw coming to 2B"), TEXT("Runner on 1st, going to 2nd"), TEXT("SS"),
		TEXT("Cover 2B for the tag"), TEXT("Back up behind 2B"), TEXT("Cover 3B"), TEXT("Back up behind 3B"),
		0, TEXT("On a ball to left the shortstop takes the bag at second; the center fielder backs it up."));

	Add(TEXT("Deep fly to left-center, ball drops in"), TEXT("Runner on 1st"), TEXT("SS"),
		TEXT("Go out as the cut-off man"), TEXT("Cover 2B"), TEXT("Back up behind 3B"), TEXT("Cover 3B"),
		0, TEXT("The shortstop is the cut-off for balls hit to left and center."));

	// ── 3루수 (3B) ──
	Add(TEXT("Single to CF, relay throw going to 3B"), TEXT("Runner on 1st, taking third"), TEXT("3B"),
		TEXT("Cover 3B for the tag"), TEXT("Back up behind 3B"), TEXT("Cover HOME"), TEXT("Line up as cut-off"),
		0, TEXT("The third baseman takes the bag; the pitcher is the one who backs up behind it."));

	// ── 외야수 — 백업의 핵심 축 ──
	Add(TEXT("Grounder to SS, throw to 1B"), TEXT("Bases empty"), TEXT("RF"),
		TEXT("Back up behind 1B"), TEXT("Hold your position"), TEXT("Back up behind 2B"), TEXT("Move to right-center"),
		0, TEXT("The right fielder backs up every throw to first - an overthrow there is his ball."));

	Add(TEXT("Runner steals, catcher throws to 2B"), TEXT("Runner on 1st"), TEXT("CF"),
		TEXT("Back up behind 2B"), TEXT("Hold your position"), TEXT("Back up behind 3B"), TEXT("Move to left-center"),
		0, TEXT("The center fielder backs up second base on steals and on relay throws."), true);

	Add(TEXT("Ball skips past the right fielder toward the corner"), TEXT("Runner on 1st"), TEXT("CF"),
		TEXT("Sprint over to back up RF"), TEXT("Hold center field"), TEXT("Back up behind 2B"), TEXT("Line up as cut-off"),
		0, TEXT("The center fielder is the outfield captain - he backs up both corner outfielders as well."), true);

	Add(TEXT("Single to CF, relay throw going to 3B"), TEXT("Runner on 1st"), TEXT("LF"),
		TEXT("Back up behind 3B"), TEXT("Hold left field"), TEXT("Cover 3B"), TEXT("Back up behind 2B"),
		0, TEXT("The left fielder backs up throws to third and covers the left-center gap."));
}

FBackupQuiz ACoverPawn::DrawQuiz()
{
	if (QuizPool.Num() == 0)
	{
		return FBackupQuiz();
	}

	// 주머니가 비면 다시 채운다 — 한 세션 안에서 같은 문제가 반복될 확률을 낮춘다.
	if (RemainingQuiz.Num() == 0)
	{
		RemainingQuiz.Reserve(QuizPool.Num());
		for (int32 i = 0; i < QuizPool.Num(); ++i) { RemainingQuiz.Add(i); }
	}

	const int32 Pick = FMath::RandRange(0, RemainingQuiz.Num() - 1);
	const int32 QuizIdx = RemainingQuiz[Pick];
	RemainingQuiz.RemoveAtSwap(Pick);
	return QuizPool[QuizIdx];
}

// ── 세션 ──

void ACoverPawn::StartSession()
{
	TrialIndex = 0;
	SuccessCount = 0;
	bSessionOver = false;
	bWaitingNext = false;
	IntervalTimer = 0.0f;

	RemainingQuiz.Reset();
	SessionResults.Reset();
	LastDrills.Reset();
	CoachingText.Reset();
	bAwaitingCoaching = false;

	SpawnNextTrial();
}

void ACoverPawn::SpawnNextTrial()
{
	if (QuizPool.Num() == 0)
	{
		EndSession();
		return;
	}

	CurrentQuiz = DrawQuiz();
	SelectedIndex = 0;
	ChosenIndex = -1;
	bAnswered = false;
	VrHoverIndex = INDEX_NONE;
	VrDwellTimer = 0.0f;
	VrCooldown = 0.5f; // 새 문제 직후 오선택 방지

	// 판단 시간 측정 시작 — 문제가 화면에 뜬 이 순간이 기준점이다.
	QuizShownTimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f;

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
	LastResult.bKeyScenario = CurrentQuiz.bKeyScenario;

	// 측정 지표 ②: 판단까지 걸린 시간.
	// VR 드웰 선택은 "겨눈 뒤 1.5초 유지"라는 고정 지연이 항상 얹히므로 그만큼 뺀다.
	// 안 빼면 VR 성적이 PC 대비 구조적으로 나빠져 두 입력이 비교 불가능해진다.
	if (QuizShownTimeSec >= 0.0f && GetWorld())
	{
		const float Raw = GetWorld()->GetTimeSeconds() - QuizShownTimeSec;
		const float DwellBias = bVR ? DwellTimeSec : 0.0f;
		LastResult.DecisionTimeSec = FMath::Max(Raw - DwellBias, 0.0f);
	}

	SessionResults.Add(LastResult);

	// 시도 1건 = 기록 1건 (정답 여부 + 판단 시간).
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		TMap<FName, float> Details;
		Details.Add(TEXT("DecisionSec"),  LastResult.DecisionTimeSec);
		Details.Add(TEXT("KeyScenario"),  LastResult.bKeyScenario ? 1.0f : 0.0f);
		MM->RecordResult(UScoringService::ScoreDefenseAttempt(bCorrect, Details));
	}

	if (bCorrect)
	{
		++SuccessCount;
	}

	++TrialIndex;
	bWaitingNext = true;
	IntervalTimer = IntervalBetweenTrials;

	UE_LOG(LogMotionBase, Log, TEXT("[Cover] %s / %s / %s → 선택 %d (정답 %d) %s  판단 %.2fs"),
		*CurrentQuiz.Situation, *CurrentQuiz.Runners, *CurrentQuiz.Role,
		OptionIndex, CurrentQuiz.Correct,
		bCorrect ? TEXT("O") : TEXT("X"), LastResult.DecisionTimeSec);

	RefreshVRTexts();
}

float ACoverPawn::GetLiveDecisionSec() const
{
	if (bAnswered || bSessionOver || QuizShownTimeSec < 0.0f) { return -1.0f; }
	const UWorld* World = GetWorld();
	return World ? (World->GetTimeSeconds() - QuizShownTimeSec) : -1.0f;
}

float ACoverPawn::GetAverageDecisionSec() const
{
	if (SessionResults.Num() == 0) { return -1.0f; }
	float Sum = 0.0f;
	for (const FCoverResult& R : SessionResults) { Sum += R.DecisionTimeSec; }
	return Sum / SessionResults.Num();
}

// ── AI 판단 코칭 ──

FWeaknessReport ACoverPawn::BuildBackupReport() const
{
	FWeaknessReport R;
	R.Mode = EGameModeId::Defense;
	R.AttemptCount = SessionResults.Num();
	R.bUncalibrated = true; // 판단 시간 기준값은 아직 실측 미보정.

	if (SessionResults.Num() == 0)
	{
		return R; // bValid=false
	}

	const int32 N = SessionResults.Num();
	int32 Correct = 0, KeyN = 0, KeyCorrect = 0;
	float SumDecision = 0.0f;
	float SumCorrectDecision = 0.0f;
	int32 CorrectDecisionN = 0;

	for (const FCoverResult& Res : SessionResults)
	{
		SumDecision += Res.DecisionTimeSec;
		if (Res.IsSuccess())
		{
			++Correct;
			SumCorrectDecision += Res.DecisionTimeSec;
			++CorrectDecisionN;
		}
		if (Res.bKeyScenario)
		{
			++KeyN;
			if (Res.IsSuccess()) { ++KeyCorrect; }
		}
	}

	R.ContactCount = Correct;
	R.bValid = true;

	const float CorrectRate = static_cast<float>(Correct) / N;
	const float AvgDecision = SumDecision / N;

	auto AddWeakness = [&R](EWeaknessAxis Axis, float Score, const FString& Evidence)
	{
		FWeakness W;
		W.Axis = Axis;
		W.Score = FMath::Clamp(Score, 0.0f, 1.0f);
		W.Severity = 1.0f - W.Score;
		W.Evidence = Evidence;
		// 문턱은 타격·포구·송구와 같은 모드 공통 상수를 쓴다.
		if (W.Severity >= UWeaknessDetector::MinReportSeverity) { R.Weaknesses.Add(W); }
	};

	// ① 백업 판단: 정답률 그대로.
	AddWeakness(EWeaknessAxis::BackupJudgment, CorrectRate,
		FString::Printf(TEXT("correct %d/%d backup calls"), Correct, N));

	// ② 판단 속도: **맞힌 문제만** 기준으로 본다.
	//    틀린 문제를 빨리 찍은 것은 빠른 판단이 아니라 그냥 오답이다 — 섞으면
	//    "빨리 틀리는 사람"이 판단 속도 만점을 받는다.
	if (CorrectDecisionN > 0)
	{
		const float AvgCorrectDecision = SumCorrectDecision / CorrectDecisionN;
		AddWeakness(EWeaknessAxis::DecisionSpeed,
			FMath::Clamp(TargetDecisionSec / FMath::Max(AvgCorrectDecision, 0.01f), 0.0f, 1.0f),
			FString::Printf(TEXT("average %.1f s to decide on correct calls (target %.1f s)"),
				AvgCorrectDecision, TargetDecisionSec));
	}

	R.Notes.Add(FString::Printf(TEXT("Average decision time over all cases: %.1f s"), AvgDecision));
	if (KeyN > 0)
	{
		R.Notes.Add(FString::Printf(
			TEXT("Key scenarios (pitcher home-or-third, center fielder wide backup, 2B covering first): %d/%d"),
			KeyCorrect, KeyN));
	}

	R.Weaknesses.Sort([](const FWeakness& A, const FWeakness& B) { return A.Severity > B.Severity; });
	return R;
}

void ACoverPawn::FlushSessionToSave()
{
	UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr;
	if (!MM)
	{
		return;
	}
	MM->FinalizeSession(
		UScoringService::ScoreDefenseSession(SuccessCount, SessionResults.Num()),
		BuildBackupReport());
}

void ACoverPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FlushSessionToSave();
	Super::EndPlay(EndPlayReason);
}

void ACoverPawn::RequestBackupFeedback()
{
	const FWeaknessReport Report = BuildBackupReport();

	// 과거 "Backup" 세션만 골라 만성 추세를 본다. 판단 훈련의 추세에 포구 반응속도가
	// 섞이면 "판단이 만성 약점"인지 "몸이 느린 건지"를 구분할 수 없게 된다.
	FChronicWeaknessReport Chronic;
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		Chronic = UWeaknessDetector::AnalyzeTrend(
			MM->GetHistory(), EGameModeId::Defense, 5, UModeManager::GetDefenseDrillIdName(2));
	}
	LastDrills = UDrillCatalog::RecommendWithHistory(Report, Chronic, 3);

	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText = TEXT("Requesting AI coaching...");
		bAwaitingCoaching = true;
		FeedbackService->RequestBackupCoaching(Report, LastDrills);
	}
	else
	{
		bAwaitingCoaching = false;
		CoachingText = TEXT("AI coaching not configured (Config/Secrets.ini)");
	}

	UE_LOG(LogMotionBase, Log, TEXT("[Cover] 코칭 요청: 정답 %d/%d, 평균 판단 %.1fs, 약점 %d개"),
		SuccessCount, TotalTrials, GetAverageDecisionSec(), Report.Weaknesses.Num());
}

void ACoverPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	CoachingText = Text;
	UE_LOG(LogMotionBase, Log, TEXT("[Cover] AI 코칭 %s: %s"),
		bSuccess ? TEXT("수신") : TEXT("실패"), *Text);
	RefreshVRTexts();
}

void ACoverPawn::EndSession()
{
	bSessionOver = true;
	RequestBackupFeedback();   // 판단 성적으로 판단 훈련을 처방한다 (체력 드릴이 아니다).
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
			// 백업 퀴즈는 보기를 겨누는 화면이라 컨트롤러가 위를 향할 일이 없다 → 항상 허용.
			bool bExit = false;
			ExitGesture.Update(PointerController->GetForwardVector(),
				PointerController->IsTracked(), /*bAllowed=*/true, DeltaSeconds, bExit);
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

	// 패널을 플레이어 정면에 고정 배치(swimming 제거·이질감 제거). 겨눔 판정이 카드
	// 위치를 쓰므로 PickHoveredCard 보다 먼저 자리를 잡는다.
	if (VrPanel && Camera)
	{
		VrPanel->UpdateComfortAnchor(Camera, MenuDistanceCm, 60.0f, /*RecenterDeg=*/55.0f);
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

	// ── 공간 연출 — 모드 선택 화면과 같은 카드/광선/링을 쓴다 (보기 선택 UI 라 규칙이 같다) ──
	if (VrPanel)
	{
		const float Progress = (DwellTimeSec > 0.0f)
			? FMath::Clamp(VrDwellTimer / DwellTimeSec, 0.0f, 1.0f) : 0.0f;
		const int32 RowCount = bSessionOver ? 0 : GetOptionCount();

		if (Camera)
		{
			// 눈높이는 **패널 자신의 높이** 기준으로 재야 한다 (이 폰의 패널은 캡슐 중심 +60cm 에 있다).
			VrPanel->ApplyCurvedLayout(
				Camera->GetRelativeLocation().Z - VrPanel->GetRelativeLocation().Z, MenuDistanceCm);
		}
		VrPanel->TickHoverAnim(DeltaSeconds, VrHoverIndex, RowCount);
		VrPanel->DrawChrome(RowCount, VrHoverIndex, Progress, false);

		if (PointerController && PointerController->IsTracked())
		{
			VrPanel->DrawPointerRay(
				PointerController->GetComponentLocation(),
				PointerController->GetForwardVector(),
				Progress, VrHoverIndex != INDEX_NONE);
		}
	}
}

void ACoverPawn::RefreshVRTexts()
{
	if (!bVR || !VrPanel) { return; }

	// 제목 = 상황·역할 (또는 종료 집계). 3D 텍스트는 영어로 표기.
	const FString Head = bSessionOver
		? FString::Printf(TEXT("Session over   %d / %d correct"), SuccessCount, TotalTrials)
		: FString::Printf(TEXT("You: %s      %s      [%s]"),
			*CurrentQuiz.Role, *CurrentQuiz.Situation, *CurrentQuiz.Runners);
	VrPanel->SetTitle(Head, FColor(228, 233, 244));

	// 세션 종료 → AI 판단 코칭 + 추천 훈련.
	if (bSessionOver)
	{
		int32 Row = 0;
		const float AvgD = GetAverageDecisionSec();
		if (AvgD >= 0.0f)
		{
			VrPanel->SetRow(Row++, FString::Printf(TEXT("avg decision %.1fs   (target %.1fs)"),
				AvgD, TargetDecisionSec), FColor(150, 200, 255));
		}
		// 코칭 문장 — TextRender 는 자동 줄바꿈이 없어 글자수로 자른다.
		{
			int32 i = 0;
			while (i < CoachingText.Len() && Row < UVRInfoPanel::MaxRows - 1)
			{
				VrPanel->SetRow(Row++, CoachingText.Mid(i, 34), FColor(228, 233, 244));
				i += 34;
			}
		}
		for (const FTrainingDrill& D : LastDrills)
		{
			if (Row >= UVRInfoPanel::MaxRows) { break; }
			VrPanel->SetRow(Row++, FString::Printf(TEXT("- %s"), *D.Name), FColor(255, 200, 120));
		}
		VrPanel->HideRowsFrom(Row);

		VrPanel->SetFooter(bAwaitingCoaching ? TEXT("Waiting for AI...") : TEXT("Recommended judgment training"),
			FColor(150, 156, 168));
		VrPanel->SetHint(TEXT("Raise the controller up to exit to menu"), FColor(110, 116, 128));
		return;
	}

	const int32 N = GetOptionCount();
	const float Progress = (DwellTimeSec > 0.0f)
		? FMath::Clamp(VrDwellTimer / DwellTimeSec, 0.0f, 1.0f) : 0.0f;

	// (세션 종료 화면은 위에서 이미 처리하고 return 했다 — 여기는 문제 진행 중만 온다.)
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

	// 결과·해설·힌트 → 푸터 (영어).
	if (bAnswered)
	{
		const bool bCorrect = (LastResult.Outcome == ECoverOutcome::Covered);
		VrPanel->SetFooter(FString::Printf(TEXT("%s(%.1fs)  %s"),
			bCorrect ? TEXT("Correct!  ") : TEXT("Wrong.  "),
			LastResult.DecisionTimeSec, *CurrentQuiz.Explain),
			bCorrect ? FColor(90, 220, 110) : FColor(230, 130, 90));
	}
	else
	{
		// 판단 시간이 흐르는 게 보여야 "빨리 결정하라"가 훈련 목표로 전달된다.
		const float Live = GetLiveDecisionSec();
		VrPanel->SetFooter(
			(Live >= 0.0f)
				? FString::Printf(TEXT("Aim an option and hold to pick        %.1fs"), Live)
				: FString(TEXT("Aim an option with the controller and hold to pick")),
			(Live > TargetDecisionSec) ? FColor(230, 130, 90) : FColor(150, 156, 168));
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
