#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/Defense/Cover/CoverTypes.h"
#include "UI/VRExitGesture.h"
#include "CoverPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UMotionControllerComponent;
class UTextRenderComponent;
class USceneComponent;
class UVRInfoPanel;

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

protected:
	virtual void BeginPlay() override;
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

	UPROPERTY(EditAnywhere, Category = "Cover|VR")
	float DwellTimeSec = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Cover|VR")
	float DwellAngleDeg = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Cover|VR")
	float MenuDistanceCm = 250.0f;

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

	// ── VR ──
	void InitVRMenu();
	void UpdateVRMenu(float DeltaSeconds);
	void RefreshVRTexts();
	int32 PickHoveredCard() const;

	// ── 상태 ──
	TArray<FBackupQuiz> QuizPool;
	FBackupQuiz CurrentQuiz;

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
