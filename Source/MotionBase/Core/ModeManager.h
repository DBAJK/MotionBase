#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/MotionBaseTypes.h"
#include "Data/ScoreResult.h"
#include "ModeManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModeChanged, EGameModeId, NewMode);

/**
 * 6개 게임 모드 전환/상태 관리. (Game Logic Layer)
 *
 * GameInstanceSubsystem 으로 두어 레벨 전환에도 세션 상태를 유지.
 * 모드별 채점 결과를 누적하고 Play Result Logger / SaveGame 으로 흘려보낸다.
 *
 * 시작 화면(AModeSelectPawn)이 여기서 모드 목록·표시 이름·구현 여부를 읽고,
 * AMotionBaseGameMode 가 선택 결과에 따라 폰을 교체한다.
 */
UCLASS()
class MOTIONBASE_API UModeManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 모드 진입 = 새 세션 시작.
	 * 같은 모드를 다시 골라도 누적 결과는 항상 초기화한다 — 이전 판의 시도가
	 * 다음 판 평균/일관성에 섞이면 점수가 조용히 오염된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void SetActiveMode(EGameModeId NewMode);

	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	EGameModeId GetActiveMode() const { return ActiveMode; }

	/** 한 판(모드 세션)의 결과를 기록. ModeId 가 비어 있으면 현재 모드 이름으로 채운다. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void RecordResult(const FScoreResult& Result);

	/** 참조 반환이라 UFUNCTION 으로 노출하지 않는다 (C++ 전용 접근자). */
	const TArray<FScoreResult>& GetSessionResults() const { return SessionResults; }

	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void ClearSessionResults();

	// ── 시작 화면(모드 선택)용 메타데이터 ──

	/** 메뉴에 표시할 모드 목록 (열거형 선언 순서 = 야구 흐름 순서). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static TArray<EGameModeId> GetMenuModes();

	/**
	 * 화면 표시 이름.
	 * ⚠️ UMETA(DisplayName) 은 에디터 전용 메타데이터라 패키징 빌드에서 사라진다.
	 *    전시용 빌드에서도 한글 이름이 나와야 하므로 여기서 직접 반환한다.
	 */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FText GetModeDisplayName(EGameModeId Mode);

	/**
	 * 저장·집계용 안정 식별자 (ASCII).
	 * 표시 이름은 번역·문구 수정으로 바뀔 수 있으므로 기록에는 이쪽을 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FName GetModeIdName(EGameModeId Mode);

	/** 메뉴 하단 한 줄 설명. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FText GetModeDescription(EGameModeId Mode);

	/** 지금 실제로 플레이 가능한 모드인지. ROADMAP 기준 현재는 타격만 true. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static bool IsModeImplemented(EGameModeId Mode);

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|Mode")
	FOnModeChanged OnModeChanged;

private:
	UPROPERTY()
	EGameModeId ActiveMode = EGameModeId::Batting;

	/** 세션 내 누적 결과 (저장/피드백 입력). */
	UPROPERTY()
	TArray<FScoreResult> SessionResults;
};
