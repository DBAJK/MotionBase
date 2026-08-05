#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/SessionResult.h"
#include "MotionBaseSaveGame.generated.h"

/**
 * 로컬 저장 (백엔드/DB 없음 — CLAUDE §3 결정).
 * Play Result Logger 의 영속 계층. UGameplayStatics::SaveGameToSlot 로 저장.
 *
 * 저장 단위는 FSessionResult(한 판=여러 시도) 로 통일한다. 개별 시도(FScoreResult)는
 * 그 안의 Attempts[] 에 담긴다 — 결과 화면·기록 비교가 세션 단위로 움직이기 때문.
 */
UCLASS()
class MOTIONBASE_API UMotionBaseSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** 슬롯 이름·유저 인덱스는 UModeManager 가 저장/로드 시 지정한다. */
	static constexpr const TCHAR* DefaultSlotName = TEXT("MotionBase_Player0");
	static constexpr uint32 DefaultUserIndex = 0;

	/** 개인 기준선 (신체 인식 모드에서 캘리브레이션). 난이도 기준선. */
	UPROPERTY(BlueprintReadWrite, Category = "Profile")
	float BaselineReachCm = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Profile")
	float BaselineHeightCm = 0.0f;

	/** 누적 세션 기록. 결과 화면·기록 비교·AI 피드백 프롬프트 구성 입력. */
	UPROPERTY(BlueprintReadWrite, Category = "History")
	TArray<FSessionResult> History;
};
