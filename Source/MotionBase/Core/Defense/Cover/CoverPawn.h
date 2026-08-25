#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/Defense/Cover/CoverTypes.h"
#include "Data/TrainingFeedback.h"
#include "UI/VRExitGesture.h"
#include "CoverPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UMotionControllerComponent;
class UTextRenderComponent;
class USceneComponent;
class UVRInfoPanel;
class UAIFeedbackService;

/**
 * 백업 위치 판단 훈련 폰 — **선택형 판단 퀴즈**.
 *
 * 흐름: 랜덤 경기 상황(타구/송구) + 당신의 수비 위치 제시 → 어디를 백업/커버할지
 *       4지선다로 선택 → 정답/오답 + 해설 → N문제 반복 → "N / 10" 집계.
 *
 * 이동이 아니라 **판단** 훈련이다 (요청 반영).
 *   - VR(HMD): 오른손 컨트롤러로 보기 카드를 겨누고 잠시 유지(드웰)하면 선택.
 *   - PC: 숫자키 1~4 로 선택.
 */
UCLASS()
class MOTIONBASE_API ACoverPawn : public APawn
{
	GENERATED_BODY()

public:
	ACoverPawn();

	virtual void Tick(float DeltaSeconds) override;

	// ── HUD(평면) 가 읽는 상태 접근자 ──
	int32 GetTotalTrials() const { return TotalTrials; }
	int32 GetSuccessCount() const { return SuccessCount; }
	int32 GetTrialNumber() const { return FMath::Min(TrialIndex + 1, TotalTrials); }

	FString GetSituationText() const { return CurrentQuiz.Situation; }
	/** 주자 상황 — 타구 방향과 함께 백업 판단을 가르는 두 번째 변수. */
	FString GetRunnersText() const { return CurrentQuiz.Runners; }
	FString GetRoleText() const { return CurrentQuiz.Role; }
	int32   GetOptionCount() const { return CurrentQuiz.Options.Num(); }
	FString GetOptionText(int32 Index) const;
	FString GetExplainText() const { return CurrentQuiz.Explain; }

	/** 현재 커서(강조) 인덱스. */
	int32 GetSelectedIndex() const { return SelectedIndex; }
	/** 답을 낸 뒤엔 정답 인덱스를 알려준다(초록 표시용). 아니면 -1. */
	int32 GetRevealCorrectIndex() const { return bAnswered ? CurrentQuiz.Correct : -1; }
	bool  IsAnswered() const { return bAnswered; }

	/** 마지막 판정 결과 문구/색. 표시할 게 있으면 true. */
	bool GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const;

	// ── 측정 지표 (스펙: 백업 정답률 + 판단까지 걸린 시간) ──
	/** 이번 문제를 본 뒤 지금까지 흐른 시간 (초). 답을 냈으면 -1. */
	float GetLiveDecisionSec() const;

	/** 세션 평균 판단 시간 (초). 표본 없으면 -1. */
	float GetAverageDecisionSec() const;

	/** 직전 문제의 판단 시간 (초). 없으면 -1. */
	float GetLastDecisionSec() const { return bAnswered ? LastResult.DecisionTimeSec : -1.0f; }

	/** 세션 종료 후 AI 운동(판단) 추천 문구. */
	const FString& GetCoachingText() const { return CoachingText; }

	/** 세션 종료 후 추천된 드릴 목록. */
	const TArray<FTrainingDrill>& GetRecommendedDrills() const { return LastDrills; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "Cover")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Cover")
	TObjectPtr<UCameraComponent> Camera;

	// ── VR 인메뉴(선택 카드) ──
	UPROPERTY(VisibleAnywhere, Category = "Cover|VR")
	TObjectPtr<UMotionControllerComponent> PointerController;

	/** VR 3D 패널 (상황=제목 · 보기 4개=행 · 결과/해설=푸터/힌트). 월드 고정. */
	UPROPERTY(VisibleAnywhere, Category = "Cover|VR")
	TObjectPtr<UVRInfoPanel> VrPanel;

	// ── 설정값 ──
	UPROPERTY(EditAnywhere, Category = "Cover")
	int32 TotalTrials = 10;

	UPROPERTY(EditAnywhere, Category = "Cover")
	float IntervalBetweenTrials = 2.0f;

	/**
	 * 이 시간(초) 안에 결정하면 판단 속도 만점.
	 * 실전에서 백업은 타구가 뜨는 순간 움직여야 하므로 짧게 잡는다.
	 * ⚠️ 실측 캘리브레이션 대상 — 하드코딩 확정 금지 (CLAUDE §규칙).
	 */
	UPROPERTY(EditAnywhere, Category = "Cover|Scoring", meta = (ClampMin = "0.5"))
	float TargetDecisionSec = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Cover|VR")
	float DwellTimeSec = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Cover|VR")
	float DwellAngleDeg = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Cover|VR")
	float MenuDistanceCm = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Cover|VR")
	float MenuHeightCm = 150.0f;

private:
	static constexpr int32 MaxOptions = 4;

	// ── 입력 ──
	void SelectPrev();   // VR/키보드 커서 이동
	void SelectNext();
	void ConfirmSelection();
	void PickOption0();  // 숫자키 1~4
	void PickOption1();
	void PickOption2();
	void PickOption3();
	void ReturnToModeSelect();

	// ── 세션 ──
	void StartSession();
	void SpawnNextTrial();
	void Answer(int32 OptionIndex);
	void EndSession();

	void BuildQuizPool();

	/** 남은 문제 주머니에서 하나 꺼낸다 (같은 문제가 연달아 나오지 않게). */
	FBackupQuiz DrawQuiz();

	// ── AI 판단 코칭 ──
	/** 세션 결과 → 백업 판단 약점 리포트 (정답률 + 판단 속도, 결정론적). */
	FWeaknessReport BuildBackupReport() const;

	/** 리포트 → 드릴 추천 + AI 코칭 요청 (세션 종료 시 1회). */
	void RequestBackupFeedback();

	/** 진행 중인 세션을 저장 슬롯에 확정한다 (모드 복귀·앱 종료 시). */
	void FlushSessionToSave();

	UFUNCTION()
	void HandleCoachingReady(bool bSuccess, const FString& Text);

	// ── VR ──
	void InitVRMenu();
	void UpdateVRMenu(float DeltaSeconds);
	void RefreshVRTexts();
	int32 PickHoveredCard() const;

	// ── 상태 ──
	TArray<FBackupQuiz> QuizPool;
	/** 아직 안 낸 문제의 인덱스 주머니. 비면 다시 채운다 — 같은 문제 반복을 줄인다. */
	TArray<int32> RemainingQuiz;
	FBackupQuiz CurrentQuiz;

	/** 이번 문제가 화면에 뜬 시각 (월드 시간). 판단 시간 측정의 기준점. */
	float QuizShownTimeSec = -1.0f;

	/** 이번 세션의 문항별 결과 (정답률·판단 시간 집계 입력). */
	TArray<FCoverResult> SessionResults;

	// ── AI 판단 코칭 상태 ──
	UPROPERTY(Transient)
	TObjectPtr<UAIFeedbackService> FeedbackService;

	TArray<FTrainingDrill> LastDrills;
	FString CoachingText;
	bool bAwaitingCoaching = false;

	int32 TrialIndex = 0;
	int32 SuccessCount = 0;
	int32 SelectedIndex = 0;     // 커서
	int32 ChosenIndex = -1;      // 이번에 고른 답
	bool  bAnswered = false;     // 답 제출됨(결과 표시 중)
	bool  bSessionOver = false;

	bool  bWaitingNext = false;
	float IntervalTimer = 0.0f;

	FCoverResult LastResult;

	// VR
	bool  bVR = false;
	int32 VrHoverIndex = INDEX_NONE;
	float VrDwellTimer = 0.0f;
	float VrCooldown = 0.0f;

	/** VR '컨트롤러 위로 들어 나가기' 제스처 상태 (헤드셋만으로 모드 선택 복귀). */
	FVRExitGesture ExitGesture;
};
