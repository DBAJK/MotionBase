#pragma once

#include "CoreMinimal.h"
#include "Data/CameraPoseFrame.h"
#include "MockCameraPoseSource.generated.h"

/**
 * Mock 포즈 소스가 만들 합성 스윙의 형태 파라미터.
 *
 * 실제 카메라(MediaPipe)가 없을 때 UBodyMechanicsAnalyzer 경로를 end-to-end 로
 * 돌리기 위한 스텁 입력이다 — 값 자체는 "그럴듯한 자리표시자"일 뿐 실측이 아니다.
 * MediaPipe 수신부가 붙으면 이 소스를 통째로 교체하고 분석기/표시 계층은 무수정으로 둔다.
 */
USTRUCT(BlueprintType)
struct FMockSwingPoseParams
{
	GENERATED_BODY()

	/** 시작→컨택 힙 회전량 (°). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float HipRotationDeg = 35.0f;

	/** 시작→컨택 어깨 회전량 (°). 힙보다 크게 두면 X-factor 가 양(+)으로 잡힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float ShoulderRotationDeg = 60.0f;

	/** 골반 중점 수평 이동량 (cm) — 체중 이동. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float WeightShiftCm = 12.0f;

	/** 스윙 구간 머리(코) 이동량 (cm) — 작을수록 축 안정. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float HeadTravelCm = 4.0f;

	/** 컨택 순간 척추 기울기 (°, 수직 기준). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float SpineTiltDeg = 12.0f;

	/** 힙 각속도 피크가 컨택보다 앞서는 시간 (초). 양수 = 정상 선행. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float HipPeakLeadSec = 0.12f;

	/** 어깨 각속도 피크가 컨택보다 앞서는 시간 (초). 힙보다 작게 두면 체인 순서가 맞는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float ShoulderPeakLeadSec = 0.06f;

	/** 컨택 이전 창 길이 (초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float PreContactSec = 0.30f;

	/** 컨택 이후 창 길이 (초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float PostContactSec = 0.05f;

	/** 샘플링 주파수 (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float SampleHz = 90.0f;

	/** 모든 관절 visibility (0~1). 게이트 통과 + 신뢰도 산출용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MockPose") float Visibility = 0.95f;
};

/**
 * Mock 카메라(MediaPipe) 포즈 소스. (테스트/PC 개발용 스텁)
 *
 * ⚠️ 실제 비전 파이프라인이 아니다. 실제 소스가 붙기 전까지 UBodyMechanicsAnalyzer 를
 *    라이브 흐름에서 실제로 돌려보기 위해, 목표 지표가 나오도록 관절 좌표를 역산해 배치한다.
 *    (각도→좌표: 좌우 관절을 중심 기준 ±각도로 벌려 놓으면 그 라인 방위각이 목표각이 된다.)
 *
 * ContactTime 은 Vive 스윙 클럭(FSwingSample.TimeSeconds)과 같은 기준시를 넣어야
 * kinetic chain 리드(ms)가 올바르게 계산된다.
 */
UCLASS()
class MOTIONBASE_API UMockCameraPoseSource : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 컨택 시각을 중심으로 한 합성 포즈 시퀀스를 만든다.
	 * @param ContactTime  컨택 시각(초). Vive 스윙 클럭과 정렬.
	 * @param Params       스윙 형태 파라미터.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input|Mock")
	static TArray<FCameraPoseFrame> BuildSwingSequence(double ContactTime, const FMockSwingPoseParams& Params);
};
