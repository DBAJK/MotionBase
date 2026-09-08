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

	// ── 내야 기본 로테이션 — "유격수 정면 땅볼, 1루 송구" 한 판에서 7개 포지션 전부 ──
	// 이 모드에서 가장 자주 나올 상황인데 예전엔 5개 포지션이 Hold 로 떨어졌다.
	{
		FBackupPlay Play;
		Play.BallZone = EBattedBallZone::InfieldLeft;
		Play.BallKind = EBattedBallKind::Grounder;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Short;
		Play.bHasThrowTo = true;
		Play.ThrowTo = EBaseType::First;

		auto Role = [&Rules, &Play](EFieldPosition Pos)
		{
			return static_cast<uint8>(UBackupPlaybook::Resolve(Rules, Play, Pos).Role);
		};
		auto Anchor = [&Rules, &Play](EFieldPosition Pos)
		{
			return static_cast<uint8>(UBackupPlaybook::Resolve(Rules, Play, Pos).AnchorBase);
		};

		TestEqual(TEXT("1루수는 1루 베이스 커버"), Role(EFieldPosition::First), static_cast<uint8>(EBackupRole::CoverBase));
		TestEqual(TEXT("1루수 커버 대상 = 1루"), Anchor(EFieldPosition::First), static_cast<uint8>(EBaseType::First));

		// 핵심 회귀: 예전엔 여기서 2루수가 "1루 뒤 백업"을 받았다.
		TestEqual(TEXT("2루수는 2루 커버 (1루 백업 아님)"), Role(EFieldPosition::Second), static_cast<uint8>(EBackupRole::CoverBase));
		TestEqual(TEXT("2루수 커버 대상 = 2루"), Anchor(EFieldPosition::Second), static_cast<uint8>(EBaseType::Second));

		TestEqual(TEXT("유격수는 본인이 처리 - Hold"), Role(EFieldPosition::Short), static_cast<uint8>(EBackupRole::Hold));
		TestEqual(TEXT("3루수는 3루 커버"), Role(EFieldPosition::Third), static_cast<uint8>(EBackupRole::CoverBase));
		TestEqual(TEXT("3루수 커버 대상 = 3루"), Anchor(EFieldPosition::Third), static_cast<uint8>(EBaseType::Third));
		TestEqual(TEXT("중견수는 2루 뒤 백업"), Role(EFieldPosition::Center), static_cast<uint8>(EBackupRole::BackUpBase));
		TestEqual(TEXT("우익수는 1루 뒤 백업"), Role(EFieldPosition::Right), static_cast<uint8>(EBackupRole::BackUpBase));
		TestEqual(TEXT("좌익수는 담당 없음 - Hold"), Role(EFieldPosition::Left), static_cast<uint8>(EBackupRole::Hold));
	}

	// 번트는 로테이션이 다르다 — 1루수가 대시하므로 1루 베이스는 2루수 몫.
	{
		FBackupPlay Play;
		Play.BallZone = EBattedBallZone::BuntThird;
		Play.BallKind = EBattedBallKind::Bunt;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Third;
		Play.bHasThrowTo = true;
		Play.ThrowTo = EBaseType::First;

		const FBackupAssignmentRule Second = UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::Second);
		TestEqual(TEXT("번트에선 2루수가 1루 커버"),
			static_cast<uint8>(Second.Role), static_cast<uint8>(EBackupRole::CoverBase));
		TestEqual(TEXT("번트 커버 대상 = 1루"),
			static_cast<uint8>(Second.AnchorBase), static_cast<uint8>(EBaseType::First));

		// 1루수는 대시해 들어가므로 베이스 커버 규칙에서 빠져야 한다.
		TestEqual(TEXT("번트에선 1루수가 베이스 커버를 받지 않음"),
			static_cast<uint8>(UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::First).Role),
			static_cast<uint8>(EBackupRole::Hold));
	}

	// 컷오프 사이드별 관례 — 좌익수 홈 송구는 3루수가 컷오프, 3루는 유격수가 커버.
	{
		FBackupPlay Play;
		Play.BallZone = EBattedBallZone::LeftLine;
		Play.BallKind = EBattedBallKind::LineDrive;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Left;
		Play.bHasThrowTo = true;
		Play.ThrowTo = EBaseType::Home;

		TestEqual(TEXT("좌익수 홈 송구 컷오프 = 3루수"),
			static_cast<uint8>(UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::Third).Role),
			static_cast<uint8>(EBackupRole::CutoffRelay));

		const FBackupAssignmentRule SS = UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::Short);
		TestEqual(TEXT("3루수가 나가면 유격수가 3루 커버"),
			static_cast<uint8>(SS.Role), static_cast<uint8>(EBackupRole::CoverBase));
		TestEqual(TEXT("커버 대상 = 3루"),
			static_cast<uint8>(SS.AnchorBase), static_cast<uint8>(EBaseType::Third));
	}

	// 외야 갭 백업 — 좌중간 타구를 중견수가 처리하면, 좌익수는 3루 뒤 백업(블랭킷 규칙)이
	// 아니라 중견수 뒤를 받쳐야 한다. 구체성 2(ThrowFrom+BallZone) > 1(ThrowTo) 로
	// 확정적으로 이겨야 하는 자리 — 동점이 되면 선언 순서에 좌우돼 조용히 뒤집힌다.
	{
		FBackupPlay Play;
		Play.BallZone = EBattedBallZone::LeftCenter;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Center;
		Play.bHasThrowTo = true;
		Play.ThrowTo = EBaseType::Third;

		const FBackupAssignmentRule R = UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::Left);
		TestEqual(TEXT("좌중간을 중견수가 잡으면 좌익수는 야수 백업"),
			static_cast<uint8>(R.Role), static_cast<uint8>(EBackupRole::BackUpFielder));
		TestEqual(TEXT("받치는 대상 = 중견수"),
			static_cast<uint8>(R.AnchorFielder), static_cast<uint8>(EFieldPosition::Center));
	}

	// 반대로 **갭이 아닌** 3루 송구에서는 좌익수가 기존대로 3루 뒤를 백업해야 한다
	// (갭 규칙이 블랭킷을 통째로 잡아먹으면 안 된다).
	{
		FBackupPlay Play;
		Play.BallZone = EBattedBallZone::Center;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Center;
		Play.bHasThrowTo = true;
		Play.ThrowTo = EBaseType::Third;

		const FBackupAssignmentRule R = UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::Left);
		TestEqual(TEXT("갭이 아니면 좌익수는 3루 뒤 백업 유지"),
			static_cast<uint8>(R.Role), static_cast<uint8>(EBackupRole::BackUpBase));
	}

	// bExcludeSelfThrow — 좌익수가 직접 처리해 3루로 던지는 상황에 "3루 뒤 백업" 블랭킷이
	// 걸리면 자기 송구를 자기가 백업하는 자기충돌이다. 예전엔 그런 플레이를 안 만드는
	// 회피책으로 막았고, 지금은 규칙 필터가 막는다.
	{
		FBackupPlay Play;
		Play.BallZone = EBattedBallZone::LeftLine;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Left;
		Play.bHasThrowTo = true;
		Play.ThrowTo = EBaseType::Third;

		const FBackupAssignmentRule R = UBackupPlaybook::Resolve(Rules, Play, EFieldPosition::Left);
		TestEqual(TEXT("좌익수 본인의 3루 송구를 본인이 백업하지 않음(Hold)"),
			static_cast<uint8>(R.Role), static_cast<uint8>(EBackupRole::Hold));
	}

	// 매칭되는 규칙이 전혀 없어도 항상 유효한 값(Hold + 설명 문구)을 돌려줘야 한다.
	// ⚠️ 기본 생성 FBackupPlay 를 쓰면 안 된다 — bHasThrowTo=true, ThrowTo=First 가 기본값이라
	//    "1루로 송구가 가는 플레이"로 읽히고, 내야 기본 로테이션 규칙에 정상적으로 걸린다.
	//    송구 자체가 없는 플레이를 만들어야 진짜 폴백 경로를 검증한다.
	{
		FBackupPlay EmptyPlay;
		EmptyPlay.bHasThrowFrom = false;
		EmptyPlay.bHasThrowTo   = false;

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
