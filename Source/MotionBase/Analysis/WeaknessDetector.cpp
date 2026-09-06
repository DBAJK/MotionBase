#include "Analysis/WeaknessDetector.h"
#include "Algo/Reverse.h"

FText UWeaknessDetector::GetAxisDisplayName(EWeaknessAxis Axis)
{
	switch (Axis)
	{
	case EWeaknessAxis::ContactRate:     return FText::FromString(TEXT("컨택률"));
	case EWeaknessAxis::Timing:          return FText::FromString(TEXT("타이밍"));
	case EWeaknessAxis::ContactAccuracy: return FText::FromString(TEXT("컨택 정확도"));
	case EWeaknessAxis::BatSpeed:        return FText::FromString(TEXT("배트 스피드"));
	case EWeaknessAxis::Consistency:     return FText::FromString(TEXT("일관성"));
	case EWeaknessAxis::HipShoulderSeparation: return FText::FromString(TEXT("상하체 분리(X-factor)"));
	case EWeaknessAxis::HeadStability:   return FText::FromString(TEXT("머리 안정"));
	case EWeaknessAxis::KineticChain:    return FText::FromString(TEXT("운동 사슬"));
	case EWeaknessAxis::WeightShift:     return FText::FromString(TEXT("체중 이동"));
	case EWeaknessAxis::CatchReaction:   return FText::FromString(TEXT("Reaction speed"));
	case EWeaknessAxis::UpperBodyFlex:   return FText::FromString(TEXT("Upper-body flexibility"));
	case EWeaknessAxis::FootSpeed:       return FText::FromString(TEXT("Foot speed"));
	case EWeaknessAxis::ThrowAccuracy:   return FText::FromString(TEXT("Throwing accuracy"));
	case EWeaknessAxis::ArmStrength:     return FText::FromString(TEXT("Arm strength (throw velocity)"));
	case EWeaknessAxis::TransferQuick:   return FText::FromString(TEXT("Catch-to-throw transfer"));
	case EWeaknessAxis::BackupJudgment:  return FText::FromString(TEXT("Backup judgment"));
	case EWeaknessAxis::DecisionSpeed:   return FText::FromString(TEXT("Decision speed"));
	case EWeaknessAxis::RouteEfficiency: return FText::FromString(TEXT("Route efficiency"));
	default:                             return FText::FromString(TEXT("알 수 없음"));
	}
}

FText UWeaknessDetector::GetTrendDisplayName(EWeaknessTrend Trend)
{
	switch (Trend)
	{
	case EWeaknessTrend::New:       return FText::FromString(TEXT("신규"));
	case EWeaknessTrend::Improving: return FText::FromString(TEXT("개선 중"));
	case EWeaknessTrend::Stable:    return FText::FromString(TEXT("정체"));
	case EWeaknessTrend::Worsening: return FText::FromString(TEXT("악화"));
	default:                        return FText::FromString(TEXT("표본 부족"));
	}
}

FChronicWeaknessReport UWeaknessDetector::AnalyzeTrend(
	const TArray<FSessionResult>& History, EGameModeId Mode, int32 Window, FName DrillId)
{
	FChronicWeaknessReport Out;
	Window = FMath::Max(Window, 1);

	// 뒤(최신)에서부터 리포트가 유효한 해당 모드 세션을 Window 개까지 모아
	// 오래된→최신 순으로 뒤집는다. (기울기 계산을 시간순으로 하기 위함)
	TArray<const FWeaknessReport*> Recent;
	for (int32 i = History.Num() - 1; i >= 0 && Recent.Num() < Window; --i)
	{
		const FSessionResult& S = History[i];
		if (S.Mode != Mode || !S.Report.bValid)
		{
			continue;
		}
		// 세부 종목 필터 — 수비처럼 한 모드 안에 축이 다른 종목이 여럿이면 섞이면 안 된다.
		if (!DrillId.IsNone() && S.DrillId != DrillId)
		{
			continue;
		}
		Recent.Add(&S.Report);
	}
	Algo::Reverse(Recent);

	Out.SessionsAnalyzed = Recent.Num();
	if (Recent.Num() == 0)
	{
		return Out; // bValid=false
	}
	Out.bValid = true;

	// 약점 미등장 세션의 대체 수행도. 약점은 severity>=0.15(=score<=0.85)에서만
	// 기록되므로, 안 잡힌 세션은 "그럭저럭 괜찮았다"로 낙관 대체한다(과대평가 방지 0.9).
	constexpr float AbsentScore = 0.9f;

	// 창 안에 한 번이라도 약점으로 등장한 축들을 수집.
	TSet<EWeaknessAxis> Axes;
	for (const FWeaknessReport* R : Recent)
	{
		for (const FWeakness& W : R->Weaknesses)
		{
			Axes.Add(W.Axis);
		}
	}

	const int32 N = Recent.Num();
	for (EWeaknessAxis Axis : Axes)
	{
		FAxisTrend T;
		T.Axis = Axis;
		T.WindowSize = N;

		// 축의 세션별 수행도 시계열(오래된→최신). 등장 안 하면 AbsentScore.
		TArray<float> Series;
		Series.Reserve(N);
		int32 LastPresentIdx = -1;
		for (int32 i = 0; i < N; ++i)
		{
			float Score = AbsentScore;
			bool bPresent = false;
			for (const FWeakness& W : Recent[i]->Weaknesses)
			{
				if (W.Axis == Axis)
				{
					Score = W.Score;
					bPresent = true;
					break;
				}
			}
			if (bPresent)
			{
				++T.AppearanceCount;
				LastPresentIdx = i;
			}
			Series.Add(Score);
		}

		// 평균 수행도.
		float Sum = 0.0f;
		for (float V : Series) { Sum += V; }
		T.AverageScore = Sum / N;

		// 최소제곱 기울기 (x = 세션 인덱스 0..N-1). N<2 면 0.
		if (N >= 2)
		{
			const float MeanX = (N - 1) * 0.5f;
			float Num = 0.0f, Den = 0.0f;
			for (int32 i = 0; i < N; ++i)
			{
				const float Dx = i - MeanX;
				Num += Dx * (Series[i] - T.AverageScore);
				Den += Dx * Dx;
			}
			T.ScoreSlope = (Den > KINDA_SMALL_NUMBER) ? (Num / Den) : 0.0f;
		}

		// 만성 = 창의 절반 이상에서 등장 (표본 2 이상일 때만).
		T.bChronic = (N >= 2) && (T.AppearanceCount * 2 >= N);

		// 추세 분류.
		constexpr float SlopeEps = 0.03f; // 세션당 수행도 변화 임계
		if (N < 2)
		{
			T.Trend = EWeaknessTrend::Insufficient;
		}
		else if (T.AppearanceCount == 1 && LastPresentIdx == N - 1)
		{
			T.Trend = EWeaknessTrend::New; // 가장 최근 세션에서 처음 잡힘
		}
		else if (T.ScoreSlope > SlopeEps)
		{
			T.Trend = EWeaknessTrend::Improving;
		}
		else if (T.ScoreSlope < -SlopeEps)
		{
			T.Trend = EWeaknessTrend::Worsening;
		}
		else
		{
			T.Trend = EWeaknessTrend::Stable;
		}

		Out.Trends.Add(T);
	}

	// 만성 먼저, 그다음 등장 빈도 높은 순, 그다음 평균 수행도 낮은(나쁜) 순.
	Out.Trends.Sort([](const FAxisTrend& A, const FAxisTrend& B)
	{
		if (A.bChronic != B.bChronic)               { return A.bChronic; }
		if (A.AppearanceCount != B.AppearanceCount) { return A.AppearanceCount > B.AppearanceCount; }
		if (!FMath::IsNearlyEqual(A.AverageScore, B.AverageScore)) { return A.AverageScore < B.AverageScore; }
		return static_cast<uint8>(A.Axis) < static_cast<uint8>(B.Axis); // 결정론적 타이브레이크
	});

	return Out;
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
	constexpr float kMinReportSeverity = UWeaknessDetector::MinReportSeverity; // 모드 공통 문턱

	// 한 축을 리포트에 추가 (심각도 문턱 통과 시).
	auto AddAxis = [&Report](EWeaknessAxis Axis, float Score, const FString& Evidence)
	{
		const float ClampedScore = FMath::Clamp(Score, 0.0f, 1.0f);
		const float Severity = 1.0f - ClampedScore;
		if (Severity >= kMinReportSeverity)
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

void UWeaknessDetector::AppendBodyMechanicsWeaknesses(
	FWeaknessReport& Report,
	const TArray<FBodyMechanicsMetrics>& BodyHistory,
	const FBodyMechanicsScoringConfig& BodyConfig)
{
	// 신뢰도 게이트를 통과한 유효 지표만 평균에 넣는다.
	int32 N = 0;
	int32 ChainOrdered = 0;
	float SumSeparation = 0.0f, SumHeadTravel = 0.0f, SumWeightShift = 0.0f;
	for (const FBodyMechanicsMetrics& M : BodyHistory)
	{
		if (!M.bValid || M.Confidence < BodyConfig.MinConfidence)
		{
			continue;
		}
		++N;
		SumSeparation += M.HipShoulderSeparationDeg;
		SumHeadTravel += M.HeadTravelCm;
		SumWeightShift += M.WeightShiftCm;
		if (M.bKineticChainOrdered) { ++ChainOrdered; }
	}

	if (N == 0)
	{
		return; // 믿을 만한 신체역학 표본 없음 — 리포트를 건드리지 않는다.
	}

	// 미보정이면 리포트에 전파 (한 축이라도 미보정 기준이면 리포트 전체 미보정).
	Report.bUncalibrated = Report.bUncalibrated || !BodyConfig.bCalibrated;

	const float AvgSep = SumSeparation / N;
	const float AvgHead = SumHeadTravel / N;
	const float AvgShift = SumWeightShift / N;
	const float ChainRate = static_cast<float>(ChainOrdered) / N;

	// 심각도 문턱은 스윙 지표 판별과 동일하게 유지 (모드 공통 상수).
	constexpr float kMinReportSeverity = UWeaknessDetector::MinReportSeverity;
	auto AddAxis = [&Report](EWeaknessAxis Axis, float Score, const FString& Evidence)
	{
		const float ClampedScore = FMath::Clamp(Score, 0.0f, 1.0f);
		const float Severity = 1.0f - ClampedScore;
		if (Severity >= kMinReportSeverity)
		{
			FWeakness W;
			W.Axis = Axis;
			W.Score = ClampedScore;
			W.Severity = Severity;
			W.Evidence = Evidence;
			Report.Weaknesses.Add(W);
		}
	};

	// 1) 상하체 분리 (X-factor) — 목표 대비 정규화
	const float SepScore = AvgSep / FMath::Max(BodyConfig.TargetSeparationDeg, KINDA_SMALL_NUMBER);
	AddAxis(EWeaknessAxis::HipShoulderSeparation, SepScore,
		FString::Printf(TEXT("평균 X-factor %.0f° (목표 %.0f°)"), AvgSep, BodyConfig.TargetSeparationDeg));

	// 2) 머리 안정 — 이동량 선형 감쇠 (작을수록 좋음)
	const float HeadScore = 1.0f - (AvgHead / FMath::Max(BodyConfig.MaxHeadTravelCm, KINDA_SMALL_NUMBER));
	AddAxis(EWeaknessAxis::HeadStability, HeadScore,
		FString::Printf(TEXT("평균 머리 이동 %.1f cm (작을수록 안정)"), AvgHead));

	// 3) 운동 사슬 — 순서 정상 비율 그대로 점수
	AddAxis(EWeaknessAxis::KineticChain, ChainRate,
		FString::Printf(TEXT("체인 순서 정상 %d/%d (%.0f%%)"), ChainOrdered, N, ChainRate * 100.0f));

	// 4) 체중 이동 — 목표 대비 정규화
	const float ShiftScore = AvgShift / FMath::Max(BodyConfig.TargetWeightShiftCm, KINDA_SMALL_NUMBER);
	AddAxis(EWeaknessAxis::WeightShift, ShiftScore,
		FString::Printf(TEXT("평균 체중 이동 %.0f cm (목표 %.0f cm)"), AvgShift, BodyConfig.TargetWeightShiftCm));

	// 스윙 축과 합쳐 다시 심각도 내림차순 정렬.
	Report.Weaknesses.Sort([](const FWeakness& A, const FWeakness& B) { return A.Severity > B.Severity; });
}

FString UWeaknessDetector::SummarizeReport(const FWeaknessReport& Report)
{
	if (!Report.bValid)
	{
		return TEXT("No records to analyze.");
	}

	FString Out = FString::Printf(TEXT("Attempts %d, successes %d.\n"), Report.AttemptCount, Report.ContactCount);

	if (Report.Weaknesses.Num() == 0)
	{
		// 약점이 없어도 부가 근거(Notes)는 계속 실어야 한다 — 코칭이 "무엇이 잘 됐는지"를
		// 숫자로 짚을 수 있어야 하므로 여기서 끊지 않는다.
		Out += TEXT("No notable weaknesses - overall stable.\n");
	}
	else
	{
		Out += TEXT("Weaknesses (most urgent first):\n");
		for (const FWeakness& W : Report.Weaknesses)
		{
			Out += FString::Printf(TEXT("- %s: %s (performance %.2f)\n"),
				*GetAxisDisplayName(W.Axis).ToString(), *W.Evidence, W.Score);
		}
	}

	// 축으로 표현되지 않는 부가 근거 (타구 타입별 성공률·베이스별 정확도 등).
	// 드릴 선택엔 안 쓰이지만 코칭 문장에는 필요한 숫자다.
	if (Report.Notes.Num() > 0)
	{
		Out += TEXT("Breakdown:\n");
		for (const FString& N : Report.Notes)
		{
			Out += FString::Printf(TEXT("- %s\n"), *N);
		}
	}

	if (Report.bUncalibrated)
	{
		Out += TEXT("(* scoring uncalibrated - reference only)");
	}
	return Out;
}
