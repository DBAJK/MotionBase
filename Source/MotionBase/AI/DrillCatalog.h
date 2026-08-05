#pragma once

#include "CoreMinimal.h"
#include "Data/TrainingFeedback.h"
#include "DrillCatalog.generated.h"

/**
 * 약점 축 → 추천 드릴 매핑 (큐레이션 카탈로그).
 *
 * ⚠️ LLM 이 운동을 **자유 생성하지 않는다.** 물리 훈련 제품이라 잘못된 처방은 부상
 *    위험이 있으므로, 드릴은 여기서 사람이 작성·검수하고 약점 축 태그를 단다.
 *    LLM 은 이 카탈로그에서 이미 선택된 드릴을 "왜 필요한지 설명"만 한다.
 *
 * 드릴을 늘리려면 CPP 의 BuildCatalog() 에 항목을 추가하면 된다 — 코드 로직 변경 불필요.
 * (후반: DataTable 에셋으로 옮겨 기획자가 편집하게 할 수 있다.)
 */
UCLASS()
class MOTIONBASE_API UDrillCatalog : public UObject
{
	GENERATED_BODY()

public:
	/** 한 약점 축을 겨냥하는 드릴들. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Feedback")
	static TArray<FTrainingDrill> DrillsForAxis(EWeaknessAxis Axis);

	/**
	 * 약점 리포트 → 추천 드릴 (결정론적).
	 * 시급한 약점부터 드릴을 뽑아 MaxDrills 개까지, 중복 없이 반환한다.
	 * 약점이 없으면 유지용 기본 드릴 1개를 돌려준다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Feedback")
	static TArray<FTrainingDrill> Recommend(const FWeaknessReport& Report, int32 MaxDrills = 3);
};
