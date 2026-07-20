#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MotionBaseGameMode.generated.h"

/**
 * MotionBase 기본 게임 모드. 독립 실행형 UE 프로토타입 진입점.
 * (뉴작 SporTrack 연동 스펙 확보 전까지 standalone 으로 개발 — CLAUDE §9)
 */
UCLASS()
class MOTIONBASE_API AMotionBaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMotionBaseGameMode();
};
