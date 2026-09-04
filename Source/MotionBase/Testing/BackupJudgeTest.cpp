#include "Misc/AutomationTest.h"
#include "Core/Defense/Backup/BackupJudge.h"

#if WITH_DEV_AUTOMATION_TESTS

// FBackupJudge 순수 로직 단위 테스트 — 합성 좌표만으로 헤드리스 실행.
// 실행: 에디터 Session Frontend > Automation > "MotionBase.Backup.Judge"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackupJudgeOnsetTest,
	"MotionBase.Backup.Judge.Onset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBackupJudgeOnsetTest::RunTest(const FString& Parameters)
{
	// 둘 다 미달이면 아직 커밋하면 안 된다 (트랙패드 노이즈로 첫 프레임에 방향을 정하지 않기 위함).
	TestFalse(TEXT("변위·시간 둘 다 미달 → 미커밋"),
		FBackupJudge::ShouldCommitHeading(50.0f, 0.1f, /*CommitDist=*/200.0f, /*CommitTime=*/0.35f));

	// 누적 변위가 먼저 차면 커밋.
	TestTrue(TEXT("변위 충분 → 커밋"),
		FBackupJudge::ShouldCommitHeading(220.0f, 0.1f, 200.0f, 0.35f));

	// 게이트 경과 시간이 먼저 차도 커밋 (거의 안 움직였어도 시간이 지나면 확정).
	TestTrue(TEXT("게이트 경과 시간 충분 → 커밋"),
		FBackupJudge::ShouldCommitHeading(10.0f, 0.4f, 200.0f, 0.35f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackupJudgeDirectionTest,
	"MotionBase.Backup.Judge.Direction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBackupJudgeDirectionTest::RunTest(const FString& Parameters)
{
	// 후보 셋: 0°(정답), 90°, 180°. 정확히 0°로 움직이면 0번이 최근접, 마진은 90°.
	{
		const TArray<FVector2D> Candidates = { FVector2D(1, 0), FVector2D(0, 1), FVector2D(-1, 0) };
		float Margin = 0.0f;
		const int32 Idx = FBackupJudge::NearestCandidate(FVector2D(1, 0), Candidates, Margin);
		TestEqual(TEXT("정확히 0번 방향으로 움직이면 0번이 최근접"), Idx, 0);
		TestEqual(TEXT("0° vs 90° 후보 → 마진 90°"), Margin, 90.0f, 1.0f);
	}

	// 애매한 경우: 45° 로 움직이면 0°(0번)와 90°(1번) 사이 — 마진이 좁아야(0에 가까워야) 한다.
	{
		const TArray<FVector2D> Candidates = { FVector2D(1, 0), FVector2D(0, 1) };
		float Margin = 0.0f;
		const FVector2D Diagonal = FVector2D(1, 1).GetSafeNormal();
		FBackupJudge::NearestCandidate(Diagonal, Candidates, Margin);
		TestTrue(TEXT("45° 대각선은 두 후보 사이라 마진이 매우 좁음(<5°)"), Margin < 5.0f);
	}

	// 후보가 하나뿐이면 무조건 확정(마진 최대) — 예: 어떤 포지션이 job 을 하나만 갖는 경우.
	{
		const TArray<FVector2D> Candidates = { FVector2D(0, -1) };
		float Margin = 0.0f;
		const int32 Idx = FBackupJudge::NearestCandidate(FVector2D(1, 0), Candidates, Margin);
		TestEqual(TEXT("후보 1개면 항상 그 인덱스"), Idx, 0);
		TestTrue(TEXT("후보 1개면 마진이 크게 잡힘(항상 확정)"), Margin > 90.0f);
	}

	// 후보가 없으면 INDEX_NONE.
	{
		float Margin = 0.0f;
		const int32 Idx = FBackupJudge::NearestCandidate(FVector2D(1, 0), TArray<FVector2D>(), Margin);
		TestEqual(TEXT("후보 없음 → INDEX_NONE"), Idx, INDEX_NONE);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackupJudgeArrivalTest,
	"MotionBase.Backup.Judge.Arrival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBackupJudgeArrivalTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("존 안이면 항상 성공"),
		static_cast<uint8>(FBackupJudge::JudgeArrival(true, false, 100.0f, 5.0f)),
		static_cast<uint8>(EBackupOutcome::Covered));

	TestEqual(TEXT("방향은 맞았는데 시간 초과 → TooSlow"),
		static_cast<uint8>(FBackupJudge::JudgeArrival(false, true, 5.0f, 5.0f)),
		static_cast<uint8>(EBackupOutcome::TooSlow));

	TestEqual(TEXT("방향도 틀리고 시간 초과 → WrongZone"),
		static_cast<uint8>(FBackupJudge::JudgeArrival(false, false, 5.0f, 5.0f)),
		static_cast<uint8>(EBackupOutcome::WrongZone));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackupJudgePathEfficiencyTest,
	"MotionBase.Backup.Judge.PathEfficiency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBackupJudgePathEfficiencyTest::RunTest(const FString& Parameters)
{
	// 직선으로 정확히 걸었으면 효율 1.0.
	TestEqual(TEXT("직선 이동 → 효율 1.0"), FBackupJudge::PathEfficiency(1000.0f, 1000.0f), 1.0f, 0.01f);

	// 헤매서 두 배 길게 걸었으면 효율 0.5.
	TestEqual(TEXT("두 배 길게 걸음 → 효율 0.5"), FBackupJudge::PathEfficiency(1000.0f, 2000.0f), 0.5f, 0.01f);

	// 이동이 없었으면(경로 길이 0) 무효(-1) — 0으로 나누기 방지 + "이동 안 함"과 "완벽한 직선"을 구분.
	TestEqual(TEXT("이동 없음 → -1(무효)"), FBackupJudge::PathEfficiency(0.0f, 0.0f), -1.0f, 0.01f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
