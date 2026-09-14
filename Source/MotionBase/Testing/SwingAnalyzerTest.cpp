#include "Misc/AutomationTest.h"
#include "Analysis/SwingAnalyzer.h"

#if WITH_DEV_AUTOMATION_TESTS

// USwingAnalyzer 순수 로직 단위 테스트 — 헤드셋 없이 헤드리스 실행.
// 실행: 에디터 Session Frontend > Automation > "MotionBase.Analysis.SwingAnalyzer"
//   또는 커맨드라인 -ExecCmds="Automation RunTests MotionBase.Analysis.SwingAnalyzer; Quit"
//
// 이 파일은 1-A(타이밍 창 비대칭)·1-C(배럴 판정↔시각 메시 불일치) 재발을 잡기 위해 신설됐다.

namespace
{
	/**
	 * 등속 직선 궤적. CrossTime 시각에 정확히 Origin 을 지나며, 방향·속력은 VelocityCmps.
	 * [IdealTime-SpanSec, IdealTime+SpanSec] 구간을 DtSec 간격으로 생성한다.
	 */
	TArray<FSwingSample> BuildLinearPass(double IdealTime, double SpanSec, double DtSec,
		double CrossTime, const FVector& Origin, const FVector& VelocityCmps, const FRotator& Rot)
	{
		TArray<FSwingSample> Samples;
		for (double T = IdealTime - SpanSec; T <= IdealTime + SpanSec + KINDA_SMALL_NUMBER; T += DtSec)
		{
			const FVector Loc = Origin + VelocityCmps * (T - CrossTime);
			Samples.Add(FSwingSample(T, Loc, Rot));
		}
		return Samples;
	}
}

// ── 1-A 회귀: 시간창 ±ContactTimeWindowSec 양끝 모두 컨택으로 잡혀야 한다 ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwingAnalyzerWindowSymmetricTest,
	"MotionBase.Analysis.SwingAnalyzer.WindowSymmetric",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSwingAnalyzerWindowSymmetricTest::RunTest(const FString& Parameters)
{
	const double IdealTime = 0.5;
	const FVector Velocity(500.0f, 0.0f, 0.0f); // 5 m/s — 동작 게이트(1.5 m/s) 여유 있게 통과.
	const FRotator Rot(0.0f, 0.0f, 0.0f);
	const float Margin = 0.02f; // 창 경계에서 살짝 안쪽 (표본 간격 대비 여유).

	// 창 후반 경계 근처(+0.30s, 창=0.32s) — 1-A 이전엔 PostContactDelaySec(0.12s)이 짧아
	// 이 구간 표본이 버퍼에 없어 아예 못 잡았다.
	{
		const double CrossTime = IdealTime + (USwingAnalyzer::ContactTimeWindowSec - Margin);
		const TArray<FSwingSample> Samples = BuildLinearPass(IdealTime, 0.6, 0.01, CrossTime,
			FVector::ZeroVector, Velocity, Rot);
		const FSwingMetrics M = USwingAnalyzer::AnalyzeSwing(Samples, FVector::ZeroVector, IdealTime);
		TestTrue(TEXT("창 후반 경계 근처 — 스윙 감지"), M.bSwingDetected);
		TestTrue(TEXT("창 후반 경계 근처 — 컨택 성공"), M.bContacted);
	}

	// 창 전반 경계 근처(-0.30s)도 대칭으로 잡혀야 한다.
	{
		const double CrossTime = IdealTime - (USwingAnalyzer::ContactTimeWindowSec - Margin);
		const TArray<FSwingSample> Samples = BuildLinearPass(IdealTime, 0.6, 0.01, CrossTime,
			FVector::ZeroVector, Velocity, Rot);
		const FSwingMetrics M = USwingAnalyzer::AnalyzeSwing(Samples, FVector::ZeroVector, IdealTime);
		TestTrue(TEXT("창 전반 경계 근처 — 스윙 감지"), M.bSwingDetected);
		TestTrue(TEXT("창 전반 경계 근처 — 컨택 성공"), M.bContacted);
	}

	return true;
}

// ── 정지한 배트는 동작 게이트에서 걸려야 한다 (공 바로 옆이어도) ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwingAnalyzerStationaryGateTest,
	"MotionBase.Analysis.SwingAnalyzer.StationaryGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSwingAnalyzerStationaryGateTest::RunTest(const FString& Parameters)
{
	const double IdealTime = 0.5;
	const FRotator Rot(0.0f, 0.0f, 0.0f);

	// 트래킹 노이즈 수준(±0.01cm)의 미세한 흔들림만 있는, 사실상 정지한 배트.
	// 공(원점) 바로 옆에 있어도 스윙으로 잡히면 "정지한 배트가 컨택 판정되는" 결함이 재발한 것.
	TArray<FSwingSample> Samples;
	for (double T = IdealTime - 0.3; T <= IdealTime + 0.3; T += 0.01)
	{
		const FVector Loc(0.01f * FMath::Sin(T * 37.0), 0.0f, 0.0f);
		Samples.Add(FSwingSample(T, Loc, Rot));
	}

	const FSwingMetrics M = USwingAnalyzer::AnalyzeSwing(Samples, FVector::ZeroVector, IdealTime);
	TestFalse(TEXT("정지한 배트 — 스윙 미감지"), M.bSwingDetected);
	TestFalse(TEXT("정지한 배트 — 컨택 아님"), M.bContacted);

	return true;
}

// ── 스윙 없음(TAKE) vs 헛스윙 — 둘 다 bContacted=false 이지만 bSwingDetected 로 갈라져야 한다 ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwingAnalyzerTakeVsWhiffTest,
	"MotionBase.Analysis.SwingAnalyzer.TakeVsWhiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSwingAnalyzerTakeVsWhiffTest::RunTest(const FString& Parameters)
{
	const double IdealTime = 0.5;
	const FRotator Rot(0.0f, 0.0f, 0.0f);

	// TAKE: 시간창(0.5±0.32 = [0.18, 0.82]) 안에 표본이 아예 없다 — 안 휘두름.
	{
		TArray<FSwingSample> Samples;
		for (double T = 0.0; T <= 0.1; T += 0.01)
		{
			Samples.Add(FSwingSample(T, FVector(1000.0f, 0.0f, 0.0f), Rot));
		}
		const FSwingMetrics M = USwingAnalyzer::AnalyzeSwing(Samples, FVector::ZeroVector, IdealTime);
		TestFalse(TEXT("TAKE — 스윙 미감지"), M.bSwingDetected);
		TestFalse(TEXT("TAKE — 컨택 아님"), M.bContacted);
	}

	// 헛스윙: 창 안에서 실제로 휘둘렀지만(동작 게이트 통과) 공에서 멀리 벗어났다(반경 밖).
	// TAKE 와 똑같이 bContacted=false 이지만 bSwingDetected=true 로 반드시 구분돼야 한다 —
	// 안 그러면 컨택률 분모(시도 수)에서 헛스윙이 통째로 빠진다.
	{
		const FVector Velocity(500.0f, 0.0f, 0.0f);
		const TArray<FSwingSample> Samples = BuildLinearPass(IdealTime, 0.3, 0.01, IdealTime,
			FVector(0.0f, 200.0f, 0.0f), Velocity, Rot); // 공에서 Y=200cm 떨어져서 지나감.
		const FSwingMetrics M = USwingAnalyzer::AnalyzeSwing(Samples, FVector::ZeroVector, IdealTime);
		TestTrue(TEXT("헛스윙 — 스윙 감지(TAKE 아님)"), M.bSwingDetected);
		TestFalse(TEXT("헛스윙 — 컨택 아님"), M.bContacted);
	}

	return true;
}

// ── 1-C 회귀: 시각 배럴 양끝(배트 끝 / 그립 쪽 배럴 시작점)으로 맞혀도 둘 다 컨택돼야 한다 ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwingAnalyzerBarrelEndpointsTest,
	"MotionBase.Analysis.SwingAnalyzer.BarrelEndpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSwingAnalyzerBarrelEndpointsTest::RunTest(const FString& Parameters)
{
	const double IdealTime = 0.5;
	// Yaw=90 → TipRotation.Vector() = (0,1,0) = 배럴 방향(그립→배트끝의 반대, 즉 SegStart 쪽 -Y).
	// 배트 자체는 +X 로 스윙 이동. 세그먼트 = [Tip-(0,BarrelLengthCm,0), Tip].
	const FRotator Rot(0.0f, 90.0f, 0.0f);
	const FVector Velocity(500.0f, 0.0f, 0.0f);

	auto AnalyzeAtBall = [&](const FVector& Ball) -> FSwingMetrics
	{
		const TArray<FSwingSample> Samples = BuildLinearPass(IdealTime, 0.3, 0.01, IdealTime,
			FVector::ZeroVector, Velocity, Rot);
		return USwingAnalyzer::AnalyzeSwing(Samples, Ball, IdealTime);
	};

	// 배트 끝(SegEnd = TipLocation).
	{
		const FSwingMetrics M = AnalyzeAtBall(FVector::ZeroVector);
		TestTrue(TEXT("배트 끝으로 맞힌 컨택 감지"), M.bContacted);
	}

	// 배럴 반대쪽 끝, 그립 쪽(SegStart = TipLocation - Forward*BarrelLengthCm).
	{
		const FSwingMetrics M = AnalyzeAtBall(FVector(0.0f, -USwingAnalyzer::BarrelLengthCm, 0.0f));
		TestTrue(TEXT("배럴 그립쪽 끝으로 맞힌 컨택 감지"), M.bContacted);
	}

	// 배럴 세그먼트 밖(반경 밖) — 컨택 아님. (세그먼트가 안 맞으면 이 거리도 잘못 판정된다.)
	{
		const float OutsideY = USwingAnalyzer::BarrelLengthCm + USwingAnalyzer::ContactRadiusCm + 5.0f;
		const FSwingMetrics M = AnalyzeAtBall(FVector(0.0f, -OutsideY, 0.0f));
		TestFalse(TEXT("배럴 밖은 컨택 아님"), M.bContacted);
	}

	return true;
}

// ── 트래커 제공 속도가 위치-미분 속도보다 우선해야 한다 ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwingAnalyzerReportedVelocityPriorityTest,
	"MotionBase.Analysis.SwingAnalyzer.ReportedVelocityPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSwingAnalyzerReportedVelocityPriorityTest::RunTest(const FString& Parameters)
{
	const double IdealTime = 0.5;
	const FVector PositionVelocity(500.0f, 0.0f, 0.0f);  // 위치 미분 기준 5 m/s (동작 게이트 통과용).
	const FVector ReportedVelocity(3000.0f, 0.0f, 0.0f); // 트래커 제공 30 m/s — 이 값이 이겨야 한다.
	const FRotator Rot(0.0f, 0.0f, 0.0f);

	TArray<FSwingSample> Samples = BuildLinearPass(IdealTime, 0.3, 0.01, IdealTime,
		FVector::ZeroVector, PositionVelocity, Rot);
	for (FSwingSample& S : Samples)
	{
		S.Velocity = ReportedVelocity; // 전 구간 동일한 트래커 제공 속도.
	}

	const FSwingMetrics M = USwingAnalyzer::AnalyzeSwing(Samples, FVector::ZeroVector, IdealTime);
	TestTrue(TEXT("컨택 감지"), M.bContacted);

	const float ExpectedMps = ReportedVelocity.Size() / 100.0f;
	TestEqual(TEXT("피크 속도는 트래커 제공값을 우선한다"), M.PeakSpeedMps, ExpectedMps, 0.01f);
	TestEqual(TEXT("컨택 속도는 트래커 제공값을 우선한다"), M.ContactSpeedMps, ExpectedMps, 0.01f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
