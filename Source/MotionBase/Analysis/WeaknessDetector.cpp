#include "Analysis/WeaknessDetector.h"

FText UWeaknessDetector::GetAxisDisplayName(EWeaknessAxis Axis)
{
	switch (Axis)
	{
	case EWeaknessAxis::ContactRate:     return FText::FromString(TEXT("컨택률"));
	case EWeaknessAxis::Timing:          return FText::FromString(TEXT("타이밍"));
	case EWeaknessAxis::ContactAccuracy: return FText::FromString(TEXT("컨택 정확도"));
	case EWeaknessAxis::BatSpeed:        return FText::FromString(TEXT("배트 스피드"));
	case EWeaknessAxis::Consistency:     return FText::FromString(TEXT("일관성"));
	default:                             return FText::FromString(TEXT("알 수 없음"));
	}
}

FWeaknessReport UWeaknessDetector::DetectSwing(const TArray<FSwingMetrics>& History, const FScoringConfig& Config)
{
	FWeaknessReport Report;
	Report.Mode = EGameModeId::Batting;
	Report.AttemptCount = History.Num();
	Report.bUncalibrated = !Config.bCalibrated;

	if (History.Num() == 0)
	{
		return Report; // bValid=false
	}

	// TODO(캘리브레이션): 아래 기준값은 실측 데이터로 조정 (하드코딩 확정 금지).
	constexpr float GoodContactRate    = 0.60f; // 이 이상이면 컨택률 약점 아님
	constexpr float MinReportSeverity  = 0.15f; // 이 미만 심각도는 리포트에서 제외

	// 한 축을 리포트에 추가 (심각도 문턱 통과 시).
	auto AddAxis = [&Report](EWeaknessAxis Axis, float Score, const FString& Evidence)
	{
		const float ClampedScore = FMath::Clamp(Score, 0.0f, 1.0f);
		const float Severity = 1.0f - ClampedScore;
		if (Severity >= MinReportSeverity)
		{
			FWeakness W;
			W.Axis = Axis;
			W.Score = ClampedScore;
			W.Severity = Severity;
			W.Evidence = Evidence;
			Report.Weaknesses.Add(W);
		}
	};

	// ── 집계 (컨택한 스윙만 물리 지표 평균에 넣는다) ──
	int32 Contacted = 0;
	float SumTimingAbs = 0.0f, SumDist = 0.0f, SumSpeed = 0.0f;
	for (const FSwingMetrics& M : History)
	{
		if (M.bContacted)
		{
			++Contacted;
			SumTimingAbs += FMath::Abs(M.TimingErrorSeconds);
			SumDist += M.ContactDistanceCm;
			SumSpeed += M.ContactSpeedMps;
		}
	}
	Report.ContactCount = Contacted;
	const float ContactRate = static_cast<float>(Contacted) / History.Num();

	// 1) 컨택률 — 헛스윙 포함 전체 기준
	{
		const float Score = ContactRate / FMath::Max(GoodContactRate, KINDA_SMALL_NUMBER);
		AddAxis(EWeaknessAxis::ContactRate, Score,
			FString::Printf(TEXT("컨택률 %d/%d (%.0f%%)"), Contacted, History.Num(), ContactRate * 100.0f));
	}

	if (Contacted > 0)
	{
		const float AvgTimingAbs = SumTimingAbs / Contacted;
		const float AvgDist = SumDist / Contacted;
		const float AvgSpeed = SumSpeed / Contacted;

		// 2) 타이밍 — 가우시안 감쇠 (채점과 동일한 σ)
		const float Sigma = FMath::Max(Config.TimingSigmaSeconds, KINDA_SMALL_NUMBER);
		const float TimingScore = FMath::Exp(-(AvgTimingAbs * AvgTimingAbs) / (2.0f * Sigma * Sigma));
		AddAxis(EWeaknessAxis::Timing, TimingScore,
			FString::Printf(TEXT("평균 타이밍 오차 %.0f ms (목표 ±%.0f ms)"), AvgTimingAbs * 1000.0f, Sigma * 1000.0f));

		// 3) 컨택 정확도 — 스위트스팟 거리 선형 감쇠
		const float DistScore = FMath::Clamp(1.0f - (AvgDist / FMath::Max(Config.MaxContactDistanceCm, 1.0f)), 0.0f, 1.0f);
		AddAxis(EWeaknessAxis::ContactAccuracy, DistScore,
			FString::Printf(TEXT("평균 컨택 거리 %.1f cm (0에 가까울수록 좋음)"), AvgDist));

		// 4) 배트 스피드 — 목표 대비 정규화
		const float Range = FMath::Max(Config.TargetBatSpeedMps - Config.MinBatSpeedMps, KINDA_SMALL_NUMBER);
		const float SpeedScore = FMath::Clamp((AvgSpeed - Config.MinBatSpeedMps) / Range, 0.0f, 1.0f);
		AddAxis(EWeaknessAxis::BatSpeed, SpeedScore,
			FString::Printf(TEXT("평균 배트 속도 %.1f m/s (목표 %.0f m/s)"), AvgSpeed, Config.TargetBatSpeedMps));
	}

	// 5) 일관성 — 컨택한 스윙들의 정확도 표준편차 (채점 계층 재사용)
	if (Contacted >= 2)
	{
		TArray<float> Acc;
		Acc.Reserve(Contacted);
		for (const FSwingMetrics& M : History)
		{
			if (M.bContacted)
			{
				Acc.Add(UScoringService::EvalAccuracy(M, Config));
			}
		}

		float Mean = 0.0f;
		for (float A : Acc) { Mean += A; }
		Mean /= Acc.Num();

		float Variance = 0.0f;
		for (float A : Acc) { Variance += FMath::Square(A - Mean); }
		Variance /= Acc.Num();
		const float StdDev = FMath::Sqrt(Variance);

		const float StdMax = FMath::Max(Config.ConsistencySigmaMax, KINDA_SMALL_NUMBER);
		const float ConScore = FMath::Clamp(1.0f - (StdDev / StdMax), 0.0f, 1.0f);
		AddAxis(EWeaknessAxis::Consistency, ConScore,
			FString::Printf(TEXT("정확도 편차 %.2f (작을수록 안정적)"), StdDev));
	}

	// 심각도 내림차순 — 가장 시급한 약점이 앞으로
	Report.Weaknesses.Sort([](const FWeakness& A, const FWeakness& B) { return A.Severity > B.Severity; });
	Report.bValid = true;
	return Report;
}

FString UWeaknessDetector::SummarizeReport(const FWeaknessReport& Report)
{
	if (!Report.bValid)
	{
		return TEXT("분석할 스윙 기록이 없습니다.");
	}

	FString Out = FString::Printf(TEXT("시도 %d회, 컨택 %d회.\n"), Report.AttemptCount, Report.ContactCount);

	if (Report.Weaknesses.Num() == 0)
	{
		Out += TEXT("두드러진 약점이 없습니다 — 전반적으로 안정적입니다.");
		return Out;
	}

	Out += TEXT("약점 (시급한 순):\n");
	for (const FWeakness& W : Report.Weaknesses)
	{
		Out += FString::Printf(TEXT("- %s: %s (수행도 %.2f)\n"),
			*GetAxisDisplayName(W.Axis).ToString(), *W.Evidence, W.Score);
	}

	if (Report.bUncalibrated)
	{
		Out += TEXT("(※ 점수 기준 미보정 — 참고용 수치)");
	}
	return Out;
}
