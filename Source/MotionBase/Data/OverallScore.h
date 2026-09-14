#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"
#include "OverallScore.generated.h"

/**
 * 종합 점수 한 칸(종목)의 정의 — "이 종목은 무엇이고 만점일 때 몇 점을 차지하는가".
 *
 * ⚠️ 종목 식별자(DrillId)를 여기서 **문자열로 새로 적지 않는다.** 저장에 남는 정본은
 *    UModeManager 가 쥐고 있고, 실제로 "Backup" → "BackupMove" 로 바뀌었을 때 리터럴을
 *    복사해 둔 곳이 함께 안 고쳐져 조용히 오동작한 적이 있다. 목록은 항상
 *    UModeManager::BuildOverallCategories() 로 만든다.
 */
USTRUCT(BlueprintType)
struct FOverallCategoryDef
{
	GENERATED_BODY()

	/** 이 칸이 집계할 모드. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Overall")
	EGameModeId Mode = EGameModeId::Batting;

	/** 수비 세부 종목. NAME_None 이면 모드만으로 매칭한다(타격). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Overall")
	FName DrillId = NAME_None;

	/**
	 * 화면 표시용 짧은 이름 (한글). 평면 HUD 와 VR 패널이 공용으로 쓴다.
	 * ⚠️ VR 패널 한 행이 ~30자라 4종목이 한 줄에 들어가려면 짧아야 한다 — "포구"/"송구" 처럼.
	 * (Content/Fonts/KRFont 도입 전엔 VR 패널용 영문 약칭을 따로 뒀었다 — 이젠 필요 없다.)
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Overall")
	FString DisplayName;

	/** 이 종목이 만점일 때 종합에서 차지하는 점수 (공격 50, 수비 3종목 각 50/3). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Overall")
	float MaxPoints = 0.0f;

	/** 공격 칸인지 (화면에서 공격 50 / 수비 50 소계를 나누는 기준). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Overall")
	bool bIsOffense = false;
};

/** 종합 점수 한 칸의 계산 결과. */
USTRUCT(BlueprintType)
struct FOverallCategoryScore
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	FString DisplayName;

	/** 한 번이라도 유효 세션을 남겼는가. false 면 아래 값들은 의미가 없다. */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	bool bPlayed = false;

	/** 난이도 계수 적용 후의 최고 점수 (0~100). 미실시면 -1. */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float BestScore = -1.0f;

	/** 계수 적용 **전**의 원점수 (0~100). 화면에서 "왜 올랐는지"를 보여줄 때 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float RawBestScore = -1.0f;

	/** 최고 기록이 나온 난이도. */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	EDifficultyLevel BestDifficulty = EDifficultyLevel::Amateur;

	/**
	 * 가장 최근 유효 세션의 점수 (난이도 계수 적용 후, 0~100). 없으면 -1.
	 * 최고점만 보여주면 새로 플레이해도 숫자가 안 바뀌어 "결과가 반영 안 된다"로 읽힌다 —
	 * 방금 한 판이 어땠는지를 최고점 옆에 같이 보여주기 위한 값.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float LatestScore = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	EDifficultyLevel LatestDifficulty = EDifficultyLevel::Amateur;

	/** 최고점 후보로 본 세션 수 (최근 창 안의 유효 세션). */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	int32 SessionsConsidered = 0;

	/** 이 칸에서 실제로 딴 점수 (0 ~ MaxPoints). */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float EarnedPoints = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float MaxPoints = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	bool bIsOffense = false;
};

/**
 * 공격 50 + 수비 50 = 100점 종합 결과.
 *
 * **미실시 종목은 0점이 아니라 계산에서 빠진다.** 부스 관람객 대부분은 한두 종목만 하는데,
 * 안 한 걸 0으로 깔면 타격을 완벽하게 한 사람도 50/100 이 되어 "절반짜리"로 보인다.
 * 대신 완료도(PlayedCount/CategoryCount)를 함께 실어, 총점이 다음 종목을 하게 만드는
 * 장치가 되게 한다. 절대 점수가 필요하면 RawTotal 을 쓴다.
 */
USTRUCT(BlueprintType)
struct FOverallScore
{
	GENERATED_BODY()

	/** 유효 세션이 하나라도 있는가. false 면 화면에 "기록 없음"을 띄운다. */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	bool bValid = false;

	/** 실시한 종목 기준 100점 환산 점수. 화면의 대표 숫자. */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float Total = 0.0f;

	/** 미실시를 0 으로 본 절대 점수 (= EarnedPoints 합). 전 종목 완주 시 Total 과 같아진다. */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float RawTotal = 0.0f;

	/** 공격 소계 (0~50). */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float OffensePoints = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float OffenseMaxPoints = 0.0f;

	/** 수비 소계 (0~50). */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float DefensePoints = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	float DefenseMaxPoints = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	int32 PlayedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	int32 CategoryCount = 0;

	/**
	 * 집계에 들어간 점수 중 하나라도 미보정 상수로 산출됐는가.
	 * ⚠️ **반드시 화면까지 전파한다.** 100점 만점 총점은 정밀해 보이는데 기준 상수는
	 *    아직 실측 보정 전이다(CLAUDE.md 규칙). 표시가 없으면 예시 상수로 뽑은 숫자가
	 *    확정 점수처럼 제출된다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	bool bUncalibrated = true;

	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	TArray<FOverallCategoryScore> Categories;

	/** 가장 최근에 유효 세션이 저장된 종목의 Categories 인덱스. 없으면 INDEX_NONE. ("방금 반영된 판" 표시용) */
	UPROPERTY(BlueprintReadOnly, Category = "Overall")
	int32 LatestCategoryIndex = INDEX_NONE;
};

/**
 * 종합 점수 계산 상수. **실측 캘리브레이션 대상 — 하드코딩 확정 금지** (CLAUDE.md 규칙).
 */
USTRUCT(BlueprintType)
struct FOverallScoreConfig
{
	GENERATED_BODY()

	/**
	 * 난이도 계수를 점수에 곱할지.
	 *
	 * ⚠️ 계수 자체가 실측 없이 정한 추정치라, 켜는 순간 총점의 근거가 그만큼 약해진다.
	 *    도전 유인을 위해 켜되, 실플레이 데이터가 쌓이면 아래 값들을 먼저 보정할 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overall")
	bool bApplyDifficultyMultiplier = true;

	/**
	 * 난이도별 계수. 곱한 뒤 100 으로 clamp 하므로 동작은 "상한이 아니라 도달 가능성"이다:
	 *   - Pro   ×1.2 → 83점만 내도 100 도달 가능 (어려운 난이도 도전에 대한 보상)
	 *   - Amateur ×1.0 → 100 을 내야 100
	 *   - Beginner ×0.8 → **아무리 잘해도 80 이 상한** (쉬운 난이도로 만점 방지)
	 * clamp 덕분에 총점이 100 을 넘는 일은 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overall")
	float BeginnerMultiplier = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overall")
	float AmateurMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overall")
	float ProMultiplier = 1.2f;

	/** 한 종목의 점수 상한. 계수를 곱한 뒤 여기로 clamp 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overall")
	float MaxCategoryScore = 100.0f;

	/**
	 * 종목별 최고점을 찾을 최근 유효 세션 수. 0 이면 역대 전체.
	 * ⚠️ 역대 최고점을 쓰면 옛 기록이 최고점에 박혀 새로 플레이해도 종합이 안 바뀐다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overall", meta = (ClampMin = "0"))
	int32 RecentSessionWindow = 5;

	/** 시도 수가 이보다 적은 세션은 종합에서 뺀다 (1~2회짜리 판의 100점이 최고점으로 박히는 것 방지). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overall", meta = (ClampMin = "1"))
	int32 MinAttemptsForOverall = 3;

	/** 수비의 옛 성공률 채점(ScoreDefenseSession) 세션을 뺄지 — 3축 채점과 기준이 달라 섞으면 왜곡된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overall")
	bool bExcludeLegacyDefenseScoring = true;

	float MultiplierFor(EDifficultyLevel Level) const
	{
		if (!bApplyDifficultyMultiplier)
		{
			return 1.0f;
		}
		switch (Level)
		{
		case EDifficultyLevel::Beginner: return BeginnerMultiplier;
		case EDifficultyLevel::Pro:      return ProMultiplier;
		case EDifficultyLevel::Amateur:
		default:                          return AmateurMultiplier;
		}
	}
};
