#include "Actors/PitchingZone.h"
#include "MotionBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"

APitchingZone::APitchingZone()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	Ball->SetupAttachment(SceneRoot);
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ball->SetVisibility(false);
	// 엔진 기본 구체(지름 100cm)를 야구공(~7.3cm)으로 축소.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Ball->SetStaticMesh(SphereMesh.Object);
		// 실제 야구공은 7.3cm 지만 VR 에서 잘 보이도록 10cm 로 키운다.
		Ball->SetWorldScale3D(FVector(0.10f));
	}
}

void APitchingZone::BeginPlay()
{
	Super::BeginPlay();
	EnterIdle();
}

FVector APitchingZone::ComputeReleaseLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, ReleaseHeightCm);
}

void APitchingZone::EnterIdle()
{
	State = EPitchState::Idle;
	IdleTimer = 0.0f;
	if (Ball)
	{
		Ball->SetVisibility(false);
	}
}

void APitchingZone::ThrowRandomPitch()
{
	const bool bBreaking = FMath::FRand() < BreakingBallRatio;
	const float Speed = FMath::FRandRange(SpeedMinKmh, SpeedMaxKmh);
	ThrowPitch(bBreaking ? EPitchType::Breaking : EPitchType::Fastball, Speed);
}

void APitchingZone::ApplyDifficulty(EDifficultyLevel Level)
{
	// TODO(캘리브레이션): 아래 프리셋 수치는 실측·플레이테스트로 조정 (하드코딩 확정 금지).
	//
	// ⚠️ 코스 분산(CourseSpread*)은 존 반폭/반높이(StrikeZoneHalf*)의 배수로 정의한다 —
	// 절대값(cm)으로 고정하면 분산이 존보다 작아질 수 있고, 그러면 FRandRange 가 뽑는 좌표가
	// 항상 존 안이라 IsLastPitchStrike() 가 매 투구 true 로 고정되어 선구(選球) 훈련이 성립하지
	// 않는다(실제로 초급에서 발생했던 버그). 배수로 두면 존을 나중에 재캘리브레이션해도
	// 이 문제가 조용히 재발하지 않는다.
	switch (Level)
	{
	case EDifficultyLevel::Beginner:
	{
		SpeedMinKmh = 70.0f;  SpeedMaxKmh = 95.0f;
		BreakingBallRatio = 0.0f;   BreakAmountCm = 40.0f;
		AutoPitchIntervalSec = 3.5f;
		constexpr float kSpreadMul = 1.25f;
		CourseSpreadLateralCm = StrikeZoneHalfWidthCm * kSpreadMul;
		CourseSpreadVerticalCm = StrikeZoneHalfHeightCm * kSpreadMul;
		break;
	}

	case EDifficultyLevel::Amateur:
	{
		SpeedMinKmh = 95.0f;  SpeedMaxKmh = 130.0f;
		BreakingBallRatio = 0.30f;  BreakAmountCm = 60.0f;
		AutoPitchIntervalSec = 2.5f;
		constexpr float kSpreadMul = 1.5f;
		CourseSpreadLateralCm = StrikeZoneHalfWidthCm * kSpreadMul;
		CourseSpreadVerticalCm = StrikeZoneHalfHeightCm * kSpreadMul;
		break;
	}

	case EDifficultyLevel::Pro:
	{
		SpeedMinKmh = 120.0f; SpeedMaxKmh = 155.0f;
		BreakingBallRatio = 0.55f;  BreakAmountCm = 80.0f;
		AutoPitchIntervalSec = 1.8f;
		constexpr float kSpreadMul = 1.8f;
		CourseSpreadLateralCm = StrikeZoneHalfWidthCm * kSpreadMul;
		CourseSpreadVerticalCm = StrikeZoneHalfHeightCm * kSpreadMul;
		break;
	}

	default:
		break;
	}

	// 방금 세팅한 프리셋을 동적 조정의 기준선(base)으로 잡는다. 이후 SeedDynamicLevel /
	// RegisterSwingOutcome 이 이 base 위에 headroom 을 얹는다. (수준 0 → 프리셋 그대로.)
	BaseSpeedMinKmh = SpeedMinKmh;
	BaseSpeedMaxKmh = SpeedMaxKmh;
	BaseBreakingBallRatio = BreakingBallRatio;
	BaseAutoPitchIntervalSec = AutoPitchIntervalSec;
	DynamicLevel = 0.0f;

	UE_LOG(LogMotionBase, Log, TEXT("PitchingZone: 난이도 적용 (구속 %.0f~%.0f, 변화구 %.0f%%, 간격 %.1fs)"),
		SpeedMinKmh, SpeedMaxKmh, BreakingBallRatio * 100.0f, AutoPitchIntervalSec);
}

void APitchingZone::RefreshDynamicPitchParams()
{
	const float L = bDynamicDifficulty ? FMath::Clamp(DynamicLevel, 0.0f, 1.0f) : 0.0f;

	SpeedMinKmh = BaseSpeedMinKmh + L * DynamicSpeedHeadroomKmh;
	SpeedMaxKmh = BaseSpeedMaxKmh + L * DynamicSpeedHeadroomKmh;
	BreakingBallRatio = FMath::Clamp(BaseBreakingBallRatio + L * DynamicBreakingHeadroom, 0.0f, 1.0f);
	AutoPitchIntervalSec = FMath::Max(BaseAutoPitchIntervalSec - L * DynamicIntervalReductionSec, MinAutoPitchIntervalSec);
}

void APitchingZone::SeedDynamicLevel(float Level01)
{
	DynamicLevel = FMath::Clamp(Level01, 0.0f, 1.0f);
	RefreshDynamicPitchParams();

	UE_LOG(LogMotionBase, Log, TEXT("PitchingZone: 동적 난이도 시드 %.2f → 구속 %.0f~%.0f, 변화구 %.0f%%, 간격 %.1fs"),
		DynamicLevel, SpeedMinKmh, SpeedMaxKmh, BreakingBallRatio * 100.0f, AutoPitchIntervalSec);
}

void APitchingZone::RegisterSwingOutcome(bool bContacted, float SwingScore01)
{
	if (!bDynamicDifficulty)
	{
		return;
	}

	// 잘 친 스윙(컨택 + 기준 점수 이상)이면 상승, 헛스윙/약한 컨택이면 하강.
	const bool bGood = bContacted && (SwingScore01 >= DynamicGoodSwingScore01);
	const float Prev = DynamicLevel;
	DynamicLevel = FMath::Clamp(DynamicLevel + (bGood ? DynamicStepUp : -DynamicStepDown), 0.0f, 1.0f);

	if (!FMath::IsNearlyEqual(Prev, DynamicLevel))
	{
		RefreshDynamicPitchParams();
	}
}

bool APitchingZone::IsLastPitchStrike() const
{
	return FMath::Abs(LastCourseLateralCm) <= StrikeZoneHalfWidthCm
		&& FMath::Abs(LastCourseVerticalCm) <= StrikeZoneHalfHeightCm;
}

void APitchingZone::DrawStrikeZone() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 존 중심 = 플레이트 중심(코스 오프셋 없음).
	const FVector Center = GetActorLocation()
		+ GetActorForwardVector() * ReleaseToPlateCm
		+ FVector(0.0f, 0.0f, PlateHeightCm);

	// 얇은 깊이 + 좌우 반폭(Y) + 상하 반높이(Z). 투수 회전으로 정렬.
	const FVector Extent(2.0f, StrikeZoneHalfWidthCm, StrikeZoneHalfHeightCm);
	DrawDebugBox(World, Center, Extent, GetActorQuat(), FColor(80, 200, 120), false, -1.0f, 0, 2.0f);
}

void APitchingZone::ThrowPitch(EPitchType PitchType, float SpeedKmh)
{
	// 구속(km/h) → cm/s
	const float SpeedCmps = (SpeedKmh * 100000.0f) / 3600.0f;
	if (SpeedCmps <= KINDA_SMALL_NUMBER)
	{
		// IdleTimer 를 리셋하지 않으면 Tick 이 매 프레임 다시 투구를 시도해 이 경고가
		// 스팸으로 쏟아진다 — 다음 AutoPitchIntervalSec 만큼은 재시도하지 않게 리셋한다.
		IdleTimer = 0.0f;
		UE_LOG(LogMotionBase, Warning, TEXT("PitchingZone: 구속이 0 — 투구 취소"));
		return;
	}

	ReleaseLocation = ComputeReleaseLocation();

	// 코스: 스트라이크존 중심 기준으로 좌우/상하 분산
	const FVector Forward = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	const float CourseLateral = FMath::FRandRange(-CourseSpreadLateralCm, CourseSpreadLateralCm);
	const float CourseVertical = FMath::FRandRange(-CourseSpreadVerticalCm, CourseSpreadVerticalCm);

	// 스트라이크/볼 판정용으로 코스 오프셋을 보관.
	LastCourseLateralCm = CourseLateral;
	LastCourseVerticalCm = CourseVertical;

	PlateLocation = GetActorLocation()
		+ Forward * ReleaseToPlateCm
		+ Right * CourseLateral
		+ FVector(0.0f, 0.0f, PlateHeightCm + CourseVertical);

	TravelDurationSec = ReleaseToPlateCm / SpeedCmps;
	FlightTime = 0.0f;

	// 변화구는 중간에서 최대로 휘고 도달점은 그대로 (도달 계약 유지).
	if (PitchType == EPitchType::Breaking)
	{
		const float LateralBreak = FMath::FRandRange(-BreakAmountCm, BreakAmountCm);
		const float VerticalBreak = FMath::FRandRange(-BreakAmountCm * 0.5f, 0.0f); // 주로 떨어짐
		BreakVector = Right * LateralBreak + FVector(0.0f, 0.0f, VerticalBreak);
	}
	else
	{
		BreakVector = FVector::ZeroVector;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ArrivalWorldTime = Now + TravelDurationSec;

	State = EPitchState::Incoming;
	if (Ball)
	{
		Ball->SetWorldLocation(ReleaseLocation);
		Ball->SetVisibility(true);
	}

	UE_LOG(LogMotionBase, Log, TEXT("Pitch: type=%s speed=%.0fkm/h travel=%.3fs"),
		PitchType == EPitchType::Breaking ? TEXT("변화구") : TEXT("직구"), SpeedKmh, TravelDurationSec);

	OnPitchThrown.Broadcast(PitchType, PlateLocation, ArrivalWorldTime);
}

void APitchingZone::LaunchHitBall(const FVector& Direction, float SpeedMps)
{
	if (!Ball)
	{
		return;
	}

	State = EPitchState::HitFlight;
	FlightTime = 0.0f;
	HitStart = Ball->GetComponentLocation();
	HitVelocity = Direction.GetSafeNormal() * SpeedMps * 100.0f; // m/s → cm/s
	Ball->SetVisibility(true);
}

void APitchingZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDrawStrikeZone)
	{
		DrawStrikeZone();
	}

	switch (State)
	{
	case EPitchState::Idle:
	{
		if (!bAutoPitch)
		{
			break;
		}
		IdleTimer += DeltaSeconds;
		if (IdleTimer >= AutoPitchIntervalSec)
		{
			ThrowRandomPitch();
		}
		break;
	}

	case EPitchState::Incoming:
	{
		FlightTime += DeltaSeconds;
		const float Alpha = (TravelDurationSec > KINDA_SMALL_NUMBER)
			? FMath::Clamp(FlightTime / TravelDurationSec, 0.0f, 1.0f)
			: 1.0f;

		// 직선 보간 + 중간에서 최대인 휨 → 도달점은 정확히 PlateLocation
		const FVector Base = FMath::Lerp(ReleaseLocation, PlateLocation, Alpha);
		const FVector Curve = BreakVector * FMath::Sin(Alpha * PI);
		if (Ball)
		{
			Ball->SetWorldLocation(Base + Curve);
		}

		if (Alpha >= 1.0f)
		{
			OnPitchArrived.Broadcast(PlateLocation);
			EnterIdle();
		}
		break;
	}

	case EPitchState::HitFlight:
	{
		FlightTime += DeltaSeconds;
		constexpr float Gravity = -980.0f; // cm/s^2
		const FVector Pos = HitStart
			+ HitVelocity * FlightTime
			+ FVector(0.0f, 0.0f, 0.5f * Gravity * FlightTime * FlightTime);

		if (Ball)
		{
			Ball->SetWorldLocation(Pos);
		}

		// 착지하거나 너무 오래되면 다음 투구 대기로
		if (Pos.Z <= GetActorLocation().Z || FlightTime > 3.0f)
		{
			EnterIdle();
		}
		break;
	}
	}
}
