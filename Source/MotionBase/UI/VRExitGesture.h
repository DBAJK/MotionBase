#pragma once

#include "CoreMinimal.h"

/**
 * VR '컨트롤러 위로 들어 나가기' 제스처 (공통 유틸).
 *
 * 왜 필요한가: 이 프로젝트는 Vive 버튼 입력 인프라가 없어(전역 dwell/제스처 방식) 헤드셋만
 * 쓰면 게임 모드에서 모드 선택으로 돌아갈 수단이 없었다(뒤로가기 = 키보드 M 뿐). 컨트롤러
 * (또는 배트)를 천장 쪽으로 향하게 들고 HoldSec 초 유지하면 발동 → 소유 폰이 메뉴로 복귀.
 *
 * 플레이 중 자연 동작(스윙/포구/송구)은 위를 '연속으로 유지'하지 않으므로 오작동이 적다.
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
	 * @param bTracked      컨트롤러 추적 여부. 추적 안 되면 진행 리셋(오발동 방지).
	 * @param DeltaSeconds  프레임 델타.
	 * @param bOutTriggered 이번 프레임에 막 HoldSec 를 넘겨 발동했으면 true (한 번만).
	 * @return 진행도 0..1 (힌트 진행바용).
	 */
	float Update(const FVector& AimForward, bool bTracked, float DeltaSeconds, bool& bOutTriggered)
	{
		bOutTriggered = false;

		if (bTracked && AimForward.Z >= UpThreshold)
		{
			const bool bWasBelow = (HeldSec < HoldSec);
			HeldSec = FMath::Min(HeldSec + DeltaSeconds, HoldSec);
			if (bWasBelow && HeldSec >= HoldSec)
			{
				bOutTriggered = true;
			}
		}
		else
		{
			HeldSec = 0.0f;
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
