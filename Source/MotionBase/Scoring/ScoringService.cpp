#include "Scoring/ScoringService.h"

float UScoringService::EvalAccuracy(const FSwingMetrics& M, const FScoringConfig& Config)
{
	if (!M.bContacted)
	{
		return 0.0f;
	}

	// 타이밍: 가우시안 감쇠  exp(-(err^2) / (2σ^2))
	const float Sigma = FMath::Max(Config.TimingSigmaSeconds, KINDA_SMALL_NUMBER);
	const float TimingScore = FMath::Exp(-(M.TimingErrorSeconds * M.TimingErrorSeconds) / (2.0f * Sigma * Sigma));

	// 컨택 거리: 0cm=1, MaxDist=0 선형 감쇠
	const float DistScore = FMath::Clamp(1.0f - (M.ContactDistanceCm / FMath::Max(Config.MaxContactDistanceCm, 1.0f)), 0.0f, 1.0f);

	return TimingScore * DistScore;
}

float UScoringService::EvalEfficiency(const FSwingMetrics& M, const FScoringConfig& Config)
{
	const float Range = FMath::Max(Config.TargetBatSpeedMps - Config.MinBatSpeedMps, KINDA_SMALL_NUMBER);
	return FMath::Clamp((M.ContactSpeedMps - Config.MinBatSpeedMps) / Range, 0.0f, 1.0f);
}

FScoreResult UScoringService::ScoreSwing(const FSwingMetrics& Metrics, const FScoringConfig& Config)
{
	FScoreResult R;
	R.Accuracy = EvalAccuracy(Metrics, Config);
	R.Efficiency = EvalEfficiency(Metrics, Config);
	R.Consistency = 1.0f; // 단일 스윙은 편차 정의 불가 → 만점 처리(세션 채점에서 실측)

	R.TotalScore = 100.0f * (
		Config.WeightAccuracy * R.Accuracy +
		Config.WeightEfficiency * R.Efficiency +
		Config.WeightConsistency * R.Consistency);

	R.bValid = Metrics.bContacted;
	R.bUncalibrated = !Config.bCalibrated;

	// AI 피드백 프롬프트 입력용 세부 지표
	R.Details.Add(TEXT("ContactSpeedMps"), Metrics.ContactSpeedMps);
	R.Details.Add(TEXT("PeakSpeedMps"), Metrics.PeakSpeedMps);
	R.Details.Add(TEXT("ContactDistanceCm"), Metrics.ContactDistanceCm);
	R.Details.Add(TEXT("TimingErrorSeconds"), Metrics.TimingErrorSeconds);

	return R;
}

FScoreResult UScoringService::ScoreSession(const TArray<FSwingMetrics>& History, const FScoringConfig& Config)
{
	FScoreResult R;
	if (History.Num() == 0)
	{
		return R;
	}

	// 축별 평균
	float SumAcc = 0.0f, SumEff = 0.0f;
	TArray<float> AccSamples;
	AccSamples.Reserve(History.Num());
	for (const FSwingMetrics& M : History)
	{
		const float A = EvalAccuracy(M, Config);
		const float E = EvalEfficiency(M, Config);
		SumAcc += A;
		SumEff += E;
		AccSamples.Add(A);
	}
	const int32 N = History.Num();
	R.Accuracy = SumAcc / N;
	R.Efficiency = SumEff / N;

	// 일관성: 정확도 축 표준편차가 작을수록 고득점. consistency = 1 - clamp(std / stdMax)
	float Mean = R.Accuracy;
	float Variance = 0.0f;
	for (float A : AccSamples)
	{
		Variance += FMath::Square(A - Mean);
	}
	Variance /= N;
	const float StdDev = FMath::Sqrt(Variance);

	const float StdMax = FMath::Max(Config.ConsistencySigmaMax, KINDA_SMALL_NUMBER);
	R.Consistency = FMath::Clamp(1.0f - (StdDev / StdMax), 0.0f, 1.0f);

	R.TotalScore = 100.0f * (
		Config.WeightAccuracy * R.Accuracy +
		Config.WeightEfficiency * R.Efficiency +
		Config.WeightConsistency * R.Consistency);

	R.bValid = true;
	R.bUncalibrated = !Config.bCalibrated;

	R.Details.Add(TEXT("AttemptCount"), static_cast<float>(N));
	R.Details.Add(TEXT("AccuracyStdDev"), StdDev);

	return R;
}
