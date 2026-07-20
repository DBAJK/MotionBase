#include "AI/AIFeedbackService.h"
#include "MotionBase.h"

FString UAIFeedbackService::BuildPrompt(const TArray<FPlayRecord>& History) const
{
	// TODO: 최근 N판 지표 추이(정확도/효율/일관성)를 요약해 코칭 프롬프트로 구성.
	FString Summary;
	const int32 Count = FMath::Min(History.Num(), 10);
	for (int32 i = History.Num() - Count; i < History.Num(); ++i)
	{
		const FPlayRecord& R = History[i];
		Summary += FString::Printf(TEXT("- mode=%d total=%.0f acc=%.2f eff=%.2f con=%.2f\n"),
			static_cast<int32>(R.Mode), R.Score.TotalScore,
			R.Score.Accuracy, R.Score.Efficiency, R.Score.Consistency);
	}
	return FString::Printf(TEXT("최근 야구 훈련 기록입니다. 강점/약점과 다음 연습 포인트를 한국어로 조언해 주세요.\n%s"), *Summary);
}

void UAIFeedbackService::RequestFeedback(const TArray<FPlayRecord>& History)
{
	const FString Prompt = BuildPrompt(History);
	UE_LOG(LogMotionBase, Log, TEXT("AIFeedback: 프롬프트 구성 완료 (%d chars). HTTP 호출 미구현."), Prompt.Len());

	// TODO: FHttpModule::Get().CreateRequest() 로 생성형 AI API async 호출.
	//       응답 파싱 후 OnFeedbackReady.Broadcast(true, Text). 실패 시 (false, 오류메시지).
	OnFeedbackReady.Broadcast(false, TEXT("AI 피드백 미구현 (확장 단계)"));
}
