#pragma once

#include "CoreMinimal.h"
#include "MotionBaseTypes.generated.h"

/** 6개 게임 모드 (야구 흐름: 셋업 → 반응 → 수비 → 주루 → 타격 → 피트니스). */
UENUM(BlueprintType)
enum class EGameModeId : uint8
{
	BodyScan       UMETA(DisplayName = "신체 인식(셋업)"),      // LiDAR 위치·키·범위 스캔
	ReactionSpeed  UMETA(DisplayName = "반응속도 훈련"),        // LiDAR 발 위치
	Defense        UMETA(DisplayName = "수비 훈련"),            // LiDAR 전신 자세
	BaseRunning    UMETA(DisplayName = "베이스 러닝"),          // LiDAR 위치·이동
	Batting        UMETA(DisplayName = "타격 훈련"),            // ★ Vive 컨트롤러 (핵심)
	FunctionalFitness UMETA(DisplayName = "기능성 피트니스")     // LiDAR 관절각·반복
};

/** 입력 소스 구분. 계층/난이도 설계와 provider 선택에 사용. */
UENUM(BlueprintType)
enum class EInputSource : uint8
{
	Mock           UMETA(DisplayName = "Mock (PC 개발/테스트)"),
	ViveController UMETA(DisplayName = "Vive 컨트롤러"),
	Lidar          UMETA(DisplayName = "LiDAR")
};

/** 투구 구종. 타격 모드 난이도 구성. */
UENUM(BlueprintType)
enum class EPitchType : uint8
{
	Fastball  UMETA(DisplayName = "직구"),
	Breaking  UMETA(DisplayName = "변화구")
};
