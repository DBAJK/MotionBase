#include "Testing/SwingTestPawn.h"
#include "MotionBase.h"
#include "Analysis/SwingAnalyzer.h"
#include "Analysis/WeaknessDetector.h"
#include "Analysis/HitModel.h"
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
	const FVector MoundLocation = GetActorLocation() + GetActorForwardVector() * Distance;
	PitchingZone->SetActorLocation(MoundLocation);
	// 마운드 → 타자 방향을 바라보게 (Forward 가 플레이트를 향해야 함)
	PitchingZone->SetActorRotation((GetActorLocation() - MoundLocation).Rotation());

	PitchingZone->OnPitchThrown.AddDynamic(this, &ASwingTestPawn::HandlePitchThrown);
	PitchingZone->OnPitchArrived.AddDynamic(this, &ASwingTestPawn::HandlePitchArrived);

	// 시작 화면에서 고른 난이도를 읽어 투구 파라미터에 반영한다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UModeManager* ModeManager = GI->GetSubsystem<UModeManager>())
		{
			SessionDifficulty = ModeManager->GetActiveDifficulty();
		}
	}
	PitchingZone->ApplyDifficulty(SessionDifficulty);

	// AI 코칭 서비스 (키가 없으면 요청 시 조용히 생략됨).
	FeedbackService = NewObject<UAIFeedbackService>(this);
	FeedbackService->OnFeedbackReady.AddDynamic(this, &ASwingTestPawn::HandleCoachingReady);
}

void ASwingTestPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
	LastReport = UWeaknessDetector::DetectSwing(SessionHistory, ScoringConfig);
	LastDrills = UDrillCatalog::Recommend(LastReport, 3);
	bShowFeedback = true;

	// 2) AI 코칭 요청 (키가 있으면 표현 문장을 얹는다).
	if (FeedbackService && FeedbackService->IsConfigured())
	{
		CoachingText.Reset();
		bAwaitingCoaching = true;
		FeedbackService->RequestSwingCoaching(LastReport, LastDrills);
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

void ASwingTestPawn::DrawFeedback() const
{
	if (!GEngine || !bShowFeedback)
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(30, 2.0f, FColor::White,
		FString::Printf(TEXT("=== 세션 피드백 ===  시도 %d · 컨택 %d"),
			LastReport.AttemptCount, LastReport.ContactCount));

	// 약점 (시급한 순, 상위 3개)
	const int32 ShowN = FMath::Min(LastReport.Weaknesses.Num(), 3);
	if (ShowN == 0)
	{
		GEngine->AddOnScreenDebugMessage(31, 2.0f, FColor::Green, TEXT("두드러진 약점 없음 — 안정적입니다."));
	}
	for (int32 i = 0; i < ShowN; ++i)
	{
		const FWeakness& W = LastReport.Weaknesses[i];
		GEngine->AddOnScreenDebugMessage(31 + i, 2.0f, FColor::Yellow,
			FString::Printf(TEXT("약점 %d) %s — %s"),
				i + 1, *UWeaknessDetector::GetAxisDisplayName(W.Axis).ToString(), *W.Evidence));
	}

	// 추천 드릴
	for (int32 i = 0; i < LastDrills.Num(); ++i)
	{
		const FTrainingDrill& D = LastDrills[i];
		GEngine->AddOnScreenDebugMessage(40 + i, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("추천 %d) %s — %s"), i + 1, *D.Name, *D.Description));
	}

	// AI 코칭
	const FString Coach = bAwaitingCoaching ? TEXT("AI 코칭 생성 중...") : CoachingText;
	if (!Coach.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(45, 2.0f, FColor::Orange, FString::Printf(TEXT("코치: %s"), *Coach));
	}
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

	if (LastMetrics.bContacted)
	{
		++ContactCount;
		if (LastHit.Class == EHitClass::HomeRun) { ++HomeRunCount; }
		else if (LastHit.Class == EHitClass::Hit) { ++HitCount; }

		// 타구 연출 — 무작위가 아니라 계산된 발사각·좌우각·타구 속도로 날린다.
		// (파울도 파울 방향으로 날아간다. 헛스윙만 연출 없음.)
		const FVector LocalDir = FRotator(LastHit.LaunchAngleDeg, LastHit.SprayAngleDeg, 0.0f).Vector();
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

void ASwingTestPawn::ResetSession()
{
	SessionHistory.Reset();
	SessionScore = FScoreResult();
	LastSwingScore = FScoreResult();
	LastMetrics = FSwingMetrics();
	LastHit = FBattedBallResult();
	SwingCount = 0;
	ContactCount = 0;
	MissedPitchCount = 0;
	HomeRunCount = 0;
	HitCount = 0;
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

	GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor::White,
		FString::Printf(TEXT("=== MotionBase 타격 훈련 [%s] ===   [Space] 스윙   [F] 분석·추천   [R] 리셋   [M] 모드 선택"),
			*UModeManager::GetDifficultyDisplayName(SessionDifficulty).ToString()));

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

	// [F] 를 눌렀으면 세션 피드백(약점·드릴·AI 코칭)을 그린다.
	DrawFeedback();
}
