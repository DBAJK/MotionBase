#include "Misc/AutomationTest.h"
#include "Core/Defense/Backup/BaseballField.h"

#if WITH_DEV_AUTOMATION_TESTS

// FBaseballField 순수 로직 단위 테스트 — 헤드셋·레벨 없이 헤드리스 실행.
// 실행: 에디터 Session Frontend > Automation > "MotionBase.Backup.Field"
//   또는 커맨드라인 -ExecCmds="Automation RunTests MotionBase.Backup.Field; Quit"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaseballFieldBaseCoordsTest,
	"MotionBase.Backup.Field.BaseCoords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBaseballFieldBaseCoordsTest::RunTest(const FString& Parameters)
{
	FBaseballField Field;

	// 베이스 간 거리가 90ft(2743.2cm) 인지 — 레벨(Content/Maps/BattingTest.umap)의
	// 실측 배치와 반드시 일치해야 하는 값이다.
	const FVector Home = Field.GetBaseLocation(EBaseType::Home);
	const FVector First = Field.GetBaseLocation(EBaseType::First);
	const FVector Second = Field.GetBaseLocation(EBaseType::Second);
	const FVector Third = Field.GetBaseLocation(EBaseType::Third);

	TestEqual(TEXT("홈-1루 거리 = 90ft"), static_cast<float>(FVector::Dist2D(Home, First)), Field.BasePathCm, 1.0f);
	TestEqual(TEXT("홈-3루 거리 = 90ft"), static_cast<float>(FVector::Dist2D(Home, Third)), Field.BasePathCm, 1.0f);
	TestEqual(TEXT("1루-2루 거리 = 90ft"), static_cast<float>(FVector::Dist2D(First, Second)), Field.BasePathCm, 1.0f);
	TestEqual(TEXT("홈-2루 거리 = 90ft * sqrt(2)"), static_cast<float>(FVector::Dist2D(Home, Second)),
		Field.BasePathCm * FMath::Sqrt(2.0f), 1.0f);

	// 1루는 +Y(우측), 3루는 -Y(좌측) — 방향 관례가 뒤집히면 모든 좌우 판정이 반대로 나온다.
	TestTrue(TEXT("1루는 +Y 쪽"), First.Y > 0.0f);
	TestTrue(TEXT("3루는 -Y 쪽"), Third.Y < 0.0f);
	TestTrue(TEXT("2루는 +X 쪽(정면)"), Second.X > Home.X);

	// 외야 세 자리는 전부 같은 깊이(OutfieldDepthCm)에서 대칭이어야 한다.
	const FVector LF = Field.GetFieldingSpot(EFieldPosition::Left);
	const FVector CF = Field.GetFieldingSpot(EFieldPosition::Center);
	const FVector RF = Field.GetFieldingSpot(EFieldPosition::Right);
	TestEqual(TEXT("좌익수 깊이 = OutfieldDepthCm"), static_cast<float>(FVector::Dist2D(Home, LF)), Field.OutfieldDepthCm, 1.0f);
	TestEqual(TEXT("중견수 깊이 = OutfieldDepthCm"), static_cast<float>(FVector::Dist2D(Home, CF)), Field.OutfieldDepthCm, 1.0f);
	TestEqual(TEXT("우익수 깊이 = OutfieldDepthCm"), static_cast<float>(FVector::Dist2D(Home, RF)), Field.OutfieldDepthCm, 1.0f);
	TestTrue(TEXT("좌익수는 3루 쪽(-Y)"), LF.Y < 0.0f);
	TestTrue(TEXT("우익수는 1루 쪽(+Y)"), RF.Y > 0.0f);
	TestEqual(TEXT("중견수는 정면(Y=0)"), static_cast<float>(CF.Y), 0.0f, 0.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaseballFieldZoneGeometryTest,
	"MotionBase.Backup.Field.ZoneGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBaseballFieldZoneGeometryTest::RunTest(const FString& Parameters)
{
	FBaseballField Field;

	// BackUpBase — 베이스 뒤로 물러난 자리. 우익수가 던져 1루로 가는 송구를 2루수가
	// 백업하면, 정답 중심은 "1루보다 2루수한테서 더 먼 쪽"(1루 바깥쪽)에 있어야 한다.
	{
		FBackupAssignmentRule Rule;
		Rule.Position = EFieldPosition::Second;
		Rule.Role = EBackupRole::BackUpBase;

		FBackupPlay Play;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Right;
		Play.ThrowTo = EBaseType::First;

		const FBackupZone Zone = Field.ResolveZone(Rule, Play);
		const FVector FirstBase = Field.GetBaseLocation(EBaseType::First);
		const FVector RFSpot = Field.GetFieldingSpot(EFieldPosition::Right);

		TestEqual(TEXT("BackUpBase 역할이 그대로 보존됨"), static_cast<uint8>(Zone.Role), static_cast<uint8>(EBackupRole::BackUpBase));
		TestTrue(TEXT("백업 중심이 베이스보다 던지는 사람에게서 더 멀다"),
			FVector::Dist2D(Zone.Center, RFSpot) > FVector::Dist2D(FirstBase, RFSpot));
		TestEqual(TEXT("백업 중심-베이스 거리 = BackupDistanceCm"),
			static_cast<float>(FVector::Dist2D(Zone.Center, FirstBase)), Field.BackupDistanceCm, 2.0f);
	}

	// CutoffRelay — 던지는 사람과 베이스 사이 선분 위, CutoffBandStart~End 구간에 있어야 한다.
	{
		FBackupAssignmentRule Rule;
		Rule.Position = EFieldPosition::First;
		Rule.Role = EBackupRole::CutoffRelay;

		FBackupPlay Play;
		Play.bHasThrowFrom = true;
		Play.ThrowFrom = EFieldPosition::Right;
		Play.ThrowTo = EBaseType::Home;

		const FBackupZone Zone = Field.ResolveZone(Rule, Play);
		const FVector From = Field.GetFieldingSpot(EFieldPosition::Right);
		const FVector To = Field.GetBaseLocation(EBaseType::Home);
		const FVector ExpectedA = FMath::Lerp(From, To, Field.CutoffBandStart);
		const FVector ExpectedB = FMath::Lerp(From, To, Field.CutoffBandEnd);

		TestEqual(TEXT("중계 구간 시작점이 예상과 일치"), static_cast<float>(FVector::Dist(Zone.SegmentA, ExpectedA)), 0.0f, 1.0f);
		TestEqual(TEXT("중계 구간 끝점이 예상과 일치"), static_cast<float>(FVector::Dist(Zone.SegmentB, ExpectedB)), 0.0f, 1.0f);

		// 선분 판정 — 구간 중점은 거리 0, 구간 밖(던진 사람 자리)은 회랑 밖이어야 한다.
		const FVector Midpoint = FMath::Lerp(Zone.SegmentA, Zone.SegmentB, 0.5f);
		TestTrue(TEXT("구간 중점은 존 안"), Zone.Contains(Midpoint));
		TestFalse(TEXT("던진 사람 본인 자리는 회랑 밖(구간이 안쪽으로 당겨져 있음)"), Zone.Contains(From));
	}

	// Hold — 자기 자리 자체가 정답이어야 한다 (다른 포지션을 물어봐도 안 섞여야 함).
	{
		FBackupAssignmentRule Rule;
		Rule.Position = EFieldPosition::Center;
		Rule.Role = EBackupRole::Hold;

		const FBackupZone Zone = Field.ResolveZone(Rule, FBackupPlay());
		TestEqual(TEXT("Hold 중심 = 그 포지션의 수비 위치"),
			static_cast<float>(FVector::Dist2D(Zone.Center, Field.GetFieldingSpot(EFieldPosition::Center))), 0.0f, 1.0f);
	}

	// 페어 지역 클램프 — 파울선(±45°) 밖 좌표를 넣으면 45° 안으로 눌려야 한다.
	{
		const FVector Foul(1000.0f, 5000.0f, 0.0f); // bearing ≈ 78.7° > 45°
		const FVector Clamped = Field.ClampToFairTerritory(Foul);
		const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Clamped.Y, Clamped.X));
		TestTrue(TEXT("클램프 후 파울선 안쪽"), FMath::Abs(BearingDeg) <= 45.01f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaseballFieldTimeLimitTest,
	"MotionBase.Backup.Field.TimeLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBaseballFieldTimeLimitTest::RunTest(const FString& Parameters)
{
	FBaseballField Field;

	// 거리가 길수록 제한 시간도 길어야 한다 — 상수 제한 시간을 쓰면 외야가 항상 실패한다
	// (설계 노트의 핵심 결함 #1). 단조증가만 확인한다 — 정확한 계수는 캘리브레이션 대상.
	const float Near = Field.DeriveTimeLimit(1500.0f);
	const float Mid = Field.DeriveTimeLimit(3500.0f);
	const float Far = Field.DeriveTimeLimit(6300.0f);

	TestTrue(TEXT("가까운 거리 < 중간 거리"), Near < Mid);
	TestTrue(TEXT("중간 거리 < 먼 거리"), Mid < Far);
	TestTrue(TEXT("모든 제한시간이 하한 이상"), Near >= Field.MinTimeLimitSec);

	// 거리 0(Hold 시행 근처)이어도 하한 밑으로는 안 내려간다.
	TestEqual(TEXT("거리 0 → 하한값"), Field.DeriveTimeLimit(0.0f), Field.MinTimeLimitSec, 0.01f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
