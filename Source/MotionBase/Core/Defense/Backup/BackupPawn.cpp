#include "Core/Defense/Backup/BackupPawn.h"
#include "Core/Defense/Backup/BackupPlaybook.h"
#include "Core/Defense/Backup/BackupJudge.h"
#include "Core/Defense/Backup/BackupHUD.h"
#include "Core/Defense/CatchBall/CatchBall.h"
#include "Core/MotionBaseGameMode.h"
#include "Core/ModeManager.h"
#include "MotionBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "UI/ModeSelectHUD.h"
#include "UI/VRInfoPanel.h"
#include "AI/DrillCatalog.h"
#include "AI/AIFeedbackService.h"
#include "Analysis/WeaknessDetector.h"
#include "Scoring/ScoringService.h"

namespace
{
	// TextRender 는 자동 줄바꿈이 없다 → 글자수로 하드 랩 (다른 폰들과 동일 패턴).
	TArray<FString> WrapBackupPanel(const FString& In, int32 MaxCharsPerLine, int32 MaxLines)
	{
		TArray<FString> Lines;
		int32 i = 0;
		const int32 Len = In.Len();
		while (i < Len && Lines.Num() < MaxLines)
		{
			Lines.Add(In.Mid(i, MaxCharsPerLine));
			i += MaxCharsPerLine;
		}
		if (i < Len && Lines.Num() > 0)
		{
			Lines.Last().Append(TEXT(" …"));
		}
		return Lines;
	}

	// ── 코스메틱 타구 방향/깊이 근사 (판정과 무관 — 실제 판정은 FBaseballField 의 정확한
	// 존 기하를 쓴다. 이건 순전히 "타구가 대략 그쪽으로 날아가는 걸 보여주기" 위한 것). ──
	float ApproxZoneBearingDeg(EBattedBallZone Zone)
	{
		switch (Zone)
		{
		case EBattedBallZone::InfieldLeft:  return -20.0f;
		case EBattedBallZone::InfieldRight: return 20.0f;
		case EBattedBallZone::BuntFirst:    return 15.0f;
		case EBattedBallZone::BuntThird:    return -15.0f;
		case EBattedBallZone::LeftLine:     return -44.0f;
		case EBattedBallZone::LeftField:    return -30.0f;
		case EBattedBallZone::LeftCenter:   return -11.0f;
		case EBattedBallZone::RightCenter:  return 11.0f;
		case EBattedBallZone::RightField:   return 30.0f;
		case EBattedBallZone::RightLine:    return 44.0f;
		case EBattedBallZone::Backstop:     return 0.0f;
		case EBattedBallZone::InfieldMiddle:
		case EBattedBallZone::Center:
		default:                            return 0.0f;
		}
	}

	float ApproxZoneDepthCm(EBattedBallZone Zone, const FBaseballField& Field)
	{
		switch (Zone)
		{
		case EBattedBallZone::InfieldLeft:
		case EBattedBallZone::InfieldMiddle:
		case EBattedBallZone::InfieldRight:
		case EBattedBallZone::BuntFirst:
		case EBattedBallZone::BuntThird:
			return 2200.0f;
		case EBattedBallZone::Backstop:
			return 300.0f;
		default:
			return Field.OutfieldDepthCm * 0.75f; // 외야 계열은 전부 외야 깊이 근처로.
		}
	}
}

ABackupPawn::ABackupPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	SetRootComponent(Capsule);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Capsule);
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = false;

	MoveController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MoveController"));
	MoveController->SetupAttachment(Capsule);
	MoveController->MotionSource = FName(TEXT("Right"));

	VrPanel = CreateDefaultSubobject<UVRInfoPanel>(TEXT("VrPanel"));
	VrPanel->SetupAttachment(Capsule);
	VrPanel->SetPlacement(UVRInfoPanel::DefaultDistanceCm, 70.0f);
}

void ABackupPawn::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

	bVR = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
	if (bVR)
	{
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);
	}

	// 나가기 제스처 — 이동 게이트(트리거)를 쥔 채 팔을 크게 휘두를 수 있어 기본값보다 조인다.
	// 시행이 진행 중인 동안(Live)에는 Tick 에서 진행을 동결한다.
	ExitGesture.UpThreshold = 0.85f;
	ExitGesture.HoldSec = 2.0f;

	if (VrPanel)
	{
		VrPanel->BuildPanel();
		if (!bVR)
		{
			VrPanel->HideAll();
		}
		else
		{
			VrPanel->SetStatusCompact();
			VrPanel->ShowBackCard(TEXT("EXIT - aim here & hold"), FColor(255, 190, 90));
		}
	}

	// 포지션은 메뉴에서 ModeManager 에 지정해 둔 값을 읽는다 — ActiveDifficulty/ActiveStance 와
	// 같은 패턴(폰 스폰 시 커스텀 파라미터를 못 넘기는 구조라 세션 상태를 경유한다).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* MM = GI->GetSubsystem<UModeManager>())
		{
			Position = MM->GetActiveFieldPosition();
		}
	}

	PlayTable = UBackupPlaybook::BuildPlayTable();
	RuleTable = UBackupPlaybook::BuildRuleTable();
	RefillBags();

	// 첫 배치 — VR 은 HMD 포즈가 아직 정확하지 않을 수 있지만, 없는 것보단 낫다.
	// 실제 배치는 SpawnNextTrial 이 매 시행(첫 시행 포함) 다시 잡는다.
	SnapToFieldingSpot();

	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &ABackupPawn::HandleCoachingReady);

	StartSession();
}

void ABackupPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FlushSessionToSave();

	if (IsValid(ActiveBall))
	{
		ActiveBall->Destroy();
		ActiveBall = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

// ── 공 속도 조절 (측정이 아니라 연출 손잡이) ──

void ABackupPawn::SpeedDown()
{
	BallSpeedScale = FMath::Clamp(BallSpeedScale - BallSpeedStep, 0.4f, 2.0f);
}

void ABackupPawn::SpeedUp()
{
	BallSpeedScale = FMath::Clamp(BallSpeedScale + BallSpeedStep, 0.4f, 2.0f);
}

void ABackupPawn::SpawnFlavorBall()
{
	// 도루·포일은 타구 자체가 없다 — 공을 띄우지 않는다.
	if (CurrentTrial.Play.BallKind == EBattedBallKind::Steal || CurrentTrial.Play.BallKind == EBattedBallKind::WildPitch)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float BearingDeg = ApproxZoneBearingDeg(CurrentTrial.Play.BallZone);
	const float DepthCm = ApproxZoneDepthCm(CurrentTrial.Play.BallZone, Field);
	const FVector Home = Field.GetBaseLocation(EBaseType::Home);
	const FVector TargetXY = Home + FRotator(0.0f, BearingDeg, 0.0f).RotateVector(FVector(DepthCm, 0.0f, 0.0f));
	const FVector Target(TargetXY.X, TargetXY.Y, Field.GroundZ);

	// 발사 지점 = 홈 플레이트 임팩트 높이 근방(타자가 방금 친 자리).
	const FVector Launch = Home + FVector(0.0f, 0.0f, 120.0f);
	const float Flight = FMath::Max(BallFlightSec / FMath::Max(BallSpeedScale, 0.1f), 0.3f);
	const float G = FMath::Abs(World->GetGravityZ());

	const FVector ToTarget = Target - Launch;
	const FVector Horiz(ToTarget.X, ToTarget.Y, 0.0f);
	const float VHoriz = Horiz.Size() / Flight;
	const float VZ = (ToTarget.Z / Flight) + 0.5f * G * Flight;
	const FVector Velocity = Horiz.GetSafeNormal() * VHoriz + FVector(0.0f, 0.0f, VZ);

	TSubclassOf<ACatchBall> Cls = BallClass ? BallClass : ACatchBall::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveBall = World->SpawnActor<ACatchBall>(Cls, Launch, FRotator::ZeroRotator, Params);
	if (ActiveBall)
	{
		ActiveBall->SetGroundZ(Field.GroundZ);
		ActiveBall->SetTrailVisible(true);
		ActiveBall->Launch(Velocity);
	}
}

void ABackupPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed,  this, &ABackupPawn::OnFwdPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ABackupPawn::OnFwdReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed,  this, &ABackupPawn::OnBackPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ABackupPawn::OnBackReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed,  this, &ABackupPawn::OnLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ABackupPawn::OnLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed,  this, &ABackupPawn::OnRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ABackupPawn::OnRightReleased);

	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ABackupPawn::ReturnToModeSelect);

	// 타구 속도 조절 — [ 느리게 / ] 빠르게. 다음 시행부터 반영된다 (사용자가 요청한
	// "타구가 날아오는 걸 느리게 볼 수 있게"를 만족시키는 손잡이 — CatchBallPawn 과 동일 패턴).
	PlayerInputComponent->BindKey(EKeys::LeftBracket,  IE_Pressed, this, &ABackupPawn::SpeedDown);
	PlayerInputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ABackupPawn::SpeedUp);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(ABackupHUD::StaticClass());
	}
}

// ── 배치 ──

void ABackupPawn::SnapToFieldingSpot()
{
	const FVector Spot = Field.GetFieldingSpot(Position);
	const FVector Home = Field.GetBaseLocation(EBaseType::Home);
	const float DesiredYaw = (Home - Spot).Rotation().Yaw; // 항상 홈 플레이트를 본다.
	const float GroundW = FloorZ();

	if (bVR && Camera)
	{
		// 폰 루트는 트래킹 원점일 뿐이라, 머리(카메라)가 실제로 수비 위치에 서고 홈을
		// 보도록 루트를 역산해서 놓는다. 회전 먼저, 위치는 회전된 오프셋에 의존하니 나중에.
		const FRotator CamRel = Camera->GetRelativeRotation();
		const FVector CamRelLoc = Camera->GetRelativeLocation();
		const FRotator RootRot(0.0f, FRotator::NormalizeAxis(DesiredYaw - CamRel.Yaw), 0.0f);
		SetActorRotation(RootRot);
		const FVector CamOffsetWS = RootRot.RotateVector(FVector(CamRelLoc.X, CamRelLoc.Y, 0.0f));
		SetActorLocation(FVector(Spot.X, Spot.Y, GroundW) - CamOffsetWS);
	}
	else
	{
		SetActorRotation(FRotator(0.0f, DesiredYaw, 0.0f));
		SetActorLocation(FVector(Spot.X, Spot.Y, GroundW));
	}
}

float ABackupPawn::FloorZ() const
{
	// VR: SetTrackingOrigin(Stage) → 바닥이 곧 폰 루트 Z. PC: 루트가 캡슐 중심이라 반높이 아래.
	return bVR ? Field.GroundZ : (Field.GroundZ + 88.0f);
}

FVector ABackupPawn::GetPlayerXY() const
{
	// VR: 머리(카메라)의 XY = 플레이어가 실제로 서 있는 자리 (폰 루트는 트래킹 원점일 뿐).
	// PC: 루트가 곧 몸.
	const FVector Src = (bVR && Camera) ? Camera->GetComponentLocation() : GetActorLocation();
	return FVector(Src.X, Src.Y, FloorZ());
}

// ── 세션 진행 ──

void ABackupPawn::StartSession()
{
	TrialIndex = 0;
	SuccessCount = 0;
	bSessionOver = false;
	bWaitingNext = false;

	SessionResults.Reset();
	LastDrills.Reset();
	CoachingText.Reset();
	bAwaitingCoaching = false;

	Phase = EBackupPhase::Done; // 첫 시행 전까지는 "진행 중"이 아니다.

	// 첫 시행은 한 박자 뒤에 — VR 은 BeginPlay 시점에 HMD 포즈가 아직 안 들어와
	// 배치를 정확히 못 잡는다. 플레이어에게도 준비할 틈이 된다.
	bWaitingNext = true;
	IntervalTimer = FMath::Max(FirstTrialDelaySec, 0.2f);
}

void ABackupPawn::RefillBags()
{
	EligibleBag.Reset();
	AllBag.Reset();
	for (int32 i = 0; i < PlayTable.Num(); ++i)
	{
		AllBag.Add(i);
		if (UBackupPlaybook::IsEligibleForPosition(RuleTable, PlayTable[i], Position))
		{
			EligibleBag.Add(i);
		}
	}
}

FBackupPlay ABackupPawn::DrawNextPlay()
{
	if (PlayTable.Num() == 0)
	{
		return FBackupPlay();
	}

	// HoldTrialFraction 확률로 전체 주머니(=Hold 로 풀릴 수도 있는 플레이 포함)에서 뽑는다.
	// "정답이 절대 제자리가 아니다"라는 편법을 막으려면 이게 꼭 필요하다 (설계 노트 참고).
	const bool bWantHold = (FMath::FRand() < HoldTrialFraction);

	if (EligibleBag.Num() == 0 || AllBag.Num() == 0)
	{
		RefillBags();
	}

	TArray<int32>& Bag = bWantHold ? AllBag : EligibleBag;
	if (Bag.Num() == 0)
	{
		return PlayTable[0]; // 저작 데이터가 비정상적으로 빈 극단적 경우의 안전망.
	}

	const int32 Pick = FMath::RandRange(0, Bag.Num() - 1);
	const int32 PlayIdx = Bag[Pick];
	Bag.RemoveAtSwap(Pick);
	return PlayTable[PlayIdx];
}

void ABackupPawn::SpawnNextTrial()
{
	// 실내 드리프트가 시행마다 누적된다 — 매번 다시 스냅한다 (설계 노트).
	SnapToFieldingSpot();

	if (IsValid(ActiveBall))
	{
		ActiveBall->Destroy();
	}
	ActiveBall = nullptr;

	const FBackupPlay Play = DrawNextPlay();
	CurrentTrial = UBackupPlaybook::BuildTrial(Field, RuleTable, Position, Play);
	SpawnFlavorBall(); // 코스메틱 — 판정(큐 시점부터 이미 시작됨)에는 영향 없음.

	Phase = EBackupPhase::Live;
	CueTimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	CueXY = GetPlayerXY();

	// 선출발 방지 — 큐 시점에 이미 게이트가 눌려 있으면, 한 번 뗄 때까지 이동을 무효화한다.
	bGateLatchedAtCue = IsGateCurrentlyHeld();
	bGateEverReleased = !bGateLatchedAtCue;

	GateElapsedSec = 0.0f;
	DisplacedCm = 0.0f;
	AccumulatedPathCm = 0.0f;
	bHeadingCommitted = false;
	bDirectionCorrect = false;
	bAmbiguousDirection = false;
	bFalseStart = false;
	PendingDecisionTimeSec = -1.0f;
	TrialSamples.Reset();

	UE_LOG(LogMotionBase, Log, TEXT("[Backup] #%d %s @ %s : %s / %s → %s"),
		TrialIndex + 1, *UBackupPlaybook::PositionName(Position), *Play.PlayId.ToString(),
		*Play.Situation, *Play.RunnerText,
		CurrentTrial.bIsHoldTrial ? TEXT("Hold") : *CurrentTrial.CorrectZone.Explain);
}

void ABackupPawn::FinishTrial(EBackupOutcome Outcome)
{
	Phase = EBackupPhase::Done;

	const float Elapsed = (CueTimeSec >= 0.0f && GetWorld()) ? (GetWorld()->GetTimeSeconds() - CueTimeSec) : 0.0f;

	FBackupResult Result;
	Result.Outcome = Outcome;
	Result.bDirectionCorrect = bDirectionCorrect;
	Result.bAmbiguousDirection = bAmbiguousDirection;
	Result.DecisionTimeSec = bHeadingCommitted ? PendingDecisionTimeSec : -1.0f;
	Result.ArrivalTimeSec = Elapsed;
	Result.bKeyScenario = CurrentTrial.CorrectZone.bKeyScenario;

	const float StraightLine = FVector::Dist2D(CueXY, CurrentTrial.CorrectZone.RepresentativePoint());
	Result.PathEfficiency = FBackupJudge::PathEfficiency(StraightLine, AccumulatedPathCm);

	LastResult = Result;
	bHasResult = true;
	SessionResults.Add(Result);

	if (Result.IsSuccess())
	{
		++SuccessCount;
	}

	// 시도 1건 = 기록 1건 (수비는 성공/실패 + 원시 측정값만 남긴다 — 3축 모델 없음).
	if (UModeManager* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		TMap<FName, float> Details;
		Details.Add(TEXT("Position"), static_cast<float>(static_cast<uint8>(Position)));
		Details.Add(TEXT("DecisionSec"), Result.DecisionTimeSec);
		Details.Add(TEXT("PathEfficiency"), Result.PathEfficiency);
		Details.Add(TEXT("KeyScenario"), Result.bKeyScenario ? 1.0f : 0.0f);
		Details.Add(TEXT("HoldTrial"), CurrentTrial.bIsHoldTrial ? 1.0f : 0.0f);
		MM->RecordResult(UScoringService::ScoreDefenseAttempt(Result.IsSuccess(), Details));
	}

	UE_LOG(LogMotionBase, Log, TEXT("[Backup] 판정: %s  판단 %.2fs  경로효율 %.0f%%"),
		*UEnum::GetValueAsString(Outcome), Result.DecisionTimeSec, Result.PathEfficiency * 100.0f);

	++TrialIndex;
	if (TrialIndex >= TotalTrials)
	{
		EndSession();
	}
	else
	{
		bWaitingNext = true;
		IntervalTimer = IntervalBetweenTrials;
	}
}

void ABackupPawn::EndSession()
{
	bSessionOver = true;
	RequestBackupFeedback();
}

// ── AI 판단 코칭 ──

FWeaknessReport ABackupPawn::BuildBackupReport() const
{
	FWeaknessReport R;
	R.Mode = EGameModeId::Defense;
	R.AttemptCount = SessionResults.Num();
	R.bUncalibrated = true; // 판단 시간·경로효율 기준값은 아직 실측 미보정.

	if (SessionResults.Num() == 0)
	{
		return R; // bValid=false
	}

	const int32 N = SessionResults.Num();
	int32 Correct = 0, KeyN = 0, KeyCorrect = 0;
	float SumDecision = 0.0f; int32 DecisionN = 0;
	float SumCorrectDecision = 0.0f; int32 CorrectDecisionN = 0;
	float SumPathEff = 0.0f; int32 PathEffN = 0;

	for (const FBackupResult& Res : SessionResults)
	{
		if (Res.DecisionTimeSec >= 0.0f) { SumDecision += Res.DecisionTimeSec; ++DecisionN; }
		if (Res.IsSuccess())
		{
			++Correct;
			if (Res.DecisionTimeSec >= 0.0f) { SumCorrectDecision += Res.DecisionTimeSec; ++CorrectDecisionN; }
		}
		if (Res.PathEfficiency >= 0.0f) { SumPathEff += Res.PathEfficiency; ++PathEffN; }
		if (Res.bKeyScenario) { ++KeyN; if (Res.IsSuccess()) { ++KeyCorrect; } }
	}

	R.ContactCount = Correct;
	R.bValid = true;

	const float CorrectRate = static_cast<float>(Correct) / N;

	// 문턱은 타격·포구·송구와 같은 모드 공통 상수를 쓴다.
	auto AddWeakness = [&R](EWeaknessAxis Axis, float Score, const FString& Evidence)
	{
		FWeakness W;
		W.Axis = Axis;
		W.Score = FMath::Clamp(Score, 0.0f, 1.0f);
		W.Severity = 1.0f - W.Score;
		W.Evidence = Evidence;
		if (W.Severity >= UWeaknessDetector::MinReportSeverity) { R.Weaknesses.Add(W); }
	};

	// ① 백업 판단: 정답률 그대로.
	AddWeakness(EWeaknessAxis::BackupJudgment, CorrectRate,
		FString::Printf(TEXT("correct %d/%d backup calls"), Correct, N));

	// ② 판단 속도: **맞힌 시행만** 기준으로 본다 — 빨리 틀리는 사람이 만점 받는 걸 막는다
	// (퀴즈 시절부터 이어지는 규칙, CoverPawn 의 동일 로직 참고).
	if (CorrectDecisionN > 0)
	{
		const float AvgCorrectDecision = SumCorrectDecision / CorrectDecisionN;
		AddWeakness(EWeaknessAxis::DecisionSpeed,
			FMath::Clamp(Field.TargetDecisionSec / FMath::Max(AvgCorrectDecision, 0.01f), 0.0f, 1.0f),
			FString::Printf(TEXT("average %.2f s to start moving on correct calls (target %.2f s)"),
				AvgCorrectDecision, Field.TargetDecisionSec));
	}

	// ③ 경로 효율: 직선거리/실제이동거리. 이동 속도(MoveSpeedCms)와 무관한 진짜 "헤맴" 지표.
	// FootSpeed 를 재사용하지 않는다 — 다리를 안 쓰는데 LLM 에 "사다리 스텝"이 전달되면
	// Backup 시스템 프롬프트의 컨디셔닝 금지 규칙과 정면 충돌한다.
	if (PathEffN > 0)
	{
		const float AvgPathEff = SumPathEff / PathEffN;
		AddWeakness(EWeaknessAxis::RouteEfficiency, AvgPathEff,
			FString::Printf(TEXT("average route efficiency %.0f%% (straight-line distance / distance actually covered)"),
				AvgPathEff * 100.0f));
	}

	if (DecisionN > 0)
	{
		R.Notes.Add(FString::Printf(TEXT("Average decision time over all cases: %.2f s"), SumDecision / DecisionN));
	}
	if (KeyN > 0)
	{
		R.Notes.Add(FString::Printf(
			TEXT("Key scenarios (wide CF coverage, 2B covering first when 1B is pulled off the bag): %d/%d"),
			KeyCorrect, KeyN));
	}
	R.Notes.Add(FString::Printf(TEXT("Position: %s"), *UBackupPlaybook::PositionName(Position)));

	R.Weaknesses.Sort([](const FWeakness& A, const FWeakness& B) { return A.Severity > B.Severity; });
	return R;
}

void ABackupPawn::FlushSessionToSave()
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

void ABackupPawn::RequestBackupFeedback()
{
	const FWeaknessReport Report = BuildBackupReport();

	// 과거 "BackupMove" 세션만 골라 만성 추세를 본다. 판단 훈련의 추세에 포구 반응속도가
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

	UE_LOG(LogMotionBase, Log, TEXT("[Backup] 코칭 요청: 정답 %d/%d, 약점 %d개"),
		SuccessCount, TotalTrials, Report.Weaknesses.Num());
}

void ABackupPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	CoachingText = Text;
	UE_LOG(LogMotionBase, Log, TEXT("[Backup] AI 코칭 %s: %s"),
		bSuccess ? TEXT("수신") : TEXT("실패"), *Text);
}

// ── 측정 지표 접근자 ──

float ABackupPawn::GetLiveDecisionSec() const
{
	if (Phase != EBackupPhase::Live || bHeadingCommitted || CueTimeSec < 0.0f)
	{
		return -1.0f;
	}
	const UWorld* World = GetWorld();
	return World ? (World->GetTimeSeconds() - CueTimeSec) : -1.0f;
}

float ABackupPawn::GetAverageDecisionSec() const
{
	float Sum = 0.0f; int32 N = 0;
	for (const FBackupResult& R : SessionResults)
	{
		if (R.DecisionTimeSec >= 0.0f) { Sum += R.DecisionTimeSec; ++N; }
	}
	return (N > 0) ? (Sum / N) : -1.0f;
}

float ABackupPawn::GetAveragePathEfficiency() const
{
	float Sum = 0.0f; int32 N = 0;
	for (const FBackupResult& R : SessionResults)
	{
		if (R.PathEfficiency >= 0.0f) { Sum += R.PathEfficiency; ++N; }
	}
	return (N > 0) ? (Sum / N) : -1.0f;
}

bool ABackupPawn::GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const
{
	if (Phase == EBackupPhase::Live)
	{
		return false; // 진행 중엔 직전 결과 잔상을 숨긴다.
	}
	if (bSessionOver)
	{
		OutText = FString::Printf(TEXT("Session over!  %d / %d correct"), SuccessCount, TotalTrials);
		OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f);
		return true;
	}
	if (!bHasResult)
	{
		return false;
	}

	switch (LastResult.Outcome)
	{
	case EBackupOutcome::Covered:
		OutText = TEXT("Covered!");     OutColor = FLinearColor(0.40f, 0.85f, 0.45f, 1.0f); return true;
	case EBackupOutcome::TooSlow:
		OutText = TEXT("Too slow");     OutColor = FLinearColor(0.95f, 0.55f, 0.30f, 1.0f); return true;
	case EBackupOutcome::WrongZone:
		OutText = TEXT("Wrong spot");   OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	case EBackupOutcome::NoStart:
		OutText = TEXT("No reaction");  OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	case EBackupOutcome::FalseStart:
		OutText = TEXT("False start");  OutColor = FLinearColor(0.90f, 0.35f, 0.35f, 1.0f); return true;
	default:
		return false;
	}
}

// ── 매 프레임 ──

bool ABackupPawn::IsGateCurrentlyHeld() const
{
	if (!bVR)
	{
		return bMoveFwd || bMoveBack || bMoveLeft || bMoveRight;
	}

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (MoveController)
		{
			const FName Hand = MoveController->MotionSource;
			const TCHAR* Side = (Hand == FName(TEXT("Left"))) ? TEXT("Left") : TEXT("Right");
			const float Trig = PC->GetInputAnalogKeyState(
				FKey(*FString::Printf(TEXT("MotionController_%s_Trigger"), Side)));
			return MoveController->IsTracked() && (Trig >= GateTriggerThreshold);
		}
	}
	return false;
}

void ABackupPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bVR)
	{
		// 패널을 플레이어 정면에 고정 배치(swimming 제거).
		if (VrPanel && Camera)
		{
			VrPanel->UpdateComfortAnchor(Camera, UVRInfoPanel::DefaultDistanceCm, 70.0f, /*RecenterDeg=*/55.0f);
		}

		if (MoveController)
		{
			// 시행이 진행 중(Live)일 땐 나가기 제스처를 동결한다 — 백업 이동 중 팔이
			// 위로 향할 수 있는데(스틱 조작 자세) 그게 나가기로 오인되면 세션이 날아간다.
			const bool bGestureAllowed = (Phase != EBackupPhase::Live);
			bool bExit = false;
			ExitGesture.Update(MoveController->GetForwardVector(),
				MoveController->IsTracked(), bGestureAllowed, DeltaSeconds, bExit);
			if (bExit)
			{
				ReturnToModeSelect();
				return; // 폰이 곧 교체된다 — 이 프레임 종료.
			}

			if (VrPanel)
			{
				const bool bAim = MoveController->IsTracked() && bGestureAllowed;
				if (VrPanel->UpdateBackDwell(MoveController->GetComponentLocation(),
					MoveController->GetForwardVector(), bAim, /*DwellSec=*/1.6f, /*AngleDeg=*/8.0f, DeltaSeconds))
				{
					ReturnToModeSelect();
					return;
				}
			}
		}
	}

	if (Phase == EBackupPhase::Live && !bSessionOver)
	{
		TickTrial(DeltaSeconds);
	}

	if (bWaitingNext && !bSessionOver)
	{
		IntervalTimer -= DeltaSeconds;
		if (IntervalTimer <= 0.0f)
		{
			bWaitingNext = false;
			SpawnNextTrial();
		}
	}

	// 정지 프레임(rest frame) — VR 멀미 완화. 폰 루트 기준 고정된 링 + 기둥 2개.
	// 눈 기준 정지된 참조물이 있으면 vection 멀미가 크게 줄어든다 (설계 노트).
	if (bVR && GetWorld())
	{
		const FVector Base = GetActorLocation();
		DrawDebugCircle(GetWorld(), Base + FVector(0, 0, 2.0f), 80.0f, 24, FColor(90, 90, 100),
			false, -1.0f, 0, 1.5f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		DrawDebugLine(GetWorld(), Base + FVector(60, 0, 0), Base + FVector(60, 0, 140), FColor(90, 90, 100), false, -1.0f, 0, 1.2f);
		DrawDebugLine(GetWorld(), Base + FVector(-60, 0, 0), Base + FVector(-60, 0, 140), FColor(90, 90, 100), false, -1.0f, 0, 1.2f);
	}

	if (Phase == EBackupPhase::Live)
	{
		DrawZones();
	}

	if (bVR)
	{
		RefreshVrPanel();
	}
}

void ABackupPawn::TryCommitHeading()
{
	if (!FBackupJudge::ShouldCommitHeading(DisplacedCm, GateElapsedSec, HeadingCommitDistanceCm, HeadingCommitTimeSec))
	{
		return;
	}

	const FVector NewPos = GetPlayerXY();
	const FVector2D HeadingDir = FVector2D(NewPos.X - CueXY.X, NewPos.Y - CueXY.Y).GetSafeNormal();

	TArray<FVector2D> CandidateDirs;
	CandidateDirs.Reserve(CurrentTrial.CandidateZones.Num());
	for (const FBackupZone& Z : CurrentTrial.CandidateZones)
	{
		const FVector Rep = Z.RepresentativePoint();
		CandidateDirs.Add(FVector2D(Rep.X - CueXY.X, Rep.Y - CueXY.Y).GetSafeNormal());
	}

	float MarginDeg = 0.0f;
	const int32 NearestIdx = FBackupJudge::NearestCandidate(HeadingDir, CandidateDirs, MarginDeg);

	bHeadingCommitted = true;
	bAmbiguousDirection = (MarginDeg < DirectionMarginDeg);
	bDirectionCorrect = (NearestIdx != INDEX_NONE) && (NearestIdx == CurrentTrial.CorrectCandidateIndex);

	// 측정 지표: 판단(이동 시작)까지 걸린 시간 = 큐부터 지금까지의 벽시계 시간.
	// VR 게이트 입력은 PC 즉시 키 입력보다 구조적으로 늦으므로 그만큼 뺀다 —
	// 안 빼면 VR 성적이 PC 대비 구조적으로 나빠져 두 입력을 비교할 수 없다.
	const float Elapsed = (CueTimeSec >= 0.0f && GetWorld()) ? (GetWorld()->GetTimeSeconds() - CueTimeSec) : 0.0f;
	const float Bias = bVR ? VRInputLatencyBiasSec : 0.0f;
	PendingDecisionTimeSec = FMath::Max(Elapsed - Bias, 0.0f);
}

void ABackupPawn::TickTrial(float DeltaSeconds)
{
	// ── 게이트 + 축 입력 ──
	bool bGateHeld = IsGateCurrentlyHeld();
	FVector2D MoveDir = FVector2D::ZeroVector; // 이미 월드 XY 축으로 정렬된 단위(이하) 벡터.

	if (bGateHeld)
	{
		if (bVR)
		{
			if (const APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				const FName Hand = MoveController ? MoveController->MotionSource : FName(TEXT("Right"));
				const TCHAR* Side = (Hand == FName(TEXT("Left"))) ? TEXT("Left") : TEXT("Right");

				// Vive 트랙패드는 두 이름 중 하나로 잡힌다 — 둘 다 읽어 절댓값이 큰 쪽을 쓴다
				// (다른 VR 폰들과 동일 패턴).
				auto ReadAxis = [PC, Side](const TCHAR* Generic, const TCHAR* ViveName) -> float
				{
					const float A = PC->GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("MotionController_%s_%s"), Side, Generic)));
					const float B = PC->GetInputAnalogKeyState(FKey(*FString::Printf(TEXT("Vive_%s_%s"), Side, ViveName)));
					return (FMath::Abs(B) > FMath::Abs(A)) ? B : A;
				};

				float AxisX = ReadAxis(TEXT("Thumbstick_X"), TEXT("Trackpad_X"));
				float AxisY = ReadAxis(TEXT("Thumbstick_Y"), TEXT("Trackpad_Y"));

				// 게이트를 막 쥔 첫 프레임은 트랙패드 최초 접촉 노이즈가 있을 수 있어
				// 더 높은 문턱을 쓴다 (그 뒤로는 일반 데드존).
				const float Floor = (GateElapsedSec <= KINDA_SMALL_NUMBER) ? FirstFrameAxisFloor : StickDeadzone;
				if (FMath::Abs(AxisX) < Floor) { AxisX = 0.0f; }
				if (FMath::Abs(AxisY) < Floor) { AxisY = 0.0f; }

				FVector2D Local(AxisY, AxisX); // 앞뒤=X, 좌우=Y (다른 VR 폰들과 동일 축 관례).
				if (Local.SizeSquared() > 1.0f) { Local.Normalize(); }

				// 머리가 보는 방향 기준 → 월드 축으로 변환.
				if (Camera && !Local.IsNearlyZero())
				{
					const float HeadYaw = Camera->GetComponentRotation().Yaw;
					const FVector Rotated = FRotator(0.0f, HeadYaw, 0.0f).RotateVector(FVector(Local.X, Local.Y, 0.0f));
					MoveDir = FVector2D(Rotated.X, Rotated.Y);
				}
			}
		}
		else
		{
			FVector Local = FVector::ZeroVector;
			if (bMoveFwd)   Local.X += 1.0f;
			if (bMoveBack)  Local.X -= 1.0f;
			if (bMoveRight) Local.Y += 1.0f;
			if (bMoveLeft)  Local.Y -= 1.0f;
			if (!Local.IsNearlyZero())
			{
				Local = Local.GetSafeNormal();
				// ⚠️ 포지션마다 폰이 다른 방향(항상 홈을 보도록, SnapToFieldingSpot)으로 회전돼
				// 있다 — CatchBallPawn 처럼 월드 축을 그대로 쓰면 예를 들어 우익수 자리에서
				// 'W' 를 눌러도 "내가 보는 방향"이 아니라 엉뚱한 세계 축으로 움직인다.
				// 폰의 현재 회전(=바라보는 방향) 기준으로 돌려 월드 축으로 변환한다.
				const float FacingYaw = GetActorRotation().Yaw;
				const FVector Rotated = FRotator(0.0f, FacingYaw, 0.0f).RotateVector(Local);
				MoveDir = FVector2D(Rotated.X, Rotated.Y);
			}
		}
	}

	// ── 선출발 방지: 큐 시점에 이미 게이트가 눌려 있었으면, 한 번 뗄 때까지 무효 ──
	if (bGateLatchedAtCue)
	{
		if (!bGateHeld)
		{
			bGateLatchedAtCue = false;
			bGateEverReleased = true;
		}
		bGateHeld = false;
		MoveDir = FVector2D::ZeroVector;
	}

	// ── Hold 시행: 게이트가 한 번이라도 걸리면 즉시 성급한 출발 ──
	if (CurrentTrial.bIsHoldTrial)
	{
		if (bGateHeld && !MoveDir.IsNearlyZero())
		{
			FinishTrial(EBackupOutcome::FalseStart);
			return;
		}
	}
	else if (bGateHeld)
	{
		GateElapsedSec += DeltaSeconds;
	}

	// ── 이동 적용 ──
	if (bGateHeld && !MoveDir.IsNearlyZero())
	{
		const float SpeedCms = bVR ? Field.MoveSpeedCms : PCMoveSpeedCms;
		const FVector2D DeltaXY = MoveDir * SpeedCms * DeltaSeconds;
		AddActorWorldOffset(FVector(DeltaXY.X, DeltaXY.Y, 0.0f), false);
		AccumulatedPathCm += DeltaXY.Size();
	}

	const FVector NewPos = GetPlayerXY();
	DisplacedCm = FVector::Dist2D(CueXY, NewPos);

	// 위치 샘플 기록 — Data/BodyPose.h(FBodyPoseSample)의 첫 소비자.
	const float Elapsed = (CueTimeSec >= 0.0f && GetWorld()) ? (GetWorld()->GetTimeSeconds() - CueTimeSec) : 0.0f;
	FBodyPoseSample Sample;
	Sample.TimeSec = Elapsed;
	Sample.RootPosition = NewPos;
	TrialSamples.Add(Sample);

	// ── 1단계: 방향 커밋 시도 (Hold 시행은 방향 개념이 없다) ──
	if (!bHeadingCommitted && !CurrentTrial.bIsHoldTrial)
	{
		TryCommitHeading();
	}

	// ── 2단계: 도착/유지 판정 ──
	const float EffectiveTimeLimit = CurrentTrial.bIsHoldTrial ? HoldWindowSec : CurrentTrial.TimeLimitSec;

	if (!CurrentTrial.bIsHoldTrial && CurrentTrial.CorrectZone.Contains(NewPos))
	{
		FinishTrial(EBackupOutcome::Covered);
		return;
	}

	if (Elapsed >= EffectiveTimeLimit)
	{
		if (CurrentTrial.bIsHoldTrial)
		{
			FinishTrial(EBackupOutcome::Covered); // 안 움직이고 버텼다 = 정답.
		}
		else if (DisplacedCm < 20.0f)
		{
			FinishTrial(EBackupOutcome::NoStart); // 사실상 안 움직였다.
		}
		else
		{
			FinishTrial((bHeadingCommitted && bDirectionCorrect) ? EBackupOutcome::TooSlow : EBackupOutcome::WrongZone);
		}
	}
}

void ABackupPawn::DrawZones() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 i = 0; i < CurrentTrial.CandidateZones.Num(); ++i)
	{
		const FBackupZone& Z = CurrentTrial.CandidateZones[i];
		const bool bCorrect = (i == CurrentTrial.CorrectCandidateIndex);
		const FColor Col = bCorrect ? FColor(90, 220, 110) : FColor(90, 96, 110);
		const float Thickness = bCorrect ? 3.0f : 1.2f;

		if (Z.Role == EBackupRole::CutoffRelay)
		{
			DrawDebugLine(World, Z.SegmentA + FVector(0, 0, 2), Z.SegmentB + FVector(0, 0, 2), Col, false, -1.0f, 0, Thickness);
		}
		else
		{
			DrawDebugCircle(World, Z.Center + FVector(0, 0, 2), Z.RadiusCm, 32, Col, false, -1.0f, 0, Thickness,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
	}
}

// ── VR 상태 패널 ──

void ABackupPawn::RefreshVrPanel()
{
	if (!VrPanel) { return; }

	if (bSessionOver)
	{
		VrPanel->SetTitle(
			FString::Printf(TEXT("AI judgment tips    (Correct %d / %d)"), SuccessCount, TotalTrials),
			FColor(150, 210, 255));

		// ⚠️ 컴팩트 상태 패널(SetStatusCompact)은 행이 4줄을 넘으면 푸터·힌트와 겹친다.
		constexpr int32 MaxContentRows = 4;
		int32 Row = 0;

		const float AvgD = GetAverageDecisionSec();
		const float AvgP = GetAveragePathEfficiency();
		if (Row < MaxContentRows && (AvgD >= 0.0f || AvgP >= 0.0f))
		{
			VrPanel->SetRow(Row++, FString::Printf(TEXT("avg decision %.2fs   route %.0f%%"),
				FMath::Max(AvgD, 0.0f), FMath::Max(AvgP, 0.0f) * 100.0f), FColor(150, 200, 255));
		}

		for (const FString& L : WrapBackupPanel(CoachingText, 30, 2))
		{
			if (Row >= MaxContentRows) { break; }
			VrPanel->SetRow(Row++, L, FColor(228, 233, 244));
		}
		for (const FTrainingDrill& D : LastDrills)
		{
			if (Row >= MaxContentRows) { break; }
			VrPanel->SetRow(Row++, FString::Printf(TEXT("- %s"), *D.Name), FColor(255, 200, 120));
		}
		VrPanel->HideRowsFrom(Row);

		VrPanel->SetFooter(bAwaitingCoaching ? TEXT("Waiting for AI...") : TEXT("Recommended training"),
			FColor(150, 156, 168));
		VrPanel->SetHint(TEXT("Raise controller = menu"), FColor(110, 116, 128));
		return;
	}

	VrPanel->SetTitle(
		FString::Printf(TEXT("%s   %d / %d   Correct %d"),
			*UBackupPlaybook::PositionName(Position), GetTrialNumber(), TotalTrials, SuccessCount),
		FColor(228, 233, 244));

	VrPanel->SetRow(0, CurrentTrial.Play.Situation, FColor(150, 200, 255));
	VrPanel->SetRow(1, CurrentTrial.Play.RunnerText, FColor(150, 156, 168));

	const float Live = GetLiveDecisionSec();
	if (Live >= 0.0f)
	{
		VrPanel->SetRow(2, FString::Printf(TEXT("deciding...  %.1fs"), Live),
			(Live > Field.TargetDecisionSec) ? FColor(230, 130, 90) : FColor(90, 220, 110));
		VrPanel->HideRowsFrom(3);
	}
	else
	{
		VrPanel->HideRowsFrom(2);
	}

	FString Outcome; FLinearColor OColor;
	if (GetLastOutcomeText(Outcome, OColor))
	{
		const FString Explain = GetLastExplainText();
		VrPanel->SetFooter(Explain.IsEmpty() ? Outcome : (Outcome + TEXT("  ") + Explain), OColor.ToFColor(true));
	}
	else
	{
		VrPanel->SetFooter(TEXT("Hold the move button and go to your backup spot"), FColor(150, 156, 168));
	}

	if (ExitGesture.IsHolding())
	{
		VrPanel->SetHint(FString::Printf(TEXT("Raise controller to exit  %s"), *ExitGesture.ProgressBar()),
			FColor(255, 190, 90));
	}
	else
	{
		VrPanel->SetHint(TEXT("Hold the trigger and push the stick to move to your backup zone"),
			FColor(110, 116, 128));
	}
}

void ABackupPawn::ReturnToModeSelect()
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
