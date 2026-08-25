#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/MotionBaseTypes.h"
#include "Data/TrainingFeedback.h"
#include "UI/VRExitGesture.h"
#include "AICoachingPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class UMotionControllerComponent;
class UVRInfoPanel;
class UAIFeedbackService;

/**
 * AI 운동 추천 폰 — 독립 메뉴 항목("AI 코칭").
 *
 * 게임 세션이 아니라 **기록 리뷰 화면**이다. 그동안 저장된 세션 이력을 읽어
 *   ① 가장 최근의 유효한 약점 리포트를 focus 로 잡고
 *   ② UWeaknessDetector::AnalyzeTrend 로 만성 추세를 계산한 뒤
 *   ③ UDrillCatalog::RecommendWithHistory 로 추천 운동을 뽑고
 *   ④ (키가 있으면) UAIFeedbackService 로 코칭 문장을 얹는다
 * 결과를 헤드셋 안 3D 패널(UVRInfoPanel)에 띄운다.
 *
 * 기록이 없으면 "먼저 플레이하세요" 안내만 보여준다. 아무 것도 저장하지 않는다(읽기 전용).
 * 나가기: 컨트롤러를 위로 들기(제스처) 또는 '뒤로' 카드를 겨눠 유지(드웰).
 */
UCLASS()
class MOTIONBASE_API AAICoachingPawn : public APawn
{
	GENERATED_BODY()

public:
	AAICoachingPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "AICoaching")
	TObjectPtr<USceneComponent> VROrigin;

	UPROPERTY(VisibleAnywhere, Category = "AICoaching")
	TObjectPtr<UCameraComponent> Camera;

	/** 겨눔 포인터(오른손) — '뒤로' 카드 드웰·나가기 제스처 판정. */
	UPROPERTY(VisibleAnywhere, Category = "AICoaching")
	TObjectPtr<UMotionControllerComponent> PointerController;

	UPROPERTY(VisibleAnywhere, Category = "AICoaching")
	TObjectPtr<UVRInfoPanel> VrPanel;

	/** 카드를 이 시간(초) 겨누면 나가기 확정 (드웰). */
	UPROPERTY(EditAnywhere, Category = "AICoaching")
	float DwellTimeSec = 1.5f;

	/** 이 각도(도) 안이면 카드에 호버. */
	UPROPERTY(EditAnywhere, Category = "AICoaching")
	float DwellAngleDeg = 8.0f;

	void ReturnToModeSelect();

	/** OnFeedbackReady 수신 — 코칭 문장(성공) 또는 사유(실패)를 패널에 반영. */
	UFUNCTION()
	void HandleCoachingReady(bool bSuccess, const FString& Text);

private:
	/** 저장 이력을 읽어 focus 리포트·만성 추세·추천 드릴을 만들고, 키가 있으면 코칭을 요청한다. */
	void BuildRecommendation();

	/** 패널 갱신 (제목·약점·드릴·코칭). */
	void RefreshPanel();

	UPROPERTY(Transient)
	TObjectPtr<UAIFeedbackService> FeedbackService;

	FWeaknessReport         FocusReport;
	FChronicWeaknessReport  FocusChronic;
	TArray<FTrainingDrill>  Drills;
	FString                 CoachingText;

	EGameModeId FocusMode = EGameModeId::Batting;
	FName       FocusDrill = NAME_None;

	bool bHasData        = false;   // 분석에 쓸 기록이 있었는지
	bool bAwaitingCoaching = false; // AI 응답 대기 중

	/** VR '컨트롤러 위로 들어 나가기' 제스처. */
	FVRExitGesture ExitGesture;
};
