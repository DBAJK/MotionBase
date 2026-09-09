#include "Misc/AutomationTest.h"
#include "Scoring/ScoringService.h"
#include "Core/ModeManager.h"

#if WITH_DEV_AUTOMATION_TESTS

// 종합 점수(공격 50 + 수비 50) 순수 계산 검증 — 합성 이력만으로 헤드리스 실행.
// 실행: 에디터 Session Frontend > Automation > "MotionBase.Overall"

namespace
{
	/** 합성 세션 하나. */
	FSessionResult MakeSession(EGameModeId Mode, FName DrillId, float TotalScore,
		EDifficultyLevel Level = EDifficultyLevel::Amateur, bool bValid = true)
	{
		FSessionResult S;
		S.Mode = Mode;
		S.DrillId = DrillId;
		S.AttemptCount = 5;
		S.DifficultyLevel = static_cast<int32>(Level);
		S.Average.TotalScore = TotalScore;
		S.Average.bValid = bValid;
		S.Average.bUncalibrated = true;
		return S;
	}

	/** 난이도 계수를 끈 설정 — 배분 로직만 따로 보고 싶을 때. */
	FOverallScoreConfig NoMultiplierConfig()
	{
		FOverallScoreConfig C;
		C.bApplyDifficultyMultiplier = false;
		return C;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverallScoreEmptyTest,
	"MotionBase.Overall.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOverallScoreEmptyTest::RunTest(const FString& Parameters)
{
	const FOverallScore R = UScoringService::ComputeOverall(
		TArray<FSessionResult>(), UModeManager::BuildOverallCategories(), NoMultiplierConfig());

	TestFalse(TEXT("기록이 없으면 무효"), R.bValid);
	TestEqual(TEXT("실시 종목 0"), R.PlayedCount, 0);
	TestEqual(TEXT("총점 0"), R.Total, 0.0f, 0.01f);
	TestEqual(TEXT("종목 칸은 4개(타격+수비3)"), R.CategoryCount, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverallScoreFullTest,
	"MotionBase.Overall.AllCategories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOverallScoreFullTest::RunTest(const FString& Parameters)
{
	TArray<FSessionResult> H;
	H.Add(MakeSession(EGameModeId::Batting, NAME_None, 100.0f));
	H.Add(MakeSession(EGameModeId::Defense, UModeManager::GetDefenseDrillIdName(0), 100.0f));
	H.Add(MakeSession(EGameModeId::Defense, UModeManager::GetDefenseDrillIdName(1), 100.0f));
	H.Add(MakeSession(EGameModeId::Defense, UModeManager::GetDefenseDrillIdName(2), 100.0f));

	const FOverallScore R = UScoringService::ComputeOverall(
		H, UModeManager::BuildOverallCategories(), NoMultiplierConfig());

	TestTrue(TEXT("유효"), R.bValid);
	TestEqual(TEXT("전 종목 만점이면 100"), R.Total, 100.0f, 0.01f);
	TestEqual(TEXT("공격 소계 50"), R.OffensePoints, 50.0f, 0.01f);
	TestEqual(TEXT("수비 소계 50"), R.DefensePoints, 50.0f, 0.01f);
	TestEqual(TEXT("4종목 전부 실시"), R.PlayedCount, 4);
	// 전 종목을 하면 환산이 무의미해져 절대 점수와 같아져야 한다.
	TestEqual(TEXT("전 종목 완주 시 Total == RawTotal"), R.Total, R.RawTotal, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverallScorePartialTest,
	"MotionBase.Overall.PartialPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOverallScorePartialTest::RunTest(const FString& Parameters)
{
	// 타격만 만점으로 한 판. 미실시를 0 으로 깔면 50 이지만, 제외하면 100 이어야 한다 —
	// 이게 이 설계의 핵심 요건이다(부스 관람객 대부분이 한 종목만 한다).
	TArray<FSessionResult> H;
	H.Add(MakeSession(EGameModeId::Batting, NAME_None, 100.0f));

	const FOverallScore R = UScoringService::ComputeOverall(
		H, UModeManager::BuildOverallCategories(), NoMultiplierConfig());

	TestEqual(TEXT("타격만 만점 → 환산 100"), R.Total, 100.0f, 0.01f);
	TestEqual(TEXT("절대 점수는 50 (미실시를 0 으로 본 값)"), R.RawTotal, 50.0f, 0.01f);
	TestEqual(TEXT("실시 종목 1"), R.PlayedCount, 1);
	TestEqual(TEXT("수비 소계 0"), R.DefensePoints, 0.0f, 0.01f);

	// 수비 한 종목만 절반 점수로 추가 → 실시 2종목 기준으로 환산된다.
	H.Add(MakeSession(EGameModeId::Defense, UModeManager::GetDefenseDrillIdName(2), 50.0f));
	const FOverallScore R2 = UScoringService::ComputeOverall(
		H, UModeManager::BuildOverallCategories(), NoMultiplierConfig());

	// 획득 = 50(타격) + 8.333(백업 50%) = 58.333, 실시 만점 = 50 + 16.667 = 66.667
	TestEqual(TEXT("실시 2종목 환산"), R2.Total, 58.3333f / 66.6667f * 100.0f, 0.1f);
	TestEqual(TEXT("실시 종목 2"), R2.PlayedCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverallScoreBestOfTest,
	"MotionBase.Overall.BestSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOverallScoreBestOfTest::RunTest(const FString& Parameters)
{
	// 최고점을 쓴다 — 나중에 망친 판이 종합을 깎으면 안 된다.
	TArray<FSessionResult> H;
	H.Add(MakeSession(EGameModeId::Batting, NAME_None, 90.0f));
	H.Add(MakeSession(EGameModeId::Batting, NAME_None, 20.0f)); // 나중에 망친 판
	H.Add(MakeSession(EGameModeId::Batting, NAME_None, 40.0f));

	const FOverallScore R = UScoringService::ComputeOverall(
		H, UModeManager::BuildOverallCategories(), NoMultiplierConfig());

	TestEqual(TEXT("최고점(90)이 쓰인다"), R.Categories[0].BestScore, 90.0f, 0.01f);
	TestEqual(TEXT("환산 총점도 90"), R.Total, 90.0f, 0.01f);

	// 무효 세션은 최고점 후보에서 빠져야 한다.
	TArray<FSessionResult> H2;
	H2.Add(MakeSession(EGameModeId::Batting, NAME_None, 30.0f));
	H2.Add(MakeSession(EGameModeId::Batting, NAME_None, 99.0f, EDifficultyLevel::Amateur, /*bValid=*/false));

	const FOverallScore R2 = UScoringService::ComputeOverall(
		H2, UModeManager::BuildOverallCategories(), NoMultiplierConfig());
	TestEqual(TEXT("무효 세션(99)은 무시되고 30 이 최고점"), R2.Categories[0].BestScore, 30.0f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverallScoreDifficultyTest,
	"MotionBase.Overall.Difficulty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOverallScoreDifficultyTest::RunTest(const FString& Parameters)
{
	FOverallScoreConfig C; // 기본값: Beginner 0.8 / Amateur 1.0 / Pro 1.2, 상한 100
	const TArray<FOverallCategoryDef> Cats = UModeManager::BuildOverallCategories();

	// Beginner 는 만점을 내도 상한에 못 닿는다 (쉬운 난이도로 만점 방지).
	{
		TArray<FSessionResult> H;
		H.Add(MakeSession(EGameModeId::Batting, NAME_None, 100.0f, EDifficultyLevel::Beginner));
		const FOverallScore R = UScoringService::ComputeOverall(H, Cats, C);
		TestEqual(TEXT("Beginner 100 → 80 이 상한"), R.Categories[0].BestScore, 80.0f, 0.01f);
		TestEqual(TEXT("원점수는 그대로 100 으로 남는다"), R.Categories[0].RawBestScore, 100.0f, 0.01f);
	}

	// Pro 는 더 낮은 원점수로 상한에 닿는다 (도전 보상). clamp 되므로 100 을 넘지 않는다.
	{
		TArray<FSessionResult> H;
		H.Add(MakeSession(EGameModeId::Batting, NAME_None, 90.0f, EDifficultyLevel::Pro));
		const FOverallScore R = UScoringService::ComputeOverall(H, Cats, C);
		TestEqual(TEXT("Pro 90 × 1.2 = 108 → 100 으로 clamp"), R.Categories[0].BestScore, 100.0f, 0.01f);
		TestTrue(TEXT("총점이 100 을 넘지 않는다"), R.Total <= 100.0f + KINDA_SMALL_NUMBER);
	}

	// 같은 원점수라면 어려운 난이도가 더 높게 잡힌다.
	{
		TArray<FSessionResult> HBeg, HPro;
		HBeg.Add(MakeSession(EGameModeId::Batting, NAME_None, 70.0f, EDifficultyLevel::Beginner));
		HPro.Add(MakeSession(EGameModeId::Batting, NAME_None, 70.0f, EDifficultyLevel::Pro));

		const FOverallScore RB = UScoringService::ComputeOverall(HBeg, Cats, C);
		const FOverallScore RP = UScoringService::ComputeOverall(HPro, Cats, C);
		TestTrue(TEXT("같은 원점수면 Pro 가 더 높다"), RP.Total > RB.Total);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverallScoreDrillIdTest,
	"MotionBase.Overall.DrillIdMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOverallScoreDrillIdTest::RunTest(const FString& Parameters)
{
	// 은퇴한 ID("Backup" — 4지선다 퀴즈 시절)는 현재 종목에 잡히면 안 된다.
	// 잡히면 기준이 완전히 다른 옛 기록이 종합에 섞인다.
	TArray<FSessionResult> H;
	H.Add(MakeSession(EGameModeId::Defense, FName(TEXT("Backup")), 100.0f));

	const FOverallScore R = UScoringService::ComputeOverall(
		H, UModeManager::BuildOverallCategories(), NoMultiplierConfig());

	TestFalse(TEXT("은퇴한 DrillId 는 집계되지 않는다"), R.bValid);
	TestEqual(TEXT("실시 종목 0"), R.PlayedCount, 0);

	// 현재 ID 는 정상 집계.
	TArray<FSessionResult> H2;
	H2.Add(MakeSession(EGameModeId::Defense, UModeManager::GetDefenseDrillIdName(2), 100.0f));
	const FOverallScore R2 = UScoringService::ComputeOverall(
		H2, UModeManager::BuildOverallCategories(), NoMultiplierConfig());
	TestTrue(TEXT("현재 DrillId 는 집계된다"), R2.bValid);
	TestEqual(TEXT("수비 소계 = 50/3"), R2.DefensePoints, 50.0f / 3.0f, 0.01f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
