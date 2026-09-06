#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/MotionBaseTypes.h"
#include "MotionBaseGameMode.generated.h"

/**
 * MotionBase 기본 게임 모드. 독립 실행형 UE 프로토타입 진입점.
 * (뉴작 SporTrack 연동 스펙 확보 전까지 standalone 으로 개발 — CLAUDE §9)
 *
 * 흐름: 시작 화면(AModeSelectPawn) → 모드 선택 → 해당 모드 폰으로 교체 → [M] 로 복귀.
 * 레벨 이동 없이 폰만 갈아끼우므로 전환 로딩이 없다 (전시 부스에서 중요).
 */
UCLASS()
class MOTIONBASE_API AMotionBaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMotionBaseGameMode();

	/**
	 * 선택된 모드+난이도로 진입한다. 실제 폰 교체는 다음 틱에 일어난다.
	 * @return 미구현 모드이거나 폰이 등록돼 있지 않으면 false (시작 화면 유지).
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Flow")
	bool StartMode(EGameModeId Mode, EDifficultyLevel Difficulty = EDifficultyLevel::Amateur,
		EBattingStance Stance = EBattingStance::Right);

	/**
	 * 수비 세부 종목 선택 → 해당 훈련 폰으로 진입. (0=포구, 1=송구, 2=백업)
	 * @param Position 백업 위치 판단(2번)에서 플레이어가 고른 수비 포지션. 다른 종목에선 무시된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Flow")
	bool StartDefenseDrill(int32 DrillIndex, EFieldPosition Position = EFieldPosition::First);
	
	/**
	 * AI 코칭(운동 추천) 리뷰 화면으로 진입. 게임 세션이 아니라 저장 이력을 읽는
	 * 읽기 전용 화면이라, SetActiveMode(새 세션 개시)를 거치지 않고 폰만 교체한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Flow")
	void StartAICoaching();

	/** 시작 화면(모드 선택)으로 복귀. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Flow")
	void ReturnToModeSelect();

	/**
	 * Vive 브링업 진단 하네스로 진입 (Phase 2 작업용).
	 * 게임 모드가 아니라 개발 도구이므로 모드 목록에 넣지 않고 별도 키로 연다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Flow")
	void StartViveBringup();

protected:
	/** 모드별 플레이 폰. 미구현 모드는 null 을 돌려준다. */
	TSubclassOf<APawn> GetPawnClassForMode(EGameModeId Mode) const;

	/**
	 * 폰 교체를 다음 틱으로 예약한다.
	 *
	 * 전환은 항상 폰의 입력 콜백(모드 선택 확정 / [M] 복귀) 안에서 시작된다.
	 * 그 자리에서 바로 UnPossess→Destroy 를 하면 아직 처리 중인 입력 스택에서
	 * 자기 InputComponent 를 빼버리는 셈이라 위험하다. 한 틱 미뤄 콜백을 빠져나온 뒤 교체한다.
	 */
	void RequestPawnSwap(TSubclassOf<APawn> NewPawnClass);

	/** 예약된 교체를 실행. 새 폰은 기존 폰과 같은 트랜스폼에 생성된다. */
	void ApplyPendingPawnSwap();

	UPROPERTY(Transient)
	TSubclassOf<APawn> PendingPawnClass;
};
