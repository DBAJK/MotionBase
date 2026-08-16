#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/Defense/Cover/CoverTypes.h"
#include "CoverPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;

/**
 * 커버 훈련 폰 (1인칭).
 *
 * 흐름: 베이스 4개 배치 → 정답 베이스 랜덤 하이라이트 → 제한 시간 안에 WASD 로
 *       그 베이스로 이동 → 도착 판정(FCoverJudge) → 10회 반복 → "N / 10" 집계.
 *
 * 공/송구 없음 — 상황 판단 + 이동 훈련. 지금은 정답 베이스가 노랗게 빛난다.
 */
UCLASS()
class MOTIONBASE_API ACoverPawn : public APawn
{
	GENERATED_BODY()

public:
	ACoverPawn();

	virtual void Tick(float DeltaSeconds) override;

	// ── HUD 가 읽는 상태 접근자 ──
	int32 GetTotalTrials() const { return TotalTrials; }
	int32 GetSuccessCount() const { return SuccessCount; }
	int32 GetTrialNumber() const { return FMath::Min(TrialIndex + 1, TotalTrials); }

	/** 남은 시간 (초). */
	float GetTimeLeft() const { return TimeLeft; }

	/** 이번에 커버할 베이스 이름 (예: "2루"). */
	FString GetTargetBaseName() const;

	/** 마지막 판정 결과 문구/색. 표시할 게 있으면 true. */
	bool GetLastOutcomeText(FString& OutText, FLinearColor& OutColor) const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "Cover")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Cover")
	TObjectPtr<UCameraComponent> Camera;

	// ── 설정값 ──

	UPROPERTY(EditAnywhere, Category = "Cover")
	int32 TotalTrials = 10;

	/** 제한 시간 (초). */
	UPROPERTY(EditAnywhere, Category = "Cover")
	float TimeLimit = 3.0f;

	/** 성공 반경 (cm). */
	UPROPERTY(EditAnywhere, Category = "Cover")
	float CoverRadius = 150.0f;

	/** 홈에서 각 베이스까지 거리 (cm). 다이아몬드 한 변 기준. */
	UPROPERTY(EditAnywhere, Category = "Cover")
	float BaseDistance = 900.0f;

	/** 한 회 사이 간격 (초). */
	UPROPERTY(EditAnywhere, Category = "Cover")
	float IntervalBetweenTrials = 1.5f;

	/** 이동 속도 (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Cover")
	float MoveSpeed = 700.0f;

private:
	// ── 입력 (BindKey 눌림/뗌 → 플래그) ──
	void OnRightPressed()  { bMoveRight = true; }
	void OnRightReleased() { bMoveRight = false; }
	void OnLeftPressed()   { bMoveLeft = true; }
	void OnLeftReleased()  { bMoveLeft = false; }
	void OnFwdPressed()    { bMoveFwd = true; }
	void OnFwdReleased()   { bMoveFwd = false; }
	void OnBackPressed()   { bMoveBack = true; }
	void OnBackReleased()  { bMoveBack = false; }
	void ReturnToModeSelect();

	// ── 세션 ──
	void StartSession();
	void SpawnNextTrial();
	void FinishTrial(const FCoverResult& Result);
	void EndSession();

	/** 베이스 4개 위치를 홈 기준 다이아몬드로 계산. */
	void SetupBaseLocations();

	FVector GetBaseLocation(EBaseType Base) const;

	// ── 상태 ──
	FVector HomeLocation = FVector::ZeroVector;

	// 이동 입력 플래그 (BindKey 눌림 상태 유지용)
	bool bMoveRight = false;
	bool bMoveLeft  = false;
	bool bMoveFwd   = false;
	bool bMoveBack  = false;

	/** 베이스 4개 월드 위치 (First/Second/Third/Home 순). */
	FVector BaseLocations[4];

	FCoverTrial CurrentTrial;

	int32 TrialIndex = 0;
	int32 SuccessCount = 0;

	float TimeLeft = 0.0f;
	bool  bTrialActive = false;   // 카운트다운 중
	bool  bSessionOver = false;

	bool  bWaitingNext = false;
	float IntervalTimer = 0.0f;

	FCoverResult LastResult;
	bool bHasResult = false;
};