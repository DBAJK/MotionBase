#include "Testing/SwingTestPawn.h"
#include "MotionBase.h"
#include "Analysis/SwingAnalyzer.h"
#include "Analysis/WeaknessDetector.h"
#include "Analysis/HitModel.h"
#include "Analysis/BodyMechanicsAnalyzer.h"
#include "Input/MockCameraPoseSource.h"
#include "Actors/PitchingZone.h"
#include "AI/AIFeedbackService.h"
#include "AI/DrillCatalog.h"
#include "Core/MotionBaseGameMode.h"
#include "Core/ModeManager.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ASwingTestPawn::ASwingTestPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	// 빙의는 AMotionBaseGameMode 가 모드 선택 결과에 따라 직접 넘긴다.
	// (AutoPossessPlayer 를 켜두면 시작 화면 폰과 Player0 을 두고 다툰다.)

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	// 타자 뒤에서 홈플레이트(전방 z≈100)와 날아오는 공을 함께 보는 각도.
	Camera->SetRelativeLocation(FVector(-400.0f, 0.0f, 170.0f));
	Camera->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
}

void ASwingTestPawn::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 투수를 정면 마운드 거리에 생성하고 타자 쪽을 향하게 한다.
	FActorSpawnParameters Params;
	Params.Owner = this;
	PitchingZone = World->SpawnActor<APitchingZone>(APitchingZone::StaticClass(),
		GetActorLocation(), GetActorRotation(), Params);

	if (!PitchingZone)
	{
		UE_LOG(LogMotionBase, Warning, TEXT("SwingTest: PitchingZone 생성 실패"));
		return;
	}

	const float Distance = PitchingZone->GetReleaseToPlateCm();
	const FVector Origin = GetActorLocation();
	const FVector Fwd    = GetActorForwardVector();

	// 컨택 지점 = 타자 앞 45cm·가슴 높이 110cm (VRBattingPawn 과 동일 규격).
	// 공이 몸이 아니라 앞쪽 스윙 지점으로 도착하게 한다.
	constexpr float ContactForwardCm = 45.0f;
	constexpr float ContactHeightCm  = 110.0f;
	const FVector ContactXY = Origin + Fwd * ContactForwardCm;          // Z = 지면
	const FVector MoundLocation = ContactXY + Fwd * Distance;           // 지면 높이
	PitchingZone->SetActorLocation(MoundLocation);
	// 마운드 Forward 가 컨택 지점을 수평으로 향하게.
	PitchingZone->SetActorRotation((ContactXY - MoundLocation).Rotation());
	PitchingZone->SetPlateHeightCm(ContactHeightCm);

	PitchingZone->OnPitchThrown.AddDynamic(this, &ASwingTestPawn::HandlePitchThrown);
	PitchingZone->OnPitchArrived.AddDynamic(this, &ASwingTestPawn::HandlePitchArrived);

	// 시작 화면에서 고른 난이도를 읽어 투구 파라미터에 반영한다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* ModeManager = GI->GetSubsystem<UModeManager>())
		{
			SessionDifficulty = ModeManager->GetActiveDifficulty();
			SessionStance = ModeManager->GetActiveStance();
		}
	}
	PitchingZone->ApplyDifficulty(SessionDifficulty);

	// 난이도 → 타구 판정 관대도 (점수 산식은 건드리지 않는다 — FScoringConfig 주석 참고).
	ScoringConfig.ApplyDifficulty(SessionDifficulty);

	// 동적 난이도: 과거 기록(이 모드 평균 총점)이 좋을수록 더 어렵게 시작한다.
	// 기록이 없으면(첫 플레이) 프리셋 그대로 시작 → 이후 스윙 성적으로 조정된다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* MM = GI->GetSubsystem<UModeManager>())
		{
			const FModeStats St = MM->GetModeStats(EGameModeId::Batting);
			if (St.SessionCount > 0)
			{
				PitchingZone->SeedDynamicLevel(St.AverageTotal / 100.0f);
			}
		}
	}

	// 타석에 맞춰 시점을 홈플레이트 옆으로 옮긴다.
	//   우타 = 3루 쪽(-Y), 좌타 = 1루 쪽(+Y). (배터가 서는 타석 위치)
	if (Camera)
	{
		const float SideSign = (SessionStance == EBattingStance::Left) ? 1.0f : -1.0f;
		constexpr float BatterBoxOffsetY = 76.0f; // cm — 타석 중심 오프셋
		FVector CamLoc = Camera->GetRelativeLocation();
		CamLoc.Y = SideSign * BatterBoxOffsetY;
		Camera->SetRelativeLocation(CamLoc);
	}

	// AI 코칭 서비스 (키가 없으면 요청 시 조용히 생략됨).
	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &ASwingTestPawn::HandleCoachingReady);
}

void ASwingTestPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 모드 복귀([M])·앱 종료로 폰이 사라지기 전에 진행 중이던 세션을 저장한다.
	// (다음 모드 진입 시 SetActiveMode 가 누적을 비우므로 여기서 flush 해야 한다.)
	FlushSessionToSave();

	// 이 폰이 만든 투수는 이 폰이 치운다. 모드 선택으로 돌아갈 때 폰만 파괴되면
	// APitchingZone 이 남아 빈 화면에 계속 공을 던진다.
	if (PitchingZone)
	{
		PitchingZone->Destroy();
		PitchingZone = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ASwingTestPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input 에셋 없이 키 직접 바인딩 (레거시 BindKey — 테스트용).
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASwingTestPawn::SimulateSwing);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ASwingTestPawn::ResetSession);
	// Esc 는 PIE 종료라 쓸 수 없다 → M(메뉴).
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ASwingTestPawn::ReturnToModeSelect);
	// F: 세션 채점 → 약점 분석 → 운동 추천 (+ AI 코칭).
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ASwingTestPawn::RequestFeedback);
}

FWeaknessReport ASwingTestPawn::BuildSessionReport() const
{
	FWeaknessReport Report = UWeaknessDetector::DetectSwing(SessionHistory, ScoringConfig);
	// 신체역학 약점축(X-factor·머리 안정·운동 사슬·체중 이동)을 같은 리포트에 합류.
	UWeaknessDetector::AppendBodyMechanicsWeaknesses(Report, SessionBodyHistory, BodyMechanicsScoring);
	return Report;
}

void ASwingTestPawn::RequestFeedback()
{
	if (SessionHistory.Num() == 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(30, 2.0f, FColor::Silver, TEXT("먼저 스윙을 몇 번 해주세요 (Space)."));
		}
		return;
	}

	// 1) 결정론적 약점 판별 + 드릴 추천 (네트워크 불필요 — 항상 나온다).
	LastReport = BuildSessionReport();

	// 과거 저장 이력에서 만성 약점·추세를 뽑아 추천에 반영한다.
	// (GetHistory 는 아직 저장 안 된 이번 세션을 제외 → "지금 vs 그동안" 비교가 성립.)
	LastChronic = FChronicWeaknessReport();
	if (UModeManager* ModeManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		LastChronic = UWeaknessDetector::AnalyzeTrend(ModeManager->GetHistory(), EGameModeId::Batting, 5, NAME_None);
	}
	LastDrills = UDrillCatalog::RecommendWithHistory(LastReport, LastChronic, 3);
	bShowFeedback = true;

	// 2) AI 코칭 요청 (키가 있으면 표현 문장을 얹는다).
	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText.Reset();
		bAwaitingCoaching = true;
		FeedbackService->RequestSwingCoaching(LastReport, LastDrills, LastChronic);
	}
	else
	{
		bAwaitingCoaching = false;
		CoachingText = TEXT("(AI 코칭 미설정 — Config/Secrets.ini 에 [AI] ApiKey)");
	}
}

void ASwingTestPawn::HandleCoachingReady(bool bSuccess, const FString& Text)
{
	bAwaitingCoaching = false;
	// 성공이면 코칭 문장, 실패면 사유 문구 — 둘 다 화면에 그대로 보여준다.
	CoachingText = Text;
	UE_LOG(LogMotionBase, Log, TEXT("[SwingTest] AI 코칭 %s"), bSuccess ? TEXT("수신") : TEXT("실패"));
}

bool ASwingTestPawn::GetSessionSummary(FSessionSummary& OutSummary) const
{
	// [F] 로 결과를 띄우지 않았으면 결과 화면을 그리지 않는다 (플레이 중).
	if (!bShowFeedback)
	{
		return false;
	}

	OutSummary.Mode = EGameModeId::Batting;
	OutSummary.Difficulty = SessionDifficulty;
	OutSummary.Score = SessionScore;

	OutSummary.SwingCount = SwingCount;
	OutSummary.ContactCount = ContactCount;
	OutSummary.HomeRunCount = HomeRunCount;
	OutSummary.HitCount = HitCount;
	OutSummary.StrikeoutCount = StrikeoutCount;
	OutSummary.WalkCount = WalkCount;

	OutSummary.MaxCarryDistanceM = SessionMaxCarryM;
	OutSummary.AvgCarryDistanceM = (ContactCount > 0) ? (SessionCarrySumM / ContactCount) : 0.0f;

	OutSummary.Report = LastReport;
	OutSummary.Chronic = LastChronic;
	OutSummary.Drills = LastDrills;
	OutSummary.CoachingText = CoachingText;
	OutSummary.bAwaitingCoaching = bAwaitingCoaching;

	OutSummary.BodyMechanics = LastBodyMechanics;
	OutSummary.bMockBodyMechanics = true; // 실제 카메라가 아니라 Mock 포즈 소스 산출

	// 최고 기록은 "아직 저장되지 않은 이번 세션"을 제외한 과거 기록에서 온다.
	// → 이번 총점이 그 값을 넘으면 신기록. (첫 기록이면 Best < 0.)
	OutSummary.BestTotalScore = -1.0f;
	OutSummary.bNewRecord = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* ModeManager = GI->GetSubsystem<UModeManager>())
		{
			OutSummary.PriorStats = ModeManager->GetModeStats(EGameModeId::Batting);
			OutSummary.BestTotalScore = ModeManager->GetBestTotalScore(EGameModeId::Batting);
			OutSummary.bNewRecord = SessionScore.bValid && SwingCount > 0 &&
				(OutSummary.BestTotalScore < 0.0f || SessionScore.TotalScore > OutSummary.BestTotalScore);
		}
	}

	return true;
}

void ASwingTestPawn::ReturnToModeSelect()
{
	if (AMotionBaseGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMotionBaseGameMode>() : nullptr)
	{
		GM->ReturnToModeSelect();
	}
}

void ASwingTestPawn::HandlePitchThrown(EPitchType PitchType, FVector InPlateLocation, float InArrivalWorldTime)
{
	CurrentPitchType = PitchType;
	bSwungThisPitch = false;
	bCurrentPitchIsStrike = PitchingZone ? PitchingZone->IsLastPitchStrike() : false;
}

void ASwingTestPawn::HandlePitchArrived(FVector InPlateLocation)
{
	// 스윙했으면 판정은 SimulateSwing 에서 이미 끝났다 (컨택은 여기 도달 전에 상태가 바뀐다).
	if (bSwungThisPitch)
	{
		return;
	}

	// 안 치고 흘려보낸 공 = 루킹 판정.
	++MissedPitchCount;
	if (bCurrentPitchIsStrike)
	{
		LastPitchCall = TEXT("스트라이크 (루킹)");
		AddStrike(false);
	}
	else
	{
		++Balls;
		LastPitchCall = TEXT("볼");
		if (Balls >= 4)
		{
			++WalkCount;
			EndAtBat(TEXT("볼넷!"));
		}
	}
}

void ASwingTestPawn::AddStrike(bool bFromFoul)
{
	// 파울은 2스트라이크 이후엔 카운트 유지 (삼진 안 됨).
	if (bFromFoul && Strikes >= 2)
	{
		return;
	}

	++Strikes;
	if (Strikes >= 3)
	{
		++StrikeoutCount;
		EndAtBat(TEXT("삼진!"));
	}
}

void ASwingTestPawn::EndAtBat(const FString& Reason)
{
	LastPitchCall = Reason;
	Balls = 0;
	Strikes = 0;
	UE_LOG(LogMotionBase, Log, TEXT("[SwingTest] 타석 종료 — %s (삼진 %d, 볼넷 %d)"),
		*Reason, StrikeoutCount, WalkCount);
}

TArray<FSwingSample> ASwingTestPawn::BuildSyntheticSwing(double ContactWorldTime, const FVector& BallLocation,
	float ContactSpeedMps, float MissDistanceCm) const
{
	constexpr int32 N = 20;
	constexpr int32 ContactIndex = 10;
	constexpr double Dt = 0.01;

	const float StepCm = ContactSpeedMps * 100.0f * static_cast<float>(Dt); // 샘플당 이동거리(cm)
	const FVector SwingDir = GetActorRightVector();                 // 배트가 가로지르는 방향
	const FVector Offset = GetActorUpVector() * MissDistanceCm;     // 공과의 수직 빗나감

	TArray<FSwingSample> Samples;
	Samples.Reserve(N);
	for (int32 i = 0; i < N; ++i)
	{
		// 컨택 샘플의 시각이 정확히 키 입력 시각이 되도록 중심을 맞춘다.
		const double T = ContactWorldTime + (i - ContactIndex) * Dt;
		const FVector Pos = BallLocation + Offset + SwingDir * static_cast<float>(i - ContactIndex) * StepCm;
		Samples.Emplace(T, Pos, FRotator::ZeroRotator);
	}
	return Samples;
}

void ASwingTestPawn::SimulateSwing()
{
	if (!PitchingZone || !PitchingZone->IsPitchInFlight())
	{
		GEngine->AddOnScreenDebugMessage(9, 1.0f, FColor::Silver, TEXT("지금은 투구 중이 아닙니다."));
		return;
	}
	if (bSwungThisPitch)
	{
		return; // 한 투구에 한 번만
	}
	bSwungThisPitch = true;

	// 결과 화면이 떠 있었으면 스윙과 함께 닫고 라이브 플레이로 돌아간다.
	bShowFeedback = false;

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const FVector Plate = PitchingZone->GetPlateLocation();
	const double Arrival = PitchingZone->GetArrivalWorldTime();

	// 배트 스피드·조준 정확도는 아직 입력에서 오지 않으므로 무작위.
	// (Vive 연동 시 실제 궤적으로 대체된다 — 타이밍은 이미 진짜 입력이다.)
	const float ContactSpeed = FMath::FRandRange(20.0f, 36.0f);
	const float Miss = FMath::FRandRange(0.0f, 22.0f);

	const TArray<FSwingSample> Samples = BuildSyntheticSwing(Now, Plate, ContactSpeed, Miss);

	LastMetrics = USwingAnalyzer::AnalyzeSwing(Samples, Plate, Arrival);
	LastHit = UHitModel::Simulate(LastMetrics, ScoringConfig);
	LastSwingScore = UScoringService::ScoreSwing(LastMetrics, ScoringConfig);

	SessionHistory.Add(LastMetrics);
	SessionScore = UScoringService::ScoreSession(SessionHistory, ScoringConfig);
	++SwingCount;
	bHasSwung = true;

	// 스윙 1회 = 시도 1건. ModeManager 에 누적해 두면 세션 종료 시
	// FinalizeSession 이 이 시도들을 한 세션 기록으로 저장한다.
	if (UModeManager* ModeManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		ModeManager->RecordResult(LastSwingScore);
	}

	// 동적 난이도 반영 — 이번 스윙 성적으로 다음 투구의 구속·변화구를 조정한다.
	PitchingZone->RegisterSwingOutcome(LastMetrics.bContacted, LastSwingScore.TotalScore / 100.0f);

	// 신체역학 경로 — 카메라(MediaPipe)가 없어 Mock 포즈로 분석기를 실제로 돌린다.
	// 컨택 클럭(Now)을 그대로 넣어 kinetic chain 리드가 스윙 타이밍과 정렬되게 한다.
	// 스윙 세기/타이밍을 Mock 형태에 살짝 반영해 스윙마다 지표가 달라지도록 한다.
	FMockSwingPoseParams PoseParams = MockPoseParams;
	PoseParams.ShoulderRotationDeg += FMath::Clamp((LastMetrics.ContactSpeedMps - 28.0f) * 0.8f, -15.0f, 20.0f);
	PoseParams.HeadTravelCm += FMath::Clamp(FMath::Abs(LastMetrics.TimingErrorSeconds) * 30.0f, 0.0f, 8.0f);

	const TArray<FCameraPoseFrame> PoseFrames = UMockCameraPoseSource::BuildSwingSequence(Now, PoseParams);
	LastBodyMechanics = UBodyMechanicsAnalyzer::Analyze(PoseFrames, Now, BodyMechanicsConfig);
	SessionBodyHistory.Add(LastBodyMechanics);

	if (LastMetrics.bContacted)
	{
		++ContactCount;
		if (LastHit.Class == EHitClass::HomeRun) { ++HomeRunCount; }
		else if (LastHit.Class == EHitClass::Hit) { ++HitCount; }

		// 비거리 집계 (최고/평균은 컨택 타구 기준).
		SessionMaxCarryM = FMath::Max(SessionMaxCarryM, LastHit.CarryDistanceM);
		SessionCarrySumM += LastHit.CarryDistanceM;

		// 타구 연출 — 무작위가 아니라 계산된 발사각·좌우각·타구 속도로 날린다.
		// (파울도 파울 방향으로 날아간다. 헛스윙만 연출 없음.)
		// 좌타는 당겨치는 방향이 반대(1루→3루 대칭)이므로 좌우각을 반전한다.
		const float SpraySign = (SessionStance == EBattingStance::Left) ? -1.0f : 1.0f;
		const FVector LocalDir = FRotator(LastHit.LaunchAngleDeg, LastHit.SprayAngleDeg * SpraySign, 0.0f).Vector();
		const FVector HitDir = GetActorTransform().TransformVectorNoScale(LocalDir).GetSafeNormal();
		PitchingZone->LaunchHitBall(HitDir, LastHit.ExitVelocityMps);
	}

	// 스트라이크/볼 판정 (스윙 결과 기준).
	if (!LastMetrics.bContacted)
	{
		LastPitchCall = TEXT("스윙 헛스윙");
		AddStrike(false); // 3스트라이크면 삼진으로 타석 종료
	}
	else if (LastHit.Class == EHitClass::Foul)
	{
		LastPitchCall = TEXT("파울");
		AddStrike(true);
	}
	else
	{
		// 페어 타구 → 인플레이, 타석 종료 (판정 이름이 결과: 홈런/안타/아웃).
		EndAtBat(UHitModel::GetClassDisplayName(LastHit.Class).ToString());
	}

	UE_LOG(LogMotionBase, Log, TEXT("[SwingTest] #%d timing=%+.3fs %s EV=%.1f m/s dist=%.0f m total=%.1f"),
		SwingCount, LastMetrics.TimingErrorSeconds,
		*UHitModel::GetClassDisplayName(LastHit.Class).ToString(),
		LastHit.ExitVelocityMps, LastHit.CarryDistanceM, LastSwingScore.TotalScore);
}

void ASwingTestPawn::FlushSessionToSave()
{
	if (UModeManager* ModeManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UModeManager>() : nullptr)
	{
		// 세션 집계 점수 + 약점 리포트를 함께 확정 저장한다. 리포트는 저장 시점의
		// SessionHistory 로 새로 계산한다 — [F] 를 안 눌렀어도 만성 약점 추적이 되도록.
		// (시도가 없으면 ModeManager 가 빈 세션으로 스스로 무시한다.)
		const FWeaknessReport Report = BuildSessionReport();
		ModeManager->FinalizeSession(SessionScore, Report);
	}
}

void ASwingTestPawn::ResetSession()
{
	// 리셋 = 지금 세션 종료 + 새 세션 시작. 버리기 전에 저장한다.
	FlushSessionToSave();

	SessionHistory.Reset();
	SessionBodyHistory.Reset();
	SessionScore = FScoreResult();
	LastSwingScore = FScoreResult();
	LastMetrics = FSwingMetrics();
	LastHit = FBattedBallResult();
	LastBodyMechanics = FBodyMechanicsMetrics();
	SwingCount = 0;
	ContactCount = 0;
	MissedPitchCount = 0;
	HomeRunCount = 0;
	HitCount = 0;
	SessionMaxCarryM = 0.0f;
	SessionCarrySumM = 0.0f;
	Balls = 0;
	Strikes = 0;
	StrikeoutCount = 0;
	WalkCount = 0;
	LastPitchCall.Reset();
	bHasSwung = false;

	// 피드백도 초기화 — 지난 세션 약점이 새 세션에 남으면 안 된다.
	bShowFeedback = false;
	bAwaitingCoaching = false;
	LastReport = FWeaknessReport();
	LastChronic = FChronicWeaknessReport();
	LastDrills.Reset();
	CoachingText.Reset();

	UE_LOG(LogMotionBase, Log, TEXT("[SwingTest] 세션 리셋"));
}

void ASwingTestPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GEngine)
	{
		return;
	}

	// 결과 화면([F])이 떠 있는 동안에는 라이브 디버그 HUD 를 그리지 않는다 —
	// HUD(AModeSelectHUD)가 결과 패널을 Canvas 로 그린다. 스윙([Space])하면 해제된다.
	if (bShowFeedback)
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor::White,
		FString::Printf(TEXT("=== MotionBase 타격 훈련 [%s · %s] ===   [Space] 스윙   [F] 분석·추천   [R] 리셋   [M] 모드 선택"),
			*UModeManager::GetDifficultyDisplayName(SessionDifficulty).ToString(),
			*UModeManager::GetStanceDisplayName(SessionStance).ToString()));

	// 동적 난이도 — 성적에 따라 구속·변화구가 오르내린다 (0%=프리셋, 100%=최대 상승).
	if (PitchingZone)
	{
		GEngine->AddOnScreenDebugMessage(11, 2.0f, FColor(255, 180, 90),
			FString::Printf(TEXT("동적 난이도: %.0f%%  (잘 치면 상승 · 놓치면 하강)"),
				PitchingZone->GetDynamicLevel() * 100.0f));
	}

	// 투구 상태
	if (PitchingZone && PitchingZone->IsPitchInFlight())
	{
		const float Remain = PitchingZone->GetArrivalWorldTime() - (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
		GEngine->AddOnScreenDebugMessage(2, 2.0f, FColor::Yellow,
			FString::Printf(TEXT("%s 투구 중 — 도달까지 %.2f초"),
				CurrentPitchType == EPitchType::Breaking ? TEXT("변화구") : TEXT("직구"),
				FMath::Max(0.0f, Remain)));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(2, 2.0f, FColor::Silver, TEXT("다음 투구 준비 중..."));
	}

	// 볼카운트 (첫 스윙 전에도 항상 표시). 스트라이크 존은 초록 박스로 보인다.
	GEngine->AddOnScreenDebugMessage(8, 2.0f, FColor::White,
		FString::Printf(TEXT("볼 %d - 스트라이크 %d    |  삼진 %d · 볼넷 %d"),
			Balls, Strikes, StrikeoutCount, WalkCount));
	if (!LastPitchCall.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(9, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("판정: %s"), *LastPitchCall));
	}

	if (!bHasSwung)
	{
		GEngine->AddOnScreenDebugMessage(3, 2.0f, FColor::White,
			TEXT("공이 날아올 때 스페이스바를 눌러 타이밍을 맞추세요. (안 치면 볼/스트라이크 판정)"));
		return;
	}

	GEngine->AddOnScreenDebugMessage(3, 2.0f, LastMetrics.bContacted ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("최근 스윙: %s | 배트속도 %.1f m/s | 컨택거리 %.1f cm | 타이밍 %+.3f s (%s)"),
			LastMetrics.bContacted ? TEXT("컨택") : TEXT("헛스윙"),
			LastMetrics.ContactSpeedMps, LastMetrics.ContactDistanceCm, LastMetrics.TimingErrorSeconds,
			LastMetrics.TimingErrorSeconds > 0.0f ? TEXT("늦음") : TEXT("빠름")));

	// 타구 결과 (컨택했을 때만) — 홈런은 눈에 띄게.
	if (LastMetrics.bContacted)
	{
		const FColor HitColor = (LastHit.Class == EHitClass::HomeRun) ? FColor::Yellow
			: (LastHit.Class == EHitClass::Hit) ? FColor::Green
			: (LastHit.Class == EHitClass::Foul) ? FColor::Silver
			: FColor::Orange; // 아웃
		const FString Bang = (LastHit.Class == EHitClass::HomeRun) ? TEXT("!!") : TEXT("");
		GEngine->AddOnScreenDebugMessage(7, 2.0f, HitColor,
			FString::Printf(TEXT("타구: %s%s | 타구속도 %.0f km/h | 비거리 %.0f m | 발사각 %.0f° | 좌우 %+.0f°"),
				*UHitModel::GetClassDisplayName(LastHit.Class).ToString(), *Bang,
				LastHit.ExitVelocityMps * 3.6f, LastHit.CarryDistanceM,
				LastHit.LaunchAngleDeg, LastHit.SprayAngleDeg));
	}

	// 일관성은 단일 스윙에서 정의되지 않는다(세션 단위) → 최근 스윙 줄에는 표시하지 않는다.
	GEngine->AddOnScreenDebugMessage(4, 2.0f, FColor::Cyan,
		FString::Printf(TEXT("최근 스윙 점수: 총점 %.1f  (정확도 %.2f  효율 %.2f)   ※일관성은 세션 단위"),
			LastSwingScore.TotalScore, LastSwingScore.Accuracy, LastSwingScore.Efficiency));

	GEngine->AddOnScreenDebugMessage(5, 2.0f, FColor::Orange,
		FString::Printf(TEXT("세션 평균: 총점 %.1f  (정확도 %.2f  효율 %.2f  일관성 %.2f)"),
			SessionScore.TotalScore, SessionScore.Accuracy, SessionScore.Efficiency, SessionScore.Consistency));

	GEngine->AddOnScreenDebugMessage(6, 2.0f, FColor::White,
		FString::Printf(TEXT("스윙 %d회 | 컨택 %d | 헛스윙 %d | 홈런 %d | 안타 %d | 그냥 보낸 공 %d"),
			SwingCount, ContactCount, SwingCount - ContactCount, HomeRunCount, HitCount, MissedPitchCount));

	// 신체역학 (Mock 포즈 → 분석기). 실제 카메라가 붙으면 소스만 교체된다.
	if (LastBodyMechanics.bValid)
	{
		GEngine->AddOnScreenDebugMessage(10, 2.0f, FColor(150, 200, 255),
			FString::Printf(TEXT("신체역학[MOCK]: X-factor %.0f° | 체중이동 %.0fcm | 머리 %.1fcm | 척추 %.0f° | 체인 %s | 신뢰도 %.2f"),
				LastBodyMechanics.HipShoulderSeparationDeg, LastBodyMechanics.WeightShiftCm,
				LastBodyMechanics.HeadTravelCm, LastBodyMechanics.SpineTiltDeg,
				LastBodyMechanics.bKineticChainOrdered ? TEXT("정상") : TEXT("흐트러짐"),
				LastBodyMechanics.Confidence));
	}
}
