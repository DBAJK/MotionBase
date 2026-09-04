#pragma once

#include "CoreMinimal.h"
#include "MotionBaseTypes.generated.h"

/**
 * 게임 모드 (야구 흐름: 셋업 → 반응 → 수비 → 주루 → 타격 → 피트니스) + AI 코칭.
 * ⚠️ 저장 세이브가 값(uint8)으로 모드를 기록한다 — 값을 새로 끼워넣지 말 것.
 *    (기존 값의 번호가 밀려 옛 기록이 다른 모드로 읽힌다.) 항상 **끝에만** 추가.
 */
UENUM(BlueprintType)
enum class EGameModeId : uint8
{
	BodyScan       UMETA(DisplayName = "신체 인식(셋업)"),      // LiDAR 위치·키·범위 스캔
	ReactionSpeed  UMETA(DisplayName = "반응속도 훈련"),        // LiDAR 발 위치
	Defense        UMETA(DisplayName = "수비 훈련"),            // LiDAR 전신 자세
	BaseRunning    UMETA(DisplayName = "베이스 러닝"),          // LiDAR 위치·이동
	Batting        UMETA(DisplayName = "타격 훈련"),            // ★ Vive 컨트롤러 (핵심)
	FunctionalFitness UMETA(DisplayName = "기능성 피트니스"),    // LiDAR 관절각·반복
	AICoaching     UMETA(DisplayName = "AI 코칭")               // 기록 기반 AI 운동 추천 (읽기 전용)
};

/** 입력 소스 구분. 계층/난이도 설계와 provider 선택에 사용. */
UENUM(BlueprintType)
enum class EInputSource : uint8
{
	Mock           UMETA(DisplayName = "Mock (PC 개발/테스트)"),
	ViveController UMETA(DisplayName = "Vive 컨트롤러"),
	Lidar          UMETA(DisplayName = "LiDAR"),
	Camera         UMETA(DisplayName = "카메라 (MediaPipe 자세)")
};

/** 투구 구종. 타격 모드 난이도 구성. */
UENUM(BlueprintType)
enum class EPitchType : uint8
{
	Fastball  UMETA(DisplayName = "직구"),
	Breaking  UMETA(DisplayName = "변화구")
};

/**
 * 난이도 단계. 타격 모드에서 투구 파라미터(구속·변화구 비율·간격 등) 프리셋으로 매핑된다.
 * (APitchingZone::ApplyDifficulty). 다른 모드도 나중에 같은 축을 재사용할 수 있다.
 */
UENUM(BlueprintType)
enum class EDifficultyLevel : uint8
{
	Beginner  UMETA(DisplayName = "초보"),
	Amateur   UMETA(DisplayName = "아마추어"),
	Pro       UMETA(DisplayName = "프로")
};

/**
 * 타석(타자 스탠스). 타격 모드 전용 — 모드 선택 3단계(모드→난이도→스탠스)의 마지막.
 * 타자가 서는 타석(홈플레이트 좌/우)과 스윙·당겨치기 방향을 결정한다.
 *   Right(우타) = 3루 쪽 타석, Left(좌타) = 1루 쪽 타석.
 */
UENUM(BlueprintType)
enum class EBattingStance : uint8
{
	Right  UMETA(DisplayName = "우타"),
	Left   UMETA(DisplayName = "좌타")
};

/**
 * 베이스 종류. 수비 계열 훈련의 공통 어휘 —
 * 송구(목표 베이스 지정)·백업(커버 대상)이 같은 열거형을 쓴다.
 * (원래 Cover/CoverTypes.h 에 있던 것을 계약 계층으로 올렸다. UENUM 경로는
 *  모듈 기준(/Script/MotionBase.EBaseType)이라 헤더를 옮겨도 에셋 참조는 유지된다.)
 */
UENUM(BlueprintType)
enum class EBaseType : uint8
{
	First   UMETA(DisplayName = "1루"),
	Second  UMETA(DisplayName = "2루"),
	Third   UMETA(DisplayName = "3루"),
	Home    UMETA(DisplayName = "홈")
};

/**
 * 수비 포지션. 백업 위치 판단 훈련이 플레이어에게 고르게 하는 7개 야수 자리
 * (투수·포수 제외 — 둘은 타구 처리 주체라 "백업하러 이동"하는 자리가 아니다).
 *
 * ModeSelectPawn(메뉴)·ModeManager(세션 상태)·Core/Defense/Backup(판정) 이 모두 이 값을
 * 공유해야 해서 EBaseType 과 같은 자리(공용 어휘 계층)에 둔다.
 * ⚠️ 저장 세이브에 값이 안 남으므로(세션 상태로만 쓰임) 순서 변경은 안전하지만,
 *    그래도 관례상 끝에만 추가할 것.
 */
UENUM(BlueprintType)
enum class EFieldPosition : uint8
{
	First   UMETA(DisplayName = "1루수"),
	Second  UMETA(DisplayName = "2루수"),
	Short   UMETA(DisplayName = "유격수"),
	Third   UMETA(DisplayName = "3루수"),
	Left    UMETA(DisplayName = "좌익수"),
	Center  UMETA(DisplayName = "중견수"),
	Right   UMETA(DisplayName = "우익수")
};
