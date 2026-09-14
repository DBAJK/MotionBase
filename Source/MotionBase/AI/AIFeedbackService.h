#pragma once

#include "CoreMinimal.h"
#include "Data/TrainingFeedback.h"
#include "AIFeedbackService.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFeedbackReady, bool, bSuccess, const FString&, FeedbackText);

/**
 * 개별 플레이 해설 완료 통지.
 * @param CacheKey 어느 (플레이 × 포지션) 에 대한 응답인지. **반드시 확인해야 한다** —
 *                 응답이 늦게 오면 이미 다음 시행으로 넘어간 뒤일 수 있고, 그때 그대로
 *                 표시하면 **틀린 상황의 해설이 붙는다**(잘못된 야구를 가르치는 셈).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayExplanationReady,
	bool, bSuccess, const FString&, CacheKey, const FString&, Explanation);

/**
 * 백업 플레이 하나에 대한 해설 요청 입력.
 * 판정은 이미 끝났고(결정론), 여기 담기는 건 **그 결정의 근거**뿐이다.
 */
USTRUCT(BlueprintType)
struct FBackupExplainRequest
{
	GENERATED_BODY()

	/** "<PlayId>|<Position>" — 같은 조합은 항상 같은 해설이라 캐시 키가 된다. */
	UPROPERTY(BlueprintReadWrite, Category = "AI")
	FString CacheKey;

	UPROPERTY(BlueprintReadWrite, Category = "AI")
	FString PositionName;

	UPROPERTY(BlueprintReadWrite, Category = "AI")
	FString Situation;

	UPROPERTY(BlueprintReadWrite, Category = "AI")
	FString RunnerText;

	/** 이 플레이에서 내가 맡은 역할 (백업/커버/중계/제자리). */
	UPROPERTY(BlueprintReadWrite, Category = "AI")
	FString JobText;

	/**
	 * 규칙 테이블이 들고 있는 한 줄 정답 근거. **이게 정본이다** — LLM 은 이걸 풀어 쓸 뿐
	 * 뒤집지 못하게 프롬프트로 막는다. 호출이 실패하면 이 문자열이 그대로 화면에 남는다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AI")
	FString AuthoredExplain;
};

/**
 * 생성형 AI 코칭 계층 (표현 전용).
 *
 * ⚠️ 이 서비스는 **약점을 판별하지 않는다.** 판별은 UWeaknessDetector 가 결정론적으로
 *    끝냈고, 추천 드릴도 UDrillCatalog 가 골랐다. 이 서비스는 그 리포트+드릴을 받아
 *    한국어 코칭 문장으로 표현만 한다. 덕분에:
 *      - LLM 이 없거나 호출이 실패해도 약점 리포트·드릴은 이미 화면에 있다 (계층 분리의 이득)
 *      - LLM 이 숫자를 지어낼 필요가 없다 (근거 숫자를 프롬프트에 그대로 넣는다)
 *
 * UE HTTP 모듈로 Anthropic Messages API 를 직접 async 호출 (별도 백엔드 없음, CLAUDE §3).
 * API 키는 Config/Secrets.ini (gitignore) 의 [AI] ApiKey. 키가 없으면 호출을 건너뛴다.
 */
UCLASS()
class MOTIONBASE_API UAIFeedbackService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 약점 리포트 + 추천 드릴 + (선택) 만성 추세 → AI 코칭 문장 async 요청.
	 * 완료 시 OnFeedbackReady(bSuccess, Text). 키 미설정·네트워크 실패 시 (false, 사유).
	 * @param Chronic 과거 이력 기반 만성 약점·추세. bValid=false 면 프롬프트에서 생략된다 —
	 *               이력이 쌓이면 코칭이 "이 약점이 반복되고 있다/나아지고 있다"까지 짚는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestSwingCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
		const FChronicWeaknessReport& Chronic);

	/**
	 * 포구(수비) 코칭 async 요청.
	 * 완료 시 같은 OnFeedbackReady 로 통지된다. 시스템 프롬프트가 '수비 코치'로 바뀐다.
	 * @param Chronic 타격과 동일하게 만성 추세를 받는다 — 각 수비 폰이 드릴 추천용으로
	 *               **이미 AnalyzeTrend 를 돌려 갖고 있는 값**이라 추가 비용이 없다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestCatchCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
		const FChronicWeaknessReport& Chronic);

	/**
	 * 송구(수비) 코칭 async 요청. 코치 역할이 '송구 코치'로 바뀐다 —
	 * 정확도·구속·전환 시간(transfer)은 포구와 처방이 다르므로 프롬프트를 분리한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestThrowCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
		const FChronicWeaknessReport& Chronic);

	/**
	 * 백업 위치 판단 코칭 async 요청.
	 * ⚠️ 다른 종목과 달리 **체력 훈련이 아니라 판단 훈련**이다 — 시스템 프롬프트가
	 *    "몸을 더 쓰라"가 아니라 "무엇을 보고 결정하라"를 말하도록 역할을 따로 준다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestBackupCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
		const FChronicWeaknessReport& Chronic);

	/**
	 * 백업 플레이 하나의 **해설** async 요청 (판정이 아니라 설명).
	 *
	 * ⚠️ **정답을 만드는 호출이 아니다.** 정답 존은 UBackupPlaybook 이 규칙으로 이미 정했고,
	 *    이 호출은 그 결정을 주자 상황·타구 방향과 엮어 풀어 쓰기만 한다. 프롬프트가
	 *    "주어진 배정은 최종이며 절대 뒤집지 말라"고 못박는다 — LLM 이 다른 베이스를
	 *    제안하면 **잘못된 야구를 가르치게 된다.**
	 *
	 * 완료 시 OnPlayExplanationReady(bSuccess, CacheKey, Text). 실패하면 호출부가
	 * 저작 해설(FBackupZone::Explain)로 되돌아가면 된다 — 화면이 비는 일은 없다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestPlayExplanation(const FBackupExplainRequest& Req);

	/** API 키가 설정돼 있는지 (호출 전 UI 에서 확인용). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|AI")
	bool IsConfigured() const;

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|AI")
	FOnFeedbackReady OnFeedbackReady;

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|AI")
	FOnPlayExplanationReady OnPlayExplanationReady;

	/**
	 * 사용할 모델. 기본은 최신 Claude.
	 * 💡 전시 부스처럼 반복 호출되고 작업이 단순(문장 표현)하면 비용상 haiku 가 유리하다:
	 *    "claude-haiku-4-5" 로 바꾸면 됨. 품질이 부족하면 상위 모델로.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|AI")
	FString ModelId = TEXT("claude-opus-5");

	/** 응답 최대 토큰. 코칭은 3~4문장이라 짧게. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|AI")
	int32 MaxTokens = 512;

	/**
	 * 플레이 해설용 모델 — 세션 코칭과 **일부러 분리**했다.
	 *
	 * 해설은 세션당 최대 시행 수만큼(기본 6회) 호출되는 반면 세션 코칭은 1회다. 부스에서
	 * 하루 종일 반복 시연하면 이 차이가 그대로 비용이 된다. 작업도 "한 줄을 두 문장으로
	 * 풀어쓰기"라 상위 모델이 필요 없다. → 고빈도·단순 작업은 haiku, 세션 요약은 opus.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|AI")
	FString ExplanationModelId = TEXT("claude-haiku-4-5-20251001");

	/** 해설 응답 최대 토큰. 1~2문장이라 아주 짧게 — 길면 판정 화면을 덮는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|AI")
	int32 ExplanationMaxTokens = 200;

	/**
	 * 출력 언어. 모든 시스템 프롬프트가 이 값을 "write ... in %s" 자리에 꽂아 쓴다.
	 * 기본은 영어 — VR 3D 패널에 한글 폰트(Content/Fonts/KRFont)가 아직 없어서 한국어로
	 * 바꾸면 글자가 깨진다. 폰트가 준비되면 여기만 "Korean"으로 바꾸면 된다(코드 수정 불필요).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|AI")
	FString OutputLanguage = TEXT("English");

private:
	/**
	 * 코치 도메인 — 시스템 프롬프트의 역할을 가른다.
	 * 수비를 한 덩어리로 묶지 않는 이유: 포구는 체력(반응·유연성), 송구는 역학(구속·전환),
	 * 백업은 판단이라 처방이 서로 다르다. 역할을 섞으면 "더 빠르게 반응하세요" 같은
	 * 엉뚱한 조언이 백업 판단 결과에 붙는다.
	 */
	enum class ECoachDomain : uint8 { Batting, Fielding, Throwing, Backup };

	/** Config/Secrets.ini 의 [AI] ApiKey 를 읽는다 (없으면 빈 문자열). */
	FString LoadApiKey() const;

	/** 코치 역할·규칙을 정하는 시스템 프롬프트 (도메인별). */
	FString BuildSystemPrompt(ECoachDomain Domain) const;

	/** 리포트+드릴+추세를 근거로 한 사용자 프롬프트 (숫자 포함). */
	FString BuildUserPrompt(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
		const FChronicWeaknessReport& Chronic) const;

	/** 요청 JSON 본문 직렬화. */
	FString BuildRequestBody(ECoachDomain Domain, const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills,
		const FChronicWeaknessReport& Chronic) const;

	/** 세션 코칭 본문을 보내고 OnFeedbackReady 로 통지. */
	void DispatchCoachingRequest(const FString& Body);

	/** 플레이 해설 요청 JSON 본문 (시스템 프롬프트가 코칭과 다르다 — 설명 전용 역할). */
	FString BuildExplanationBody(const FBackupExplainRequest& Req) const;

	/**
	 * 공통 HTTP 경로. 응답(성공/실패 + 텍스트)을 OnComplete 로 넘긴다.
	 * 완료 콜백은 서비스가 살아 있을 때만 불리므로 첫 인자는 항상 유효하다.
	 *
	 * @param bIsRetry 재시도 호출인지 — 429/5xx(일시적 오류)는 짧은 대기 후 **한 번만** 자동
	 *        재시도한다. 4xx(요청 자체가 잘못됨)는 재시도해도 같은 응답이 나오므로 즉시 실패
	 *        사유를 돌려준다. 부스 시연 중 순간적인 API 과부하/타임아웃에 화면이 바로
	 *        "미설정" 처럼 보이지 않게 하기 위함.
	 */
	void DispatchRequest(const FString& Body, const FString& Model,
		TFunction<void(UAIFeedbackService*, bool, const FString&)> OnComplete, bool bIsRetry = false);
};
