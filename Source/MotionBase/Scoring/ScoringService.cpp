#include "Scoring/ScoringService.h"
#include "Analysis/HitModel.h"

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
	if (!M.bContacted)
	{
		// 헛스윙은 효율 0 — 공을 못 맞혔다. (UHitModel::ExitVelocityMps 도 0을 돌려주지만
		// 방어적으로 여기서도 막는다.)
		return 0.0f;
	}

	// 효율 = 가상 타구 속도 / 만점 기준. 단순 배트 속도가 아니라 **컨택 품질까지 반영된**
	// 타구 속도로 채점한다 → 빗맞은 강스윙보다 정타가 높은 효율을 받는다.
	// 타구 모델이 유일한 물리 출처 (UI·연출과 같은 값).
	const float ExitVelocity = UHitModel::ExitVelocityMps(M, Config);
	const float Target = FMath::Max(Config.ExitVelocityTargetMps, KINDA_SMALL_NUMBER);
	return FMath::Clamp(ExitVelocity / Target, 0.0f, 1.0f);
}

FScoreResult UScoringService::ScoreSwing(const FSwingMetrics& Metrics, const FScoringConfig& Config)
{
	FScoreResult R;
	R.bUncalibrated = !Config.bCalibrated;

	// 세부 지표는 헛스윙이어도 피드백에 쓰이므로 항상 채운다.
	R.Details.Add(TEXT("ContactSpeedMps"), Metrics.ContactSpeedMps);
	R.Details.Add(TEXT("PeakSpeedMps"), Metrics.PeakSpeedMps);
	R.Details.Add(TEXT("ContactDistanceCm"), Metrics.ContactDistanceCm);
	R.Details.Add(TEXT("TimingErrorSeconds"), Metrics.TimingErrorSeconds);

	if (!Metrics.bContacted)
	{
		// 헛스윙 → 0점. 예전엔 효율(배트 속도)·일관성(1.0 고정)이 남아 최대 60점이 나왔다.
		R.bValid = false;
		return R; // Accuracy/Efficiency/Consistency/TotalScore = 0
	}

	R.Accuracy = EvalAccuracy(Metrics, Config);
	R.Efficiency = EvalEfficiency(Metrics, Config);

	// 단일 스윙은 편차를 정의할 수 없다 → 일관성 축을 빼고 정확도·효율 가중치를 재정규화한다.
	// (1.0 으로 채워 가중합에 넣으면 '공짜 25점'이 되어 단일 스윙 총점이 부풀려진다.)
	R.Consistency = 0.0f; // 미측정 — 세션 채점에서 실측
	const float W = FMath::Max(Config.WeightAccuracy + Config.WeightEfficiency, KINDA_SMALL_NUMBER);
	R.TotalScore = 100.0f * (Config.WeightAccuracy * R.Accuracy + Config.WeightEfficiency * R.Efficiency) / W;

	R.bValid = true;
	return R;
}

FScoreResult UScoringService::ScoreSession(const TArray<FSwingMetrics>& History, const FScoringConfig& Config)
{
	FScoreResult R;
	R.bUncalibrated = !Config.bCalibrated;
	if (History.Num() == 0)
	{
		return R; // bValid=false
	}

	// 축별 평균은 전체 시도로 낸다. 헛스윙은 정확도·효율 모두 0이므로 평균을 끌어내린다
	// → 컨택률이 자연스럽게 점수에 반영된다.
	// 일관성은 '컨택한' 스윙의 정확도 편차로만 잰다 (아래).
	float SumAcc = 0.0f, SumEff = 0.0f;
	TArray<float> ContactedAcc;
	ContactedAcc.Reserve(History.Num());
	for (const FSwingMetrics& M : History)
	{
		const float A = EvalAccuracy(M, Config);
		const float E = EvalEfficiency(M, Config);
		SumAcc += A;
		SumEff += E;
		if (M.bContacted)
		{
			ContactedAcc.Add(A);
		}
	}
	const int32 N = History.Num();
	R.Accuracy = SumAcc / N;
	R.Efficiency = SumEff / N;

	// 일관성: 컨택한 스윙들의 정확도 표준편차가 작을수록 고득점.
	// ⚠️ 헛스윙을 편차 계산에 넣으면 '전부 헛스윙 → 정확도 전부 0 → 편차 0 → 일관성 만점'이라는
	//    역설이 생긴다(한 번도 못 맞힌 사람이 가끔 맞히는 사람보다 일관성 높음). 그래서 제외한다.
	//    컨택 표본이 2개 미만이면 편차 정의 불가 → 일관성 축을 빼고 재정규화 (단일 스윙과 동일 원칙).
	const bool bHasConsistency = ContactedAcc.Num() >= 2;
	float StdDev = 0.0f;
	if (bHasConsistency)
	{
		float Mean = 0.0f;
		for (float A : ContactedAcc) { Mean += A; }
		Mean /= ContactedAcc.Num();

		float Variance = 0.0f;
		for (float A : ContactedAcc) { Variance += FMath::Square(A - Mean); }
		Variance /= ContactedAcc.Num();
		StdDev = FMath::Sqrt(Variance);

		const float StdMax = FMath::Max(Config.ConsistencySigmaMax, KINDA_SMALL_NUMBER);
		R.Consistency = FMath::Clamp(1.0f - (StdDev / StdMax), 0.0f, 1.0f);

		R.TotalScore = 100.0f * (
			Config.WeightAccuracy * R.Accuracy +
			Config.WeightEfficiency * R.Efficiency +
			Config.WeightConsistency * R.Consistency);
	}
	else
	{
		// 컨택 표본 <2 → 일관성 측정 불가. 전부 헛스윙이면 정확도·효율이 0이라 총점도 0.
		R.Consistency = 0.0f;
		const float W = FMath::Max(Config.WeightAccuracy + Config.WeightEfficiency, KINDA_SMALL_NUMBER);
		R.TotalScore = 100.0f * (Config.WeightAccuracy * R.Accuracy + Config.WeightEfficiency * R.Efficiency) / W;
	}

	R.bValid = true;

	R.Details.Add(TEXT("AttemptCount"), static_cast<float>(N));
	R.Details.Add(TEXT("ContactCount"), static_cast<float>(ContactedAcc.Num()));
	R.Details.Add(TEXT("AccuracyStdDev"), StdDev);

	return R;
}
