#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"

/**
 * VR 컨트롤러 햅틱 펄스 (공통 유틸).
 *
 * 왜 이 형태인가: UE 의 햅틱 에셋(UHapticFeedbackEffect_Curve)은 에디터에서 만들어야 하는데,
 * 이 프로젝트는 UMG·텍스처와 마찬가지로 **에셋 없이 코드만으로 동작**하는 것을 원칙으로 한다
 * (AModeSelectHUD 가 Canvas 프리미티브만 쓰는 것과 같은 이유 — 에셋이 빠지면 조용히 죽는다).
 * 그래서 APlayerController::SetHapticsByValue 로 직접 진동을 켜고, 지속시간이 지나면 끈다.
 *
 * ⚠️ SetHapticsByValue 는 **끌 때까지 계속 울린다.** 반드시 소유 폰이 매 프레임 Update() 를
 *    불러야 하고, 폰이 사라질 때(EndPlay) Stop() 을 불러야 한다. 안 그러면 모드를 나가도
 *    컨트롤러가 계속 진동한다.
 *
 * 상태만 들고 있는 POD 다 (컴포넌트 아님) — FVRExitGesture 와 같은 패턴.
 */
struct FVRHapticPulse
{
	/** 남은 재생 시간 (초). */
	float RemainingSec = 0.0f;

	/** 재생 중인지. */
	bool bActive = false;

	/** 어느 손에 울리고 있는지 (Stop 이 같은 손을 꺼야 하므로 기억해 둔다). */
	EControllerHand Hand = EControllerHand::Right;

	/**
	 * 펄스 시작. 이미 재생 중이면 새 값으로 덮어쓴다
	 * (연타로 두 번 맞아도 진동이 겹쳐 길어지지 않게 — 마지막 임팩트가 이긴다).
	 *
	 * @param PC           진동을 보낼 플레이어 컨트롤러. null 이면 무시.
	 * @param InHand       울릴 손.
	 * @param InFrequency  진동 주파수 0~1 (낮으면 묵직, 높으면 날카롭다).
	 * @param InAmplitude  진동 세기 0~1.
	 * @param DurationSec  지속 시간 (초). 0 이하면 무시.
	 */
	void Play(APlayerController* PC, EControllerHand InHand,
		float InFrequency, float InAmplitude, float DurationSec)
	{
		if (!PC || DurationSec <= 0.0f)
		{
			return;
		}

		Hand         = InHand;
		RemainingSec = DurationSec;
		bActive      = true;

		PC->SetHapticsByValue(
			FMath::Clamp(InFrequency, 0.0f, 1.0f),
			FMath::Clamp(InAmplitude, 0.0f, 1.0f),
			Hand);
	}

	/** 매 프레임 호출. 지속시간이 끝나면 스스로 끈다. */
	void Update(APlayerController* PC, float DeltaSeconds)
	{
		if (!bActive)
		{
			return;
		}

		RemainingSec -= DeltaSeconds;
		if (RemainingSec <= 0.0f)
		{
			Stop(PC);
		}
	}

	/** 즉시 정지. **EndPlay 에서 반드시 호출할 것** (폰이 사라져도 진동은 안 멈춘다). */
	void Stop(APlayerController* PC)
	{
		if (!bActive)
		{
			return;
		}

		bActive      = false;
		RemainingSec = 0.0f;

		if (PC)
		{
			PC->SetHapticsByValue(0.0f, 0.0f, Hand);
		}
	}
};
