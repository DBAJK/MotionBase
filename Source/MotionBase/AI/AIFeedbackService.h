#pragma once

#include "CoreMinimal.h"
#include "Data/TrainingFeedback.h"
#include "AIFeedbackService.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFeedbackReady, bool, bSuccess, const FString&, FeedbackText);

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
	 * 포구(수비) 코칭 async 요청. 타격과 달리 만성 추세(이력 분석)는 아직 없으므로 리포트+드릴만 받는다.
	 * 완료 시 같은 OnFeedbackReady 로 통지된다. 시스템 프롬프트가 '수비 코치'로 바뀐다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestCatchCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills);

	/**
	 * 송구(수비) 코칭 async 요청. 코치 역할이 '송구 코치'로 바뀐다 —
	 * 정확도·구속·전환 시간(transfer)은 포구와 처방이 다르므로 프롬프트를 분리한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestThrowCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills);

	/**
	 * 백업 위치 판단 코칭 async 요청.
	 * ⚠️ 다른 종목과 달리 **체력 훈련이 아니라 판단 훈련**이다 — 시스템 프롬프트가
	 *    "몸을 더 쓰라"가 아니라 "무엇을 보고 결정하라"를 말하도록 역할을 따로 준다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestBackupCoaching(const FWeaknessReport& Report, const TArray<FTrainingDrill>& Drills);

	/** API 키가 설정돼 있는지 (호출 전 UI 에서 확인용). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|AI")
	bool IsConfigured() const;

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|AI")
	FOnFeedbackReady OnFeedbackReady;

	/**
	 * 사용할 모델. 기본은 최신 Claude.
	 * 💡 전시 부스처럼 반복 호출되고 작업이 단순(문장 표현)하면 비용상 haiku 가 유리하다:
	 *    "claude-haiku-4-5" 로 바꾸면 됨. 품질이 부족하면 상위 모델로.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|AI")
	FString ModelId = TEXT("claude-opus-5");

	/** 응답 최대 토큰. 코칭은 2~3문장이라 짧게. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|AI")
	int32 MaxTokens = 512;

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

	/** 본문을 Anthropic API 로 async 전송하고 OnFeedbackReady 로 통지 (공통 HTTP 경로). */
	void DispatchCoachingRequest(const FString& Body);
};
