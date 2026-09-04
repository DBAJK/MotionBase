#pragma once

#include "CoreMinimal.h"
#include "Data/MotionBaseTypes.h"
#include "TrainingFeedback.generated.h"

/**
 * 약점 판별 → 운동 추천 → (선택) AI 코칭 파이프라인의 계층 간 계약.
 *
 * 핵심 원칙: **약점을 찾는 것은 결정론적 계산(UWeaknessDetector)**, LLM은 그 위에
 * 코칭 문장만 얹는 표현 계층이다. 덕분에 네트워크·API 키 없이도 약점 리포트와
 * 추천 드릴이 나오고, 단위 테스트가 가능하며, "왜 이 조언이 나왔는가"에 숫자로 답할 수 있다.
 *
 * 계약은 **모드 무관**이다 — 타격은 스윙 지표로, 베이스 러닝은 전신 지표로 같은
 * FWeaknessReport 를 채운다. FScoreResult 가 모드 무관인 것과 같은 패턴.
 */

/** 약점 축. 점수 3축보다 세분화 — 코칭은 구체적일수록 좋다. */
UENUM(BlueprintType)
enum class EWeaknessAxis : uint8
{
	// ── 스윙 지표(Vive) 기반 ──
	ContactRate     UMETA(DisplayName = "컨택률"),        // 헛스윙이 잦음
	Timing          UMETA(DisplayName = "타이밍"),        // 컨택 순간이 도달 시각과 어긋남
	ContactAccuracy UMETA(DisplayName = "컨택 정확도"),   // 스위트스팟에서 벗어남
	BatSpeed        UMETA(DisplayName = "배트 스피드"),   // 임팩트 속도 부족
	Consistency     UMETA(DisplayName = "일관성"),        // 시도별 편차가 큼

	// ── 신체역학(카메라/MediaPipe) 기반 ──
	HipShoulderSeparation UMETA(DisplayName = "상하체 분리(X-factor)"), // 비틀림=파워 저장 부족
	HeadStability         UMETA(DisplayName = "머리 안정"),             // 스윙 중 머리 흔들림
	KineticChain          UMETA(DisplayName = "운동 사슬"),             // 힙→어깨→손 순서 흐트러짐
	WeightShift           UMETA(DisplayName = "체중 이동"),             // 뒷발→앞발 이동 부족

	// ── 수비(포구) 기반 ──
	CatchReaction   UMETA(DisplayName = "반응속도"),      // 타이밍 늦음·놓침 → 반응이 느림
	UpperBodyFlex   UMETA(DisplayName = "상체 유연성"),   // 글러브가 공에 못 닿음 → 뻗는 가동범위 부족
	FootSpeed       UMETA(DisplayName = "발 스피드"),     // 위치 선점 실패 (이동이 컨트롤러라 비중은 낮음)

	// ── 수비(송구) 기반 ── ⚠️ 값을 새로 끼워넣지 말 것 (저장된 세이브의 축이 밀린다). 항상 끝에 추가.
	ThrowAccuracy   UMETA(DisplayName = "송구 정확도"),   // 목표 zone 을 벗어남 (짧음/넘김/좌우)
	ArmStrength     UMETA(DisplayName = "송구 구속"),     // 릴리스 구속 부족 → 주자를 못 잡음
	TransferQuick   UMETA(DisplayName = "포구→송구 전환"), // 글러브에서 손으로 옮기는 시간이 김

	// ── 수비(백업 위치 판단) 기반 ── ⚠️ 값을 새로 끼워넣지 말 것 (저장된 세이브의 축이 밀린다). 항상 끝에 추가.
	BackupJudgment  UMETA(DisplayName = "백업 판단"),     // 정답 백업 zone 을 못 고름
	DecisionSpeed   UMETA(DisplayName = "판단 속도"),     // 답은 맞지만 결정(이동 개시)이 느림
	RouteEfficiency UMETA(DisplayName = "이동 경로 효율") // 정답은 맞지만 헤매며 이동함 (직선거리/실제이동거리)
	// ⚠️ FootSpeed 를 재사용하지 말 것 — 이 축은 다리가 아니라 컨트롤러로 움직인 경로의
	//    효율이다. FootSpeed 로 잡으면 DrillCatalog 가 "사다리 스텝" 같은 체력 드릴을
	//    내주는데, 백업 판단은 판단 훈련이라 컨디셔닝 처방과 정면 충돌한다
	//    (AIFeedbackService 의 Backup 시스템 프롬프트가 이걸 명시적으로 금지한다).
};

/** 한 축의 약점. */
USTRUCT(BlueprintType)
struct FWeakness
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	EWeaknessAxis Axis = EWeaknessAxis::Timing;

	/** 이 축 수행도 0~1 (높을수록 잘함). */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	float Score = 0.0f;

	/** 심각도 0~1 (= 1 - Score, 높을수록 나쁨). 정렬·우선순위 기준. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	float Severity = 0.0f;

	/** 근거 문자열 (실측 숫자 포함). 예: "평균 타이밍 오차 120 ms (목표 ±50 ms)". */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString Evidence;
};

/** 세션 1회의 약점 분석 결과. 결정론적으로 산출된다. */
USTRUCT(BlueprintType)
struct FWeaknessReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	EGameModeId Mode = EGameModeId::Batting;

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	int32 AttemptCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	int32 ContactCount = 0;

	/** 심각도 내림차순 정렬. 앞쪽이 가장 시급한 약점. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	TArray<FWeakness> Weaknesses;

	/**
	 * 약점 축으로는 표현되지 않지만 코칭에 필요한 부가 근거(실측 문자열).
	 * 예: "타구 타입별 성공률 GB 3/4, FB 1/3, LD 0/3", "베이스별 정확도 1B 3/3, Home 0/2".
	 *
	 * 왜 축이 아니라 노트인가: "뜬공만 못 잡는다"는 **처방(드릴)을 바꾸지 않는다** —
	 * 약점 축은 드릴 매핑의 키라서 늘리면 카탈로그가 같이 커진다. 반면 코칭 문장에는
	 * 반드시 들어가야 할 정보라서, 드릴 선택엔 영향 없이 프롬프트에만 실리는 자리를 둔다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	TArray<FString> Notes;

	/** 표본이 있어 분석이 유효한지. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	bool bValid = false;

	/** 점수 기준 상수가 미보정 예시값인지. LLM 프롬프트에 전파해 단정을 막는다. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	bool bUncalibrated = true;
};

/** 추천 운동(드릴) 한 개. 카탈로그(UDrillCatalog)에서 온다 — LLM 자유생성 아님. */
USTRUCT(BlueprintType)
struct FTrainingDrill
{
	GENERATED_BODY()

	/** 드릴 이름. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString Name;

	/** 어떤 약점을 겨냥하는지. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	EWeaknessAxis TargetAxis = EWeaknessAxis::Timing;

	/** 수행 방법 한 줄. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString Description;

	/** 핵심 포커스 큐 (짧게). */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString FocusCue;

	/**
	 * 이 운동이 무엇을 향상시키는지 — "데드리프트는 고관절과 코어를 키운다" 의 자리.
	 * 코칭 문장이 운동 이름만 나열하지 않고 **왜 하는지**를 말할 수 있게 하는 근거다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString Benefit;

	/**
	 * 수행량 처방 ("3 sets x 15 reps", "3 sets x 30 seconds", "4 sets x 10m").
	 *
	 * ⚠️ **LLM 이 지어내면 안 되는 값이다.** 물리 훈련 처방이라 부상 위험이 있어
	 *    카탈로그(사람이 검수)가 확정해 프롬프트로 내려보내고, LLM 은 그대로 인용만 한다.
	 * 정수 필드(Sets/Reps)로 쪼개지 않은 이유: 횟수·시간·거리로 단위가 제각각이라
	 * (30초 3세트 / 10m 4세트) 정수 두 개로는 절반이 표현되지 않는다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FString Prescription;
};

/**
 * 한 약점 축의 세션 간 변화 방향. 단일 세션 리포트로는 알 수 없고,
 * 저장 이력 여러 건을 UWeaknessDetector::AnalyzeTrend 로 훑어야 나온다.
 */
UENUM(BlueprintType)
enum class EWeaknessTrend : uint8
{
	Insufficient UMETA(DisplayName = "표본 부족"), // 창 안 유효 세션 < 2
	New          UMETA(DisplayName = "신규"),      // 최근에 처음 잡힌 약점
	Improving    UMETA(DisplayName = "개선 중"),   // 수행도가 오르는 추세
	Stable       UMETA(DisplayName = "정체"),      // 변화 미미
	Worsening    UMETA(DisplayName = "악화")        // 수행도가 내려가는 추세
};

/**
 * 한 축의 세션 간 추세 (저장 이력 기반). "이 약점이 만성인가, 나아지는가"에 답한다.
 * 추천 우선순위(만성일수록 위로)와 결과 화면 추세 표시의 입력.
 */
USTRUCT(BlueprintType)
struct FAxisTrend
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	EWeaknessAxis Axis = EWeaknessAxis::Timing;

	/** 분석 창 안에서 이 축이 약점으로 잡힌 세션 수. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	int32 AppearanceCount = 0;

	/** 실제 분석에 쓴 세션 수 (요청 창 크기 이하, 리포트 있는 세션만). */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	int32 WindowSize = 0;

	/** 창 안 평균 수행도 0~1 (약점 미등장 세션은 낙관 대체값으로 채움). */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	float AverageScore = 0.0f;

	/** 수행도 기울기(세션당). 양수 = 개선, 음수 = 악화. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	float ScoreSlope = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	EWeaknessTrend Trend = EWeaknessTrend::Insufficient;

	/** 창의 절반 이상에서 반복 등장한 만성 약점인지. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	bool bChronic = false;
};

/**
 * 저장 이력 전체를 훑은 만성 약점/추세 리포트 (계산 계층).
 * ⚠️ "아직 저장되지 않은 이번 세션"은 포함하지 않는다 — FModeStats 와 같은 규칙.
 *    그래서 이번 세션 리포트(단발)와 과거 추세(이력)를 분리해 비교할 수 있다.
 */
USTRUCT(BlueprintType)
struct FChronicWeaknessReport
{
	GENERATED_BODY()

	/** 만성도·심각도 순 정렬 (앞쪽이 가장 시급). */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	TArray<FAxisTrend> Trends;

	/** 실제 분석에 쓴 세션 수. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	int32 SessionsAnalyzed = 0;

	/** 분석할 이력이 충분했는지. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	bool bValid = false;
};
