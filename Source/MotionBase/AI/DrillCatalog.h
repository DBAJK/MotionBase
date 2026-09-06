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
 * ⚠️ **수행량(Prescription)도 마찬가지로 여기서 확정된다.** "15회 3세트" 같은 숫자를
 *    LLM 이 지어내면 검수되지 않은 처방이 그대로 사용자에게 나간다. 프롬프트는
 *    이 값을 **그대로 인용**하라고 지시하며, 반올림·조정·중량 추가를 금지한다.
 *
 * 드릴을 늘리려면 CPP 의 DrillsForAxis() 에 항목을 추가하면 된다 — 코드 로직 변경 불필요.
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

	/**
	 * 이번 세션 리포트 + 과거 만성 추세를 함께 반영한 추천 (결정론적).
	 * Recommend 와 달리:
	 *   - 만성 축(여러 세션 반복)·악화 축을 우선순위 위로 끌어올린다.
	 *   - 같은 축이 반복 처방될 때 드릴을 로테이션해 매번 같은 운동만 나오지 않게 한다.
	 * Chronic.bValid=false(이력 부족)면 Recommend 와 동일하게 동작한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Feedback")
	static TArray<FTrainingDrill> RecommendWithHistory(
		const FWeaknessReport& Report, const FChronicWeaknessReport& Chronic, int32 MaxDrills = 3);
};
