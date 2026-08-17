#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/MotionBaseTypes.h"
#include "Data/ScoreResult.h"
#include "Data/SessionResult.h"
#include "ModeManager.generated.h"

class UMotionBaseSaveGame;

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
	/** 저장 슬롯을 로드(없으면 생성)해 세션 상태를 준비. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 서브시스템 종료 시 진행 중이던 세션이 있으면 마지막으로 flush. */
	virtual void Deinitialize() override;

	/**
	 * 모드+난이도 진입 = 새 세션 시작.
	 * 같은 모드를 다시 골라도 누적 결과는 항상 초기화한다 — 이전 판의 시도가
	 * 다음 판 평균/일관성에 섞이면 점수가 조용히 오염된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void SetActiveMode(EGameModeId NewMode, EDifficultyLevel NewDifficulty = EDifficultyLevel::Amateur);

	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	EGameModeId GetActiveMode() const { return ActiveMode; }

	/** 현재 세션의 난이도. 모드 폰(ASwingTestPawn 등)이 읽어 파라미터에 반영한다. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	EDifficultyLevel GetActiveDifficulty() const { return ActiveDifficulty; }

	/** 한 판(모드 세션)의 결과를 기록. ModeId 가 비어 있으면 현재 모드 이름으로 채운다. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void RecordResult(const FScoreResult& Result);

	/** 참조 반환이라 UFUNCTION 으로 노출하지 않는다 (C++ 전용 접근자). */
	const TArray<FScoreResult>& GetSessionResults() const { return SessionResults; }

	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void ClearSessionResults();

	// ── 세션 영속화 (로컬 저장) ──

	/**
	 * 진행 중인 세션을 한 건의 FSessionResult 로 확정해 저장 슬롯에 적는다.
	 * 시도가 하나도 없으면(빈 세션) 저장하지 않고 조용히 무시한다.
	 * 저장 후 SessionResults 를 비운다 → 같은 세션이 두 번 기록되지 않는다.
	 * @param SessionAverage 세션 집계 점수 (호출자가 ScoreSession 으로 구한 값).
	 * @param Report         이 세션의 약점 리포트 (없으면 기본값 — 만성 약점 계산에서 무시됨).
	 * @return 실제로 저장했으면 true.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	bool FinalizeSession(const FScoreResult& SessionAverage, const FWeaknessReport& Report);

	/** 저장된 전체 세션 기록 (오래된→최신 순, append 순서). */
	const TArray<FSessionResult>& GetHistory() const;

	/**
	 * 특정 모드의 역대 최고 총점. 기록이 없으면 음수(-1)를 돌려준다 —
	 * "첫 기록"과 "0점"을 구분해야 UI 가 '신기록' 표시를 낼 수 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	float GetBestTotalScore(EGameModeId Mode) const;

	/**
	 * 특정 모드의 누적 기록 집계 (세션 수·최고·평균·직전·최근 추세).
	 * 결과 화면이 이번 판을 과거와 비교하는 데 쓴다. 저장된 세션만 반영한다.
	 * @param RecentCount 추세 그래프에 담을 최근 세션 개수.
	 */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	FModeStats GetModeStats(EGameModeId Mode, int32 RecentCount = 6) const;

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

	// ── 난이도 선택용 메타데이터 ──

	/** 난이도 목록 (쉬운→어려운 순). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static TArray<EDifficultyLevel> GetMenuDifficulties();

	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FText GetDifficultyDisplayName(EDifficultyLevel Level);

	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FText GetDifficultyDescription(EDifficultyLevel Level);

	/** 저장·집계용 안정 식별자 (ASCII). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FName GetDifficultyIdName(EDifficultyLevel Level);

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|Mode")
	FOnModeChanged OnModeChanged;

private:
	UPROPERTY()
	EGameModeId ActiveMode = EGameModeId::Batting;

	UPROPERTY()
	EDifficultyLevel ActiveDifficulty = EDifficultyLevel::Amateur;

	/** 세션 내 누적 결과 (저장/피드백 입력). */
	UPROPERTY()
	TArray<FScoreResult> SessionResults;

	/** 현재 세션 시작 시각. SetActiveMode 진입 시점에 찍어 FinalizeSession 에서 쓴다. */
	FDateTime SessionStartedAt = FDateTime();

	/** 로컬 저장 오브젝트 (Initialize 에서 로드/생성, FinalizeSession 에서 슬롯에 기록). */
	UPROPERTY()
	TObjectPtr<UMotionBaseSaveGame> SaveData;

	/** SaveData 를 슬롯에 기록. */
	void PersistSaveData() const;
};
