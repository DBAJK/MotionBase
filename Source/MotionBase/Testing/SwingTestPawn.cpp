#include "Testing/SwingTestPawn.h"
#include "MotionBase.h"
#include "Analysis/SwingAnalyzer.h"
#include "Actors/PitchingZone.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ASwingTestPawn::ASwingTestPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0; // 자동 빙의 → 입력 수신

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
}

void ASwingTestPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input 에셋 없이 키 직접 바인딩 (레거시 BindKey — 테스트용).
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ASwingTestPawn::SimulateSwing);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ASwingTestPawn::ResetSession);
}

void ASwingTestPawn::HandlePitchThrown(EPitchType PitchType, FVector InPlateLocation, float InArrivalWorldTime)
{
	CurrentPitchType = PitchType;
	bSwungThisPitch = false;
}

void ASwingTestPawn::HandlePitchArrived(FVector InPlateLocation)
{
	// 스윙하지 않고 흘려보낸 공
	if (!bSwungThisPitch)
	{
		++MissedPitchCount;
	}
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
	LastSwingScore = UScoringService::ScoreSwing(LastMetrics, ScoringConfig);

	SessionHistory.Add(LastMetrics);
	SessionScore = UScoringService::ScoreSession(SessionHistory, ScoringConfig);
	++SwingCount;
	bHasSwung = true;

	if (LastMetrics.bContacted)
	{
		++ContactCount;
		// 타구 연출 — 전방 위쪽으로 날려보낸다.
		const FVector HitDir = (GetActorForwardVector() * 2.0f
			+ GetActorRightVector() * FMath::FRandRange(-0.6f, 0.6f)
			+ FVector(0.0f, 0.0f, 1.2f)).GetSafeNormal();
		PitchingZone->LaunchHitBall(HitDir, LastMetrics.ContactSpeedMps);
	}

	UE_LOG(LogMotionBase, Log, TEXT("[SwingTest] #%d timing=%+.3fs contacted=%d total=%.1f"),
		SwingCount, LastMetrics.TimingErrorSeconds, LastMetrics.bContacted, LastSwingScore.TotalScore);
}

void ASwingTestPawn::ResetSession()
{
	SessionHistory.Reset();
	SessionScore = FScoreResult();
	LastSwingScore = FScoreResult();
	LastMetrics = FSwingMetrics();
	SwingCount = 0;
	ContactCount = 0;
	MissedPitchCount = 0;
	bHasSwung = false;
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
		TEXT("=== MotionBase 타격 테스트 ===   [Space] 스윙   [R] 리셋"));

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

	if (!bHasSwung)
	{
		GEngine->AddOnScreenDebugMessage(3, 2.0f, FColor::White,
			TEXT("공이 날아올 때 스페이스바를 눌러 타이밍을 맞추세요."));
		return;
	}

	GEngine->AddOnScreenDebugMessage(3, 2.0f, LastMetrics.bContacted ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("최근 스윙: %s | 배트속도 %.1f m/s | 컨택거리 %.1f cm | 타이밍 %+.3f s (%s)"),
			LastMetrics.bContacted ? TEXT("컨택") : TEXT("헛스윙"),
			LastMetrics.ContactSpeedMps, LastMetrics.ContactDistanceCm, LastMetrics.TimingErrorSeconds,
			LastMetrics.TimingErrorSeconds > 0.0f ? TEXT("늦음") : TEXT("빠름")));

	GEngine->AddOnScreenDebugMessage(4, 2.0f, FColor::Cyan,
		FString::Printf(TEXT("최근 점수: 총점 %.1f  (정확도 %.2f  효율 %.2f  일관성 %.2f)"),
			LastSwingScore.TotalScore, LastSwingScore.Accuracy, LastSwingScore.Efficiency, LastSwingScore.Consistency));

	GEngine->AddOnScreenDebugMessage(5, 2.0f, FColor::Orange,
		FString::Printf(TEXT("세션 평균: 총점 %.1f  (정확도 %.2f  효율 %.2f  일관성 %.2f)"),
			SessionScore.TotalScore, SessionScore.Accuracy, SessionScore.Efficiency, SessionScore.Consistency));

	GEngine->AddOnScreenDebugMessage(6, 2.0f, FColor::White,
		FString::Printf(TEXT("스윙 %d회 | 컨택 %d | 헛스윙 %d | 그냥 보낸 공 %d"),
			SwingCount, ContactCount, SwingCount - ContactCount, MissedPitchCount));
}
