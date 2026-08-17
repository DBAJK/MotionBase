#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/SessionSummary.h"
#include "SessionResultView.generated.h"

UINTERFACE(MinimalAPI)
class USessionResultView : public UInterface
{
	GENERATED_BODY()
};

/**
 * 세션 결과 화면을 HUD 가 그릴 수 있도록 요약 데이터를 제공하는 폰 인터페이스.
 *
 * HUD(AModeSelectHUD)는 이 인터페이스로만 결과를 읽으므로 특정 폰 구현에 묶이지 않는다 —
 * 임시 폰(ASwingTestPawn)이든 나중의 ABat 폰이든 이 인터페이스만 구현하면 같은 결과 화면을 쓴다.
 */
class ISessionResultView
{
	GENERATED_BODY()

public:
	/**
	 * 지금 결과 화면을 띄워야 하면 OutSummary 를 채우고 true, 아니면 false.
	 * (플레이 중에는 false → HUD 는 결과 패널을 그리지 않는다.)
	 */
	virtual bool GetSessionSummary(FSessionSummary& OutSummary) const = 0;
};
