#include "Scoring/ScoringService.h"
#include "Analysis/HitModel.h"

void FScoringConfig::ApplyDifficulty(EDifficultyLevel Level)
{
	// 판정선만 움직인다 (안타 비거리 / 홈런 비거리 / 파울 라인).
	// 기준(Pro)은 간이 모델의 원래 값 — 아래로 갈수록 "잘 맞혔다"로 쳐 주는 폭이 넓어진다.
	switch (Level)
	{
	case EDifficultyLevel::Beginner:
		// 처음 잡는 사람: 앞으로 날아가기만 하면 안타로 불러 준다.
		HitDistanceM     = 10.0f;
		HomeRunDistanceM = 60.0f;
		FoulLineDeg      = 58.0f;
		break;

	case EDifficultyLevel::Pro:
		HitDistanceM     = 30.0f;
		HomeRunDistanceM = 100.0f;
		FoulLineDeg      = 45.0f;
		break;

	default: // Amateur — 기본 시연 난이도
		// VR 컨트롤러 스윙의 배트 속도(대략 15~22 m/s)에서 **정타면 안타가 나오도록** 맞췄다.
		//   배트 19 m/s · 컨택 오차 10cm → 타구속도 ≈ 20 m/s → 비거리 ≈ 16m
		// 예전 기준(30m)은 타구속도 26 m/s(=프로급 배트 속도)를 요구해서, 잘 맞혀도
		// 대부분 '아웃'으로 찍혔다 — 간이 모델이 투구 속도를 안 더하는 탓이다.
		HitDistanceM     = 16.0f;
		HomeRunDistanceM = 75.0f;
		FoulLineDeg      = 52.0f;
		break;
	}
}

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

FScoreResult UScoringService::ScoreDefenseAttempt(bool bSuccess, const TMap<FName, float>& Details)
{
	FScoreResult R;
	R.Accuracy   = bSuccess ? 1.0f : 0.0f;
	R.Efficiency = 0.0f;   // 미정의 — 수비 3축 모델 없음
	R.Consistency = 0.0f;  // 미정의 — 단일 시도
	R.TotalScore = bSuccess ? 100.0f : 0.0f;
	R.bValid = true;       // 시도 자체는 유효하다 (실패도 데이터다)
	R.bUncalibrated = true;
	R.Details = Details;
	return R;
}

FScoreResult UScoringService::ScoreDefenseSession(int32 SuccessCount, int32 AttemptCount)
{
	FScoreResult R;
	R.bUncalibrated = true;

	if (AttemptCount <= 0)
	{
		return R; // bValid=false
	}

	const float Rate = static_cast<float>(SuccessCount) / AttemptCount;
	R.Accuracy = Rate;
	R.TotalScore = Rate * 100.0f;
	R.bValid = true;
	R.Details.Add(TEXT("AttemptCount"), static_cast<float>(AttemptCount));
	R.Details.Add(TEXT("SuccessCount"), static_cast<float>(SuccessCount));
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

		// 가중치 합으로 나눠 0~100 을 지킨다 — 에디터에서 가중치를 만져도 총점이 100 을 넘지 않는다.
		R.TotalScore = 100.0f * (
			Config.WeightAccuracy * R.Accuracy +
			Config.WeightEfficiency * R.Efficiency +
			Config.WeightConsistency * R.Consistency) / Config.WeightSum();
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

FOverallScore UScoringService::ComputeOverall(const TArray<FSessionResult>& History,
	const TArray<FOverallCategoryDef>& Categories, const FOverallScoreConfig& Config)
{
	FOverallScore Out;
	Out.CategoryCount = Categories.Num();

	float PlayedMaxPoints = 0.0f; // 실시한 종목들의 만점 합 — 100 환산의 분모.
	bool  bAnyUncalibrated = false;

	for (const FOverallCategoryDef& Def : Categories)
	{
		FOverallCategoryScore Cat;
		Cat.DisplayName = Def.DisplayName;
		Cat.ShortNameEn = Def.ShortNameEn;
		Cat.MaxPoints   = Def.MaxPoints;
		Cat.bIsOffense  = Def.bIsOffense;

		// 이 종목의 세션 중 (난이도 계수 적용 후) 최고점을 찾는다.
		for (const FSessionResult& S : History)
		{
			if (S.Mode != Def.Mode)
			{
				continue;
			}
			// DrillId 가 지정된 칸(수비 세부 종목)은 정확히 일치해야 한다.
			// NAME_None 인 칸(타격)은 종목 구분이 없으므로 모드만 본다.
			if (!Def.DrillId.IsNone() && S.DrillId != Def.DrillId)
			{
				continue;
			}
			if (!S.Average.bValid)
			{
				continue; // 시도 부족·헛스윙 등으로 점수가 성립하지 않은 세션.
			}

			const EDifficultyLevel Level = static_cast<EDifficultyLevel>(
				FMath::Clamp(S.DifficultyLevel, 0, static_cast<int32>(EDifficultyLevel::Pro)));

			const float Raw = S.Average.TotalScore;
			// 계수를 곱한 뒤 상한으로 clamp — Beginner 는 상한에 못 닿고, Pro 는 더 쉽게 닿는다.
			const float Adjusted = FMath::Clamp(Raw * Config.MultiplierFor(Level), 0.0f, Config.MaxCategoryScore);

			if (!Cat.bPlayed || Adjusted > Cat.BestScore)
			{
				Cat.bPlayed        = true;
				Cat.BestScore      = Adjusted;
				Cat.RawBestScore   = Raw;
				Cat.BestDifficulty = Level;
			}

			bAnyUncalibrated |= S.Average.bUncalibrated;
		}

		if (Cat.bPlayed)
		{
			const float Denom = FMath::Max(Config.MaxCategoryScore, KINDA_SMALL_NUMBER);
			Cat.EarnedPoints = (Cat.BestScore / Denom) * Cat.MaxPoints;

			PlayedMaxPoints += Cat.MaxPoints;
			++Out.PlayedCount;
			Out.RawTotal += Cat.EarnedPoints;
		}

		if (Def.bIsOffense)
		{
			Out.OffenseMaxPoints += Cat.MaxPoints;
			Out.OffensePoints    += Cat.EarnedPoints;
		}
		else
		{
			Out.DefenseMaxPoints += Cat.MaxPoints;
			Out.DefensePoints    += Cat.EarnedPoints;
		}

		Out.Categories.Add(Cat);
	}

	Out.bValid = (Out.PlayedCount > 0);
	Out.bUncalibrated = bAnyUncalibrated;

	// 미실시 종목은 분모에서도 빠진다 — 한 종목만 한 사람도 그 종목 기준의 정당한 점수를 받는다.
	// (전 종목을 하면 PlayedMaxPoints 가 100 이 되어 Total == RawTotal.)
	Out.Total = (PlayedMaxPoints > KINDA_SMALL_NUMBER)
		? (Out.RawTotal / PlayedMaxPoints) * 100.0f
		: 0.0f;

	return Out;
}
