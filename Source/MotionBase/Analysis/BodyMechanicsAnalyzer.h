#pragma once

#include "CoreMinimal.h"
#include "Data/CameraPoseFrame.h"
#include "BodyMechanicsAnalyzer.generated.h"

/**
 * 신체역학 분석 유효성 게이트 (판정 임계값 아님 — 측정 신뢰 여부만).
 *
 * ⚠️ '좋은 X-factor 범위' 같은 판정 임계값은 여기 두지 않는다 (점수 계층 소관).
 *    이 구조체는 "이 프레임/시퀀스를 믿고 계산할지"만 정한다.
 */
USTRUCT(BlueprintType)
struct FBodyMechanicsConfig
{
	GENERATED_BODY()

	/** 관절 신뢰도 게이트. Visibility가 이 값 미만인 관절은 그 프레임에서 무시. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	float MinVisibility = 0.5f;

	/** 지표를 산출하려면 필요한 최소 유효 프레임 수. 미달 시 bValid=false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	int32 MinValidFrames = 5;

	/**
	 * 실측 캘리브레이션 완료 여부. 게이트 값들이 실측으로 정해졌는지.
	 * ⚠️ false면 기본값은 개발용 플레이스홀더.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	bool bCalibrated = false;
};

/**
 * 카메라(MediaPipe) 자세 시퀀스 → 타격 신체역학 지표. (계산 계층, UE 액터/렌더 비의존)
 *
 * 입력은 숫자(FCameraPoseFrame 배열)뿐, 출력도 숫자(FBodyMechanicsMetrics)뿐이므로
 * 카메라·헤드셋 없이 Mock 데이터로 단위 테스트 가능. static 순수 함수로 구현.
 *
 * 좌표 규약: 위치 cm, UE z-up. 회전각은 수평면(XY)에서 좌→우 관절 라인의 방위각.
 * 시간 규약: FCameraPoseFrame.TimeSeconds 와 ContactTime 은 같은 기준시(초).
 */
UCLASS()
class MOTIONBASE_API UBodyMechanicsAnalyzer : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 자세 시퀀스에서 타격 신체역학 지표를 계산한다.
	 * @param Frames       시간순 정렬된 카메라 자세 프레임 (cm, 초).
	 * @param ContactTime  컨택 시각 (초). Vive 스윙 클럭과 정렬된 값.
	 * @param Config       유효성 게이트.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Analysis")
	static FBodyMechanicsMetrics Analyze(
		const TArray<FCameraPoseFrame>& Frames,
		double ContactTime,
		const FBodyMechanicsConfig& Config);

	// ---- 헬퍼 (테스트에서 직접 검증 가능) ----

	/** 좌→우 관절을 잇는 라인의 수평면(XY) 방위각 (도). atan2(dy, dx). */
	static float HorizontalLineAngleDeg(const FVector& Left, const FVector& Right);

	/** 각도 차를 (-180, 180] 로 래핑. */
	static float WrapDeg(float Deg);

	/** 시각 T 에 가장 가까운 프레임 인덱스. 빈 배열이면 INDEX_NONE. */
	static int32 FindFrameNearestTime(const TArray<FCameraPoseFrame>& Frames, double T);

	/**
	 * 좌우 관절 라인의 각속도 피크 (deg/s)와 그 시각을 찾는다.
	 * 각도는 언랩(unwrap)해 360° 점프를 제거. Visibility 게이트 통과 프레임만 사용.
	 * @return 피크 |각속도| (deg/s). 유효 표본 부족 시 0, OutTime=음수.
	 */
	static float FindAngularVelocityPeak(
		const TArray<FCameraPoseFrame>& Frames,
		EBodyLandmark Left,
		EBodyLandmark Right,
		float MinVisibility,
		double& OutTime);
};
