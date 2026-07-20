#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/ScoreResult.h"
#include "Data/MotionBaseTypes.h"
#include "MotionBaseSaveGame.generated.h"

/** 사용자별 플레이 기록 1건. */
USTRUCT(BlueprintType)
struct FPlayRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Record")
	EGameModeId Mode = EGameModeId::Batting;

	UPROPERTY(BlueprintReadWrite, Category = "Record")
	FScoreResult Score;

	UPROPERTY(BlueprintReadWrite, Category = "Record")
	FDateTime Timestamp;
};

/**
 * 로컬 저장 (백엔드/DB 없음 — CLAUDE §3 결정).
 * Play Result Logger 의 영속 계층. UGameplayStatics::SaveGameToSlot 로 저장.
 */
UCLASS()
class MOTIONBASE_API UMotionBaseSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString SlotName = TEXT("MotionBase_Player0");

	UPROPERTY()
	uint32 UserIndex = 0;

	/** 개인 기준선 (신체 인식 모드에서 캘리브레이션). 난이도 기준선. */
	UPROPERTY(BlueprintReadWrite, Category = "Profile")
	float BaselineReachCm = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Profile")
	float BaselineHeightCm = 0.0f;

	/** 누적 플레이 기록. AI 피드백 프롬프트 구성 입력. */
	UPROPERTY(BlueprintReadWrite, Category = "History")
	TArray<FPlayRecord> History;
};
