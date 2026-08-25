#pragma once

#include "CoreMinimal.h"

/**
 * VR '컨트롤러 위로 들어 나가기' 제스처 (공통 유틸).
 *
 * 왜 필요한가: 이 프로젝트는 Vive 버튼 입력 인프라가 없어(전역 dwell/제스처 방식) 헤드셋만
 * 쓰면 게임 모드에서 모드 선택으로 돌아갈 수단이 없었다(뒤로가기 = 키보드 M 뿐). 컨트롤러
 * (또는 배트)를 천장 쪽으로 향하게 들고 HoldSec 초 유지하면 발동 → 소유 폰이 메뉴로 복귀.
 *
 * ⚠️ **자연 동작과 겹치는 종목이 있다.** 타자의 준비 자세는 배트를 거의 수직으로 세우고,
 *    뜬공 포구는 글러브를 위로 든 채 기다린다 — 둘 다 이 조건과 똑같다. 그래서 소유 폰이
 *    ① HoldSec/UpThreshold 를 종목에 맞게 조이고 ② 위험한 순간에는 bAllowed=false 로
 *    아예 꺼야 한다. 기본값은 '겨눔 동작이 없는 화면'(모드 선택·백업 퀴즈) 기준이다.
 *
 * 소유 폰이 상태(HeldSec)를 들고 매 프레임 Update() 를 호출한다. (컴포넌트 아님 — POD)
 */
struct FVRExitGesture
{
	/** 이 시간(초)간 계속 위를 향하면 발동. */
	float HoldSec = 1.5f;

	/** 위 판정: 조준 forward 의 Z 성분이 이 값 이상이면 '위를 향함'. 0.80 ≈ 수직에서 37° 이내. */
	float UpThreshold = 0.80f;

	/** 현재까지 연속 유지된 시간(초). 위를 벗어나면 0 으로 리셋. */
	float HeldSec = 0.0f;

	/**
	 * @param AimForward    컨트롤러/배트 조준 방향 (월드 단위벡터).
	 * @param bTracked      컨트롤러 추적 여부. 추적이 끊기면 진행 **리셋**(오발동 방지).
	 * @param bAllowed      지금 이 제스처를 받아도 되는 구간인지. false 면 진행이 **동결**된다
	 *                      (리셋이 아니다 — 아래 참고).
	 * @param DeltaSeconds  프레임 델타.
	 * @param bOutTriggered 이번 프레임에 막 HoldSec 를 넘겨 발동했으면 true (한 번만).
	 * @return 진행도 0..1 (힌트 진행바용).
	 *
	 * ⚠️ **동결이지 리셋이 아닌 이유**: 포구는 투구 간격(1.5s)이 홀드 시간(2.5s)보다 짧다.
	 *    위험 구간마다 0으로 되돌리면 진행이 영영 안 차서 **VR 에서 나갈 방법이 사라진다.**
	 *    동결이면 안전한 틈(구간 사이)마다 조금씩 쌓여 두어 번의 틈으로 나갈 수 있고,
	 *    공을 잡느라 잠깐 글러브를 든 것은 한 톨도 쌓이지 않는다.
	 *    단, 위를 향하지 않으면 구간과 무관하게 리셋한다 — "계속 들고 있어야" 성립하는 제스처다.
	 */
	float Update(const FVector& AimForward, bool bTracked, bool bAllowed, float DeltaSeconds, bool& bOutTriggered)
	{
		bOutTriggered = false;

		// 추적 끊김 또는 위를 향하지 않음 → 진행 리셋.
		if (!bTracked || AimForward.Z < UpThreshold)
		{
			HeldSec = 0.0f;
			return Progress();
		}

		// 위를 향하고는 있지만 지금은 받을 수 없는 구간 → 동결(유지, 누적 없음).
		if (!bAllowed)
		{
			return Progress();
		}

		const bool bWasBelow = (HeldSec < HoldSec);
		HeldSec = FMath::Min(HeldSec + DeltaSeconds, HoldSec);
		if (bWasBelow && HeldSec >= HoldSec)
		{
			bOutTriggered = true;
		}

		return Progress();
	}

	float Progress() const
	{
		return (HoldSec > 0.0f) ? FMath::Clamp(HeldSec / HoldSec, 0.0f, 1.0f) : 0.0f;
	}

	/** 위로 드는 중(진행 누적 중)인가 — 진행바를 보여줄지 판단용. */
	bool IsHolding() const { return HeldSec > KINDA_SMALL_NUMBER; }

	/** 힌트에 덧붙일 ASCII 진행바 (예: "[===...]"). 폰트 글리프 걱정 없음. */
	FString ProgressBar() const
	{
		const int32 Cells = 6;
		const int32 Filled = FMath::Clamp(FMath::RoundToInt(Progress() * Cells), 0, Cells);
		return FString::Printf(TEXT("[%s%s]"),
			*FString::ChrN(Filled, TEXT('=')), *FString::ChrN(Cells - Filled, TEXT('.')));
	}
};
