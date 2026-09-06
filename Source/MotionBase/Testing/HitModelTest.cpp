#include "Misc/AutomationTest.h"
#include "Analysis/HitModel.h"

#if WITH_DEV_AUTOMATION_TESTS

// UHitModel 순수 로직 단위 테스트 — 헤드셋 없이 헤드리스 실행.
// 실행: 에디터 Session Frontend > Automation > "MotionBase.Analysis.HitModel"
//   또는 커맨드라인 -ExecCmds="Automation RunTests MotionBase.Analysis.HitModel; Quit"

namespace
{
	FSwingMetrics MakeContact(float TimingErrorSeconds, float ContactDistanceCm, float ContactSpeedMps)
	{
		FSwingMetrics M;
		M.bSwingDetected = true;
		M.bContacted = true;
		M.TimingErrorSeconds = TimingErrorSeconds;
		M.ContactDistanceCm = ContactDistanceCm;
		M.ContactSpeedMps = ContactSpeedMps;
		return M;
	}
}

// ── 1-A 회귀: 이른/늦은 타이밍 오차가 대칭이어야 한다 ──
//
// UHitModel 의 좌우각·판정 로직은 내부 캘리브레이션 상수(실측 대상, TODO)에 기대지 않고도
// "타이밍 오차 부호만 반대인 두 스윙은 좌우각 크기와 페어/파울 판정이 같아야 한다"는
// 성질로 검증할 수 있다. 1-A 이전엔 시간창이 비대칭([-0.32,+0.12])이라 늦은 쪽(+0.12~+0.32s)
// 컨택 자체가 분석기에서 나올 수 없어, 이 대칭성이 사실상 도달 불가능한 코드였다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitModelFoulBandSymmetricTest,
	"MotionBase.Analysis.HitModel.FoulBandSymmetric",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHitModelFoulBandSymmetricTest::RunTest(const FString& Parameters)
{
	const FScoringConfig Config; // 기본값 (FoulLineDeg=45)

	// 시간창(±0.32s) 양끝에 해당하는 이른/늦은 컨택 — 그 외 조건은 동일하게.
	const FSwingMetrics EarlyMetrics = MakeContact(-0.30f, 5.0f, 25.0f);
	const FSwingMetrics LateMetrics  = MakeContact(+0.30f, 5.0f, 25.0f);

	const FBattedBallResult EarlyResult = UHitModel::Simulate(EarlyMetrics, Config);
	const FBattedBallResult LateResult  = UHitModel::Simulate(LateMetrics, Config);

	TestEqual(TEXT("좌우각 크기가 대칭(부호만 반대)"),
		FMath::Abs(EarlyResult.SprayAngleDeg), FMath::Abs(LateResult.SprayAngleDeg), 0.01f);
	TestTrue(TEXT("좌우각 부호가 반대(또는 둘 다 0)"),
		FMath::Sign(EarlyResult.SprayAngleDeg) != FMath::Sign(LateResult.SprayAngleDeg)
		|| (FMath::IsNearlyZero(EarlyResult.SprayAngleDeg) && FMath::IsNearlyZero(LateResult.SprayAngleDeg)));
	TestEqual(TEXT("페어/파울 판정이 대칭"), EarlyResult.bFair, LateResult.bFair);
	TestEqual(TEXT("판정 클래스가 대칭"), static_cast<uint8>(EarlyResult.Class), static_cast<uint8>(LateResult.Class));

	return true;
}

// ── 완벽한 타이밍 + 충분한 파워 → 홈런 판정 구간에 들어가야 한다 ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitModelHomeRunLaunchWindowTest,
	"MotionBase.Analysis.HitModel.HomeRunLaunchWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHitModelHomeRunLaunchWindowTest::RunTest(const FString& Parameters)
{
	const FScoringConfig Config; // HomeRunDistanceM=100, FoulLineDeg=45 (기본값)

	// 타이밍 오차 0(정타) + 스위트스팟(거리 0) + 강한 컨택속도 → 페어 + 장타.
	const FSwingMetrics Metrics = MakeContact(0.0f, 0.0f, 45.0f);
	const FBattedBallResult Result = UHitModel::Simulate(Metrics, Config);

	TestTrue(TEXT("페어 타구"), Result.bFair);
	TestTrue(TEXT("충분한 비거리"), Result.CarryDistanceM >= Config.HomeRunDistanceM);
	TestEqual(TEXT("홈런 판정"), Result.Class, EHitClass::HomeRun);

	return true;
}

// ── 컨택 거리가 MaxContactDistanceCm 이면 타구속도가 0 이어야 한다 (스위트스팟 밖 하한) ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitModelMaxContactDistanceZeroEVTest,
	"MotionBase.Analysis.HitModel.MaxContactDistanceZeroExitVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHitModelMaxContactDistanceZeroEVTest::RunTest(const FString& Parameters)
{
	const FScoringConfig Config; // MaxContactDistanceCm=32 (기본값)

	const FSwingMetrics Metrics = MakeContact(0.0f, Config.MaxContactDistanceCm, 40.0f);
	const float EV = UHitModel::ExitVelocityMps(Metrics, Config);
	TestEqual(TEXT("컨택 거리 상한에서 타구속도 0"), EV, 0.0f, KINDA_SMALL_NUMBER);

	const FBattedBallResult Result = UHitModel::Simulate(Metrics, Config);
	TestEqual(TEXT("Simulate 결과도 타구속도 0"), Result.ExitVelocityMps, 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("비거리도 0"), Result.CarryDistanceM, 0.0f, KINDA_SMALL_NUMBER);

	return true;
}

// ── 컨택 안 한 스윙은 무조건 Whiff — 다른 필드는 참조하지 않는다 ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitModelNoContactIsWhiffTest,
	"MotionBase.Analysis.HitModel.NoContactIsWhiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHitModelNoContactIsWhiffTest::RunTest(const FString& Parameters)
{
	const FScoringConfig Config;

	FSwingMetrics Metrics;
	Metrics.bSwingDetected = true;
	Metrics.bContacted = false;
	// 헛스윙인데도 속도만 높게 남아 있는 경우(방어적 테스트) — 그래도 Whiff 여야 한다.
	Metrics.ContactSpeedMps = 40.0f;

	const FBattedBallResult Result = UHitModel::Simulate(Metrics, Config);
	TestEqual(TEXT("헛스윙 → Whiff"), Result.Class, EHitClass::Whiff);
	TestEqual(TEXT("헛스윙 타구속도 0"), Result.ExitVelocityMps, 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("헛스윙 비거리 0"), Result.CarryDistanceM, 0.0f, KINDA_SMALL_NUMBER);

	const float EV = UHitModel::ExitVelocityMps(Metrics, Config);
	TestEqual(TEXT("ExitVelocityMps 단독 호출도 0"), EV, 0.0f, KINDA_SMALL_NUMBER);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
