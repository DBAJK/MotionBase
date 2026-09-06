#include "Misc/AutomationTest.h"
#include "Scoring/ScoringService.h"

#if WITH_DEV_AUTOMATION_TESTS

// UScoringService 순수 로직 단위 테스트 — 헤드셋 없이 헤드리스 실행.
// 실행: 에디터 Session Frontend > Automation > "MotionBase.Scoring.ScoringService"
//   또는 커맨드라인 -ExecCmds="Automation RunTests MotionBase.Scoring.ScoringService; Quit"

namespace
{
	/** Accuracy=1.0, Efficiency=1.0 이 되도록 맞춘 "완벽한 컨택" 지표. */
	FSwingMetrics MakePerfectContact()
	{
		FSwingMetrics M;
		M.bSwingDetected = true;
		M.bContacted = true;
		M.TimingErrorSeconds = 0.0f;  // 타이밍 오차 0 → 가우시안 감쇠 1.0
		M.ContactDistanceCm = 0.0f;   // 스위트스팟 정타 → 거리 감쇠 1.0
		M.ContactSpeedMps = 30.0f;    // 기본 Config(반발 0.5, 목표타구속도 40)에서 효율이 1.0으로 클램프됨
		return M;
	}

	FSwingMetrics MakeWhiff()
	{
		FSwingMetrics M;
		M.bSwingDetected = true;
		M.bContacted = false;
		return M;
	}
}

// ── 헛스윙은 총점 0 이어야 한다 (예전엔 효율·일관성이 남아 최대 60점까지 나왔다) ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScoringServiceWhiffZeroTest,
	"MotionBase.Scoring.ScoringService.WhiffTotalScoreZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScoringServiceWhiffZeroTest::RunTest(const FString& Parameters)
{
	const FScoringConfig Config; // 기본값
	const FScoreResult R = UScoringService::ScoreSwing(MakeWhiff(), Config);

	TestFalse(TEXT("헛스윙은 bValid=false"), R.bValid);
	TestEqual(TEXT("헛스윙 총점 0"), R.TotalScore, 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("헛스윙 정확도 0"), R.Accuracy, 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("헛스윙 효율 0"), R.Efficiency, 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("헛스윙 일관성 0"), R.Consistency, 0.0f, KINDA_SMALL_NUMBER);

	return true;
}

// ── 단일 스윙은 일관성 축을 빼고 정확도·효율만으로 재정규화해야 한다 ──
//
// Accuracy=Efficiency=1.0 인 완벽한 컨택으로 만들면:
//   올바른 구현(정확도+효율 가중치로만 재정규화) → 총점 = 100
//   틀린 구현(3축 가중치 합으로 나눔, 일관성 0을 그대로 반영) → 총점 = 75 (기본 가중치 기준)
// 두 값이 갈라지므로 회귀를 정확히 잡아낸다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScoringServiceSingleSwingRenormalizeTest,
	"MotionBase.Scoring.ScoringService.SingleSwingRenormalize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScoringServiceSingleSwingRenormalizeTest::RunTest(const FString& Parameters)
{
	const FScoringConfig Config; // WeightAccuracy=0.4, WeightEfficiency=0.35, WeightConsistency=0.25
	const FSwingMetrics Metrics = MakePerfectContact();

	// 전제 확인 — Efficiency 계산이 예상대로 1.0 근처로 클램프되는지.
	const float Efficiency = UScoringService::EvalEfficiency(Metrics, Config);
	TestEqual(TEXT("전제: 효율이 1.0으로 클램프됨"), Efficiency, 1.0f, KINDA_SMALL_NUMBER);

	const FScoreResult R = UScoringService::ScoreSwing(Metrics, Config);
	TestTrue(TEXT("컨택 스윙은 bValid=true"), R.bValid);
	TestEqual(TEXT("정확도 1.0"), R.Accuracy, 1.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("효율 1.0"), R.Efficiency, 1.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("단일 스윙 일관성은 미측정(0)"), R.Consistency, 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("정확도·효율만으로 재정규화 → 총점 100"), R.TotalScore, 100.0f, 0.01f);

	return true;
}

// ── 세션 일관성은 헛스윙을 제외한 '컨택한 스윙'의 정확도 편차로만 잰다 ──
//
// 헛스윙(정확도 0)까지 편차 계산에 넣으면 편차가 커져 일관성이 떨어져야 정상인데,
// 실제로는 "다 헛스윙 → 편차 0 → 일관성 만점" 이라는 역설이 생겨 제외해야 한다.
// 여기선 반대로: 헛스윙을 몇 개 섞어도, 컨택한 스윙들이 서로 완전히 같으면
// 일관성이 여전히 만점(1.0)이어야 함을 확인한다 (헛스윙이 편차에 안 들어간다는 증거).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScoringServiceConsistencyExcludesWhiffsTest,
	"MotionBase.Scoring.ScoringService.ConsistencyExcludesWhiffs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScoringServiceConsistencyExcludesWhiffsTest::RunTest(const FString& Parameters)
{
	const FScoringConfig Config;

	TArray<FSwingMetrics> History;
	History.Add(MakeWhiff());
	History.Add(MakeWhiff());
	History.Add(MakeWhiff());
	History.Add(MakePerfectContact());
	History.Add(MakePerfectContact()); // 컨택 2건, 정확도 둘 다 동일(1.0) → 편차 0.

	const FScoreResult R = UScoringService::ScoreSession(History, Config);

	TestTrue(TEXT("세션 유효"), R.bValid);
	if (const float* ContactCount = R.Details.Find(TEXT("ContactCount")))
	{
		TestEqual(TEXT("컨택 표본 2건 집계"), *ContactCount, 2.0f, KINDA_SMALL_NUMBER);
	}
	else
	{
		AddError(TEXT("Details에 ContactCount 없음"));
	}
	TestEqual(TEXT("헛스윙 섞여도 컨택 표본 편차 0 → 일관성 만점"), R.Consistency, 1.0f, KINDA_SMALL_NUMBER);

	return true;
}

// ── 가중치를 어떻게 바꿔도(합이 1이 아니어도) 총점은 100을 넘지 않아야 한다 ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScoringServiceWeightScaleClampTest,
	"MotionBase.Scoring.ScoringService.WeightScaleNeverExceeds100",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScoringServiceWeightScaleClampTest::RunTest(const FString& Parameters)
{
	// 완벽한 스윙만 있는 세션 — 정확도=효율=일관성=1.0 이 되어 상한 케이스를 만든다.
	TArray<FSwingMetrics> History;
	History.Add(MakePerfectContact());
	History.Add(MakePerfectContact());

	FScoringConfig SkewedConfig;
	SkewedConfig.WeightAccuracy = 5.0f;
	SkewedConfig.WeightEfficiency = 3.0f;
	SkewedConfig.WeightConsistency = 4.0f; // 합 12 — 가중치 합이 1이 아닌 임의의 스케일.

	const FScoreResult R = UScoringService::ScoreSession(History, SkewedConfig);

	TestTrue(TEXT("세션 유효"), R.bValid);
	TestTrue(TEXT("가중치 스케일과 무관하게 총점 <= 100"), R.TotalScore <= 100.0f + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("총점은 음수가 아님"), R.TotalScore >= 0.0f);
	// 이 케이스(3축 모두 1.0)는 스케일에 관계없이 정확히 100이어야 한다 — 그렇지 않으면
	// WeightSum() 정규화가 깨진 것.
	TestEqual(TEXT("3축 모두 만점이면 총점 정확히 100"), R.TotalScore, 100.0f, 0.01f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
