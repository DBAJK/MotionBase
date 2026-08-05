#pragma once

#include "CoreMinimal.h"
#include "CameraPose.generated.h"

/**
 * MediaPipe Pose 랜드마크 부분집합 — 타격 신체역학에 필요한 관절만 추린다.
 * ()안은 MediaPipe Pose(33 랜드마크)의 원본 인덱스. 외부 파이썬 프로세스가
 * 이 인덱스로 보내면 UE 수신부가 EBodyLandmark 로 매핑한다.
 *
 * ⚠️ MediaPipe 좌표계(y-down, 정규화 0~1) → UE(cm, z-up) 변환은 수신부 책임.
 *    이 계약에 들어올 때는 이미 cm·UE 좌표계로 정규화된 값이어야 한다.
 */
UENUM(BlueprintType)
enum class EBodyLandmark : uint8
{
	Nose        UMETA(DisplayName = "코(머리)"),        // 0  — 머리 고정성
	LeftShoulder  UMETA(DisplayName = "왼어깨"),         // 11
	RightShoulder UMETA(DisplayName = "오른어깨"),       // 12
	LeftElbow   UMETA(DisplayName = "왼팔꿈치"),         // 13
	RightElbow  UMETA(DisplayName = "오른팔꿈치"),       // 14
	LeftWrist   UMETA(DisplayName = "왼손목"),           // 15
	RightWrist  UMETA(DisplayName = "오른손목"),         // 16
	LeftHip     UMETA(DisplayName = "왼엉덩이"),         // 23
	RightHip    UMETA(DisplayName = "오른엉덩이"),       // 24
	LeftKnee    UMETA(DisplayName = "왼무릎"),           // 25
	RightKnee   UMETA(DisplayName = "오른무릎"),         // 26
	LeftAnkle   UMETA(DisplayName = "왼발목"),           // 27
	RightAnkle  UMETA(DisplayName = "오른발목"),         // 28

	Count       UMETA(Hidden)   // 배열 크기용 sentinel — 항상 마지막
};

/**
 * 관절 하나의 한 프레임 상태.
 * MediaPipe 는 관절마다 visibility(가림/신뢰도 0~1)를 준다 — 오클루전 판정에 쓴다.
 */
USTRUCT(BlueprintType)
struct FBodyLandmark
{
	GENERATED_BODY()

	/** 관절 월드 위치 (cm, UE z-up). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CameraPose")
	FVector Position = FVector::ZeroVector;

	/** MediaPipe visibility 0~1. 낮으면 가려졌거나 추정치 — 지표 계산 시 가중/배제 근거. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CameraPose")
	float Visibility = 0.0f;
};

/**
 * 카메라(MediaPipe) 자세 한 프레임. FSwingSample 과 대칭되는 계층 간 계약
 * (외부 비전 프로세스 → UE 수신부 → 계산 계층).
 *
 * ★ 시각 동기화: TimeSeconds 는 Vive 스윙 스트림(FSwingSample.TimeSeconds)과
 *   **같은 기준시(초, double)** 여야 한다. 그래야 "힙 피크가 컨택보다 몇 ms 앞섰나"
 *   (kinetic chain) 를 잴 수 있다. 수신부에서 카메라 클럭 오프셋을 보정해 맞춘다.
 */
USTRUCT(BlueprintType)
struct FCameraPoseFrame
{
	GENERATED_BODY()

	/** 프레임 시각 (초). Vive 스윙 클럭과 정렬된 공통 기준시. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CameraPose")
	double TimeSeconds = 0.0;

	/**
	 * 관절 위치 배열. EBodyLandmark 를 인덱스로 사용한다
	 * (크기 = (int32)EBodyLandmark::Count). GetLandmark() 로 안전 접근.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CameraPose")
	TArray<FBodyLandmark> Landmarks;

	/** 이 프레임 전체 추적 유효 여부 (사람 미검출/전면 오클루전 시 false). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CameraPose")
	bool bTracked = false;

	FCameraPoseFrame()
	{
		Landmarks.SetNum(static_cast<int32>(EBodyLandmark::Count));
	}

	/** 안전 접근자 — 범위 밖이면 기본값(Visibility=0) 반환. */
	FORCEINLINE const FBodyLandmark& GetLandmark(EBodyLandmark Joint) const
	{
		static const FBodyLandmark Empty;
		const int32 Idx = static_cast<int32>(Joint);
		return Landmarks.IsValidIndex(Idx) ? Landmarks[Idx] : Empty;
	}
};

/**
 * UBodyMechanicsAnalyzer 가 FCameraPoseFrame 시퀀스에서 뽑아낸 타격 신체역학 지표.
 * (계산 계층 → 점수/AI 피드백 입력). 각도 도(°), 거리 cm, 시간 ms.
 *
 * ⚠️ 판정 임계값(정상 X-factor 범위, 체인 순서 허용 오차 등)은 실측 캘리브레이션 대상 —
 *    지표 구조체에 하드코딩하지 않는다. 임계값은 별도 FBodyMechanicsConfig(분석기와 함께).
 */
USTRUCT(BlueprintType)
struct FBodyMechanicsMetrics
{
	GENERATED_BODY()

	/**
	 * X-factor — 힙 회전각 대비 어깨 회전각의 최대 분리각 (°).
	 * 상하체 비틀림 = 파워 저장. 클수록(적정 범위 내) 파워 잠재력 큼.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float HipShoulderSeparationDeg = 0.0f;

	/** 컨택 순간 힙 회전각 (°, 정면 기준). */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float HipRotationAtContactDeg = 0.0f;

	/** 컨택 순간 어깨 회전각 (°, 정면 기준). */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float ShoulderRotationAtContactDeg = 0.0f;

	/**
	 * Kinetic chain 순서 정상 여부. 이상적 순서: 힙 → 몸통/어깨 → 손 피크가
	 * 시간차를 두고 순차 발생. 역순/동시면 false (에너지 전달 비효율).
	 */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	bool bKineticChainOrdered = false;

	/** 힙 각속도 피크가 컨택보다 앞선 시간 (ms). 양수=선행(정상). */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float HipPeakLeadMs = 0.0f;

	/** 어깨 각속도 피크가 컨택보다 앞선 시간 (ms). 양수=선행. */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float ShoulderPeakLeadMs = 0.0f;

	/** 무게중심(엉덩이 중점) 뒷발→앞발 수평 이동량 (cm). 체중 이동. */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float WeightShiftCm = 0.0f;

	/**
	 * 스윙 구간 머리(코) 이동량 (cm). 작을수록 좋음 — 시선/축 안정.
	 * TODO(캘리브레이션): '안정' 임계값은 실측.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float HeadTravelCm = 0.0f;

	/** 척추 기울기 (°, 수직 기준). 어깨중점→엉덩이중점 벡터 각도. */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float SpineTiltDeg = 0.0f;

	/**
	 * 이 지표 산출에 쓴 프레임들의 평균 추적 신뢰도 (0~1).
	 * 낮으면(가림·저조도) 지표를 점수/피드백에서 약하게 반영하거나 무시.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	float Confidence = 0.0f;

	/** 지표가 유효한지. 프레임 부족/추적 실패 시 false. */
	UPROPERTY(BlueprintReadWrite, Category = "CameraPose")
	bool bValid = false;
};
