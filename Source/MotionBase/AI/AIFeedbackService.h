#pragma once

#include "CoreMinimal.h"
#include "Save/MotionBaseSaveGame.h"
#include "AIFeedbackService.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFeedbackReady, bool, bSuccess, const FString&, FeedbackText);

/**
 * 생성형 AI 피드백 모듈 (확장 단계 — MVP 아님).
 *
 * 누적 플레이 데이터 → 프롬프트 구성 → UE HTTP 모듈로 API 직접 호출(async) → 텍스트 표시.
 * 별도 백엔드 없이 UE 내부에서 처리 (CLAUDE §3). API 키는 Config/Secrets.ini (gitignore).
 */
UCLASS()
class MOTIONBASE_API UAIFeedbackService : public UObject
{
	GENERATED_BODY()

public:
	/** 누적 기록으로 맞춤 피드백을 비동기 요청. 완료 시 OnFeedbackReady 발행. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|AI")
	void RequestFeedback(const TArray<FPlayRecord>& History);

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|AI")
	FOnFeedbackReady OnFeedbackReady;

private:
	/** 기록 → 프롬프트 문자열. */
	FString BuildPrompt(const TArray<FPlayRecord>& History) const;
};
