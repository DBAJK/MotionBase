#include "Core/MotionBaseGameMode.h"
#include "Testing/SwingTestPawn.h"

AMotionBaseGameMode::AMotionBaseGameMode()
{
	PrimaryActorTick.bCanEverTick = false;

	// Stage 1 화면 테스트: 스페이스바로 가상 스윙→점수. Vive 배선 후 VR 폰으로 교체.
	DefaultPawnClass = ASwingTestPawn::StaticClass();
}
