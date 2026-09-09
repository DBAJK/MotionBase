#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/MotionBaseTypes.h"
#include "Data/ScoreResult.h"
#include "Data/SessionResult.h"
#include "Data/OverallScore.h"
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
	void SetActiveMode(EGameModeId NewMode, EDifficultyLevel NewDifficulty = EDifficultyLevel::Amateur,
		EBattingStance NewStance = EBattingStance::Right);

	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	EGameModeId GetActiveMode() const { return ActiveMode; }

	/** 현재 세션의 난이도. 모드 폰(ASwingTestPawn 등)이 읽어 파라미터에 반영한다. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	EDifficultyLevel GetActiveDifficulty() const { return ActiveDifficulty; }

	/** 현재 세션의 타석(좌타/우타). 타격 폰이 읽어 타석 위치·타구 방향에 반영한다. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	EBattingStance GetActiveStance() const { return ActiveStance; }

	/**
	 * 모드 안의 세부 종목을 지정한다 (수비: "Catch"/"Throw"/"Backup").
	 * ⚠️ SetActiveMode 뒤에 불러야 한다 — SetActiveMode 가 새 세션을 열면서 이 값을 비운다.
	 * 저장 시 FSessionResult::DrillId 로 남아, 추세 분석이 종목별로 갈라진다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void SetActiveDrill(FName DrillId);

	/** 현재 세션의 세부 종목. 세부 종목이 없는 모드는 NAME_None. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	FName GetActiveDrill() const { return ActiveDrill; }

	/** 수비 세부 종목 인덱스(0=포구,1=송구,2=백업) → 저장·집계용 안정 식별자. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FName GetDefenseDrillIdName(int32 DrillIndex);

	/**
	 * 백업 위치 판단 훈련에서 플레이어가 고른 수비 포지션을 지정한다 (7개 야수 자리).
	 * ⚠️ SetActiveMode/SetActiveDrill 뒤에 불러야 한다 — SetActiveMode 가 세션을 열며 이 값을
	 * First 로 되돌린다. ActiveStance 와 같은 패턴(폰 스폰 시 커스텀 파라미터를 못 넘기는
	 * 구조라 세션 상태를 경유해 새 폰의 BeginPlay 에 전달한다).
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	void SetActiveFieldPosition(EFieldPosition InPosition);

	/** 현재 세션의 수비 포지션 (백업 훈련 외에는 의미 없음). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	EFieldPosition GetActiveFieldPosition() const { return ActiveFieldPosition; }

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

	// ── AI 플레이 해설 캐시 ──
	// (플레이 × 포지션) 조합은 상황이 고정이라 해설도 항상 같다. 한 번 받아두면 재사용할 수
	// 있어서, 부스에서 하루 종일 반복 시연해도 같은 조합에 다시 과금되지 않는다.
	//
	// ⚠️ 왜 폰이나 AI 서비스가 아니라 여기인가: 둘 다 드릴을 나가면 파괴된다. 이 서브시스템은
	//    GameInstance 수명이라 폰 교체·모드 전환을 넘어 살아남는다. **SetActiveMode 의
	//    세션 초기화가 이 캐시를 건드리면 안 된다** — 세션과 무관한 자산이다.
	//    (앱을 재시작하면 비워진다. 디스크 영속은 필요해지면 그때.)

	/**
	 * 종합 점수(공격 50 + 수비 50)의 종목 정의 목록.
	 *
	 * ⚠️ **여기가 유일한 작성 지점이다.** 종목 식별자("Catch"/"Throw"/"BackupMove")는 저장에
	 *    남는 값이고 이 클래스가 정본(GetDefenseDrillIdName)을 쥐고 있다. 다른 파일에서
	 *    리터럴로 복사해 두면 ID 가 바뀔 때 조용히 어긋난다 — 실제로 "Backup" →
	 *    "BackupMove" 변경 때 AI 코칭 화면이 그렇게 깨졌다.
	 *
	 * 배분: 공격(타격) 50, 수비 3종목 각 50/3. 균등 배분인 이유는 가중치를 임의로 두면
	 * 근거를 댈 수 없기 때문 — 실측 후 조정하려면 이 함수만 고치면 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Mode")
	static TArray<FOverallCategoryDef> BuildOverallCategories();

	/**
	 * 공격 50 + 수비 50 = 100점 종합. **이력이 바뀔 때만 다시 계산한다.**
	 *
	 * ⚠️ 호출부가 직접 ComputeOverall 을 부르지 않게 하려고 여기 둔다. 두 가지를 동시에 막는다:
	 *   1) **오래된 값**: 세션 저장(FinalizeSession)은 드릴 폰의 EndPlay 에서 일어나는데,
	 *      폰 교체는 **새 폰을 먼저 스폰**하고 옛 폰을 나중에 파괴한다. 그래서 메뉴 폰이
	 *      BeginPlay 에서 미리 계산해 두면 **방금 끝낸 세션이 빠진 값**을 들고 있게 된다.
	 *      여기서 버전으로 무효화하면, 저장이 끝난 뒤 처음 읽는 시점에 최신값이 나온다.
	 *   2) **매 프레임 재계산**: HUD 는 DrawHUD 에서 읽는데, ComputeOverall 은 저장 이력
	 *      전체를 4번 훑고 카테고리 배열도 매번 할당한다. 부스 하루치 이력이면 무시 못 한다.
	 */
	const FOverallScore& GetOverallScore() const;

	/** 캐시에 있으면 true 와 함께 해설을 돌려준다. */
	bool TryGetPlayExplanation(const FString& Key, FString& OutText) const;

	/** 해설을 캐시에 넣는다. 빈 문자열은 무시한다(실패 응답을 캐싱하지 않기 위함). */
	void CachePlayExplanation(const FString& Key, const FString& Text);

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

	// ── 스탠스(좌타/우타) 선택용 메타데이터 (타격 모드 전용) ──

	/** 타석 목록 (우타, 좌타 순). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static TArray<EBattingStance> GetMenuStances();

	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FText GetStanceDisplayName(EBattingStance Stance);

	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FText GetStanceDescription(EBattingStance Stance);

	/** 저장·집계용 안정 식별자 (ASCII). */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Mode")
	static FName GetStanceIdName(EBattingStance Stance);

	UPROPERTY(BlueprintAssignable, Category = "MotionBase|Mode")
	FOnModeChanged OnModeChanged;

private:
	UPROPERTY()
	EGameModeId ActiveMode = EGameModeId::Batting;

	UPROPERTY()
	EDifficultyLevel ActiveDifficulty = EDifficultyLevel::Amateur;

	UPROPERTY()
	EBattingStance ActiveStance = EBattingStance::Right;

	/** 현재 세션의 세부 종목 (수비 전용). SetActiveMode 가 비우고 SetActiveDrill 이 채운다. */
	UPROPERTY()
	FName ActiveDrill = NAME_None;

	/** 현재 세션의 수비 포지션 (백업 훈련 전용). SetActiveMode 가 First 로 되돌린다. */
	UPROPERTY()
	EFieldPosition ActiveFieldPosition = EFieldPosition::First;

	/** 세션 내 누적 결과 (저장/피드백 입력). */
	UPROPERTY()
	TArray<FScoreResult> SessionResults;

	/**
	 * AI 플레이 해설 캐시 ("<PlayId>|<Position>" → 해설).
	 * ⚠️ 세션 상태가 아니다 — SetActiveMode 에서 절대 비우지 말 것.
	 */
	TMap<FString, FString> PlayExplanationCache;

	// ── 종합 점수 캐시 ──
	// 이력이 바뀔 때마다 HistoryVersion 을 올리고, 캐시가 그보다 오래됐을 때만 다시 계산한다.
	// mutable 인 이유: GetOverallScore() 는 논리적으로 읽기 연산이라 const 로 두는 게 맞다.
	int32 HistoryVersion = 0;
	mutable int32 CachedOverallVersion = -1;
	mutable FOverallScore CachedOverall;

	/** 현재 세션 시작 시각. SetActiveMode 진입 시점에 찍어 FinalizeSession 에서 쓴다. */
	FDateTime SessionStartedAt = FDateTime();

	/** 로컬 저장 오브젝트 (Initialize 에서 로드/생성, FinalizeSession 에서 슬롯에 기록). */
	UPROPERTY()
	TObjectPtr<UMotionBaseSaveGame> SaveData;

	/** SaveData 를 슬롯에 기록. */
	void PersistSaveData() const;
};
