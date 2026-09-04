#include "Misc/AutomationTest.h"
#include "Core/Defense/Backup/BackupPlaybook.h"

#if WITH_DEV_AUTOMATION_TESTS

// UBackupPlaybook 저작 데이터 검증 — 커버리지 미달이 데모가 아니라 빌드에서 터지게 한다.
// 실행: 에디터 Session Frontend > Automation > "MotionBase.Backup.Playbook"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackupPlaybookCoverageTest,
	"MotionBase.Backup.Playbook.Coverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBackupPlaybookCoverageTest::RunTest(const FString& Parameters)
{
	const TArray<FBackupPlay> Plays = UBackupPlaybook::BuildPlayTable();
	const TArray<FBackupAssignmentRule> Rules = UBackupPlaybook::BuildRuleTable();

	TestTrue(TEXT("플레이 테이블이 비어있지 않음"), Plays.Num() > 0);
	TestTrue(TEXT("규칙 테이블이 비어있지 않음"), Rules.Num() > 0);

	// 포지션마다 PlayId 가 중복 없이 유일해야 무반복 주머니 로직이 안전하다.
	TSet<FName> SeenIds;
	bool bAllUnique = true;
	for (const FBackupPlay& Play : Plays)
	{
		if (SeenIds.Contains(Play.PlayId)) { bAllUnique = false; }
		SeenIds.Add(Play.PlayId);
	}
	TestTrue(TEXT("모든 PlayId 가 유일함"), bAllUnique);

	// 핵심 요건: 7개 포지션 전부 Hold 아닌 플레이가 최소 5개는 있어야 세션(6~10시행)에
	// 반복 없이 다양한 상황이 나올 수 있다.
	TArray<FString> Errors;
	const bool bCovered = UBackupPlaybook::ValidateCoverage(Plays, Rules, /*MinNonHoldPerPosition=*/5, Errors);
	for (const FString& Err : Errors)
	{
		AddError(Err);
	}
	TestTrue(TEXT("7개 포지션 전부 최소 커버리지 충족"), bCovered);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackupPlaybookResolveTest,
	"MotionBase.Backup.Playbook.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBackupPlaybookResolveTest::RunTest(const FString& Parameters)
{
	const TArray<FBackupAssignmentRule> Rules = UBackupPlaybook::BuildRuleTable();

	// 자기충돌 방지 — 저작 과정에서 실제로 두 번 잡았던 결함(설계 노트)이 재발하지 않는지.
	// "2루수가 처리해 2루로 던지는 병살"에서 2루수 본인이 2루를 커버한다고 나오면 안 된다.
	{
		FBackupPlay Play;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Second;
		Play.bHasThrowTo = true;
		Play.ThrowTo = EBaseType::Second;

		const FBackupAssignmentRule R = UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::Second);
		TestEqual(TEXT("2루수가 처리한 공을 2루수 본인이 커버하지 않음(Hold)"),
			static_cast<uint8>(R.Role), static_cast<uint8>(EBackupRole::Hold));
	}

	// 같은 상황에서 유격수는 커버해야 한다 (본인이 던진 게 아니므로).
	{
		FBackupPlay Play;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Second;
		Play.bHasThrowTo = true;
		Play.ThrowTo = EBaseType::Second;

		const FBackupAssignmentRule R = UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::Short);
		TestEqual(TEXT("2루수가 처리한 병살은 유격수가 2루 커버"),
			static_cast<uint8>(R.Role), static_cast<uint8>(EBackupRole::CoverBase));
		TestEqual(TEXT("커버 대상 = 2루"), static_cast<uint8>(R.AnchorBase), static_cast<uint8>(EBaseType::Second));
	}

	// ThrowTo 가 없는 플레이(BackUpFielder 전용)에 ThrowTo 필터 규칙이 잘못 걸리지 않는지 —
	// 예전에 더미값(Home)을 썼다가 실제로 겪었던 자기충돌 버그의 회귀 테스트.
	{
		FBackupPlay Play;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Right;
		Play.bHasThrowTo = false; // 송구 목적지 개념이 없는 플레이.

		const FBackupAssignmentRule R = UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::First);
		TestEqual(TEXT("ThrowTo 없는 플레이엔 ThrowTo 필터 규칙이 안 걸림(1루수는 Hold)"),
			static_cast<uint8>(R.Role), static_cast<uint8>(EBackupRole::Hold));
	}

	// 매칭되는 규칙이 전혀 없어도 항상 유효한 값(Hold + 설명 문구)을 돌려줘야 한다.
	{
		FBackupPlay EmptyPlay;
		const FBackupAssignmentRule R = UBackupPlaybook::Resolve(Rules, EmptyPlay, EFieldPosition::Third);
		TestEqual(TEXT("매칭 없으면 기본 Hold"), static_cast<uint8>(R.Role), static_cast<uint8>(EBackupRole::Hold));
		TestFalse(TEXT("기본 Hold 도 설명 문구가 비어있지 않음"), R.Explain.IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackupPlaybookBuildTrialTest,
	"MotionBase.Backup.Playbook.BuildTrial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBackupPlaybookBuildTrialTest::RunTest(const FString& Parameters)
{
	const FBaseballField Field;
	const TArray<FBackupPlay> Plays = UBackupPlaybook::BuildPlayTable();
	const TArray<FBackupAssignmentRule> Rules = UBackupPlaybook::BuildRuleTable();

	// 비-Hold 시행: 후보 목록에 정답이 반드시 포함돼 있어야 한다 (인덱스가 안 깨졌는지).
	int32 CheckedNonHold = 0;
	for (EFieldPosition Pos : UBackupPlaybook::AllPositions())
	{
		for (const FBackupPlay& Play : Plays)
		{
			if (!UBackupPlaybook::IsEligibleForPosition(Rules, Play, Pos)) { continue; }

			const FBackupTrial Trial = UBackupPlaybook::BuildTrial(Field, Rules, Pos, Play);
			TestFalse(TEXT("비-Hold 시행"), Trial.bIsHoldTrial);
			TestTrue(TEXT("정답 후보 인덱스가 유효 범위"),
				Trial.CandidateZones.IsValidIndex(Trial.CorrectCandidateIndex));
			TestTrue(TEXT("제한 시간이 양수"), Trial.TimeLimitSec > 0.0f);
			++CheckedNonHold;
		}
	}
	TestTrue(TEXT("비-Hold 시행을 최소 한 번은 검증함"), CheckedNonHold > 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
