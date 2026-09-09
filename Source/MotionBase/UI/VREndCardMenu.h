#pragma once

#include "CoreMinimal.h"

class UVRInfoPanel;

/**
 * 세션 종료 화면의 **선택 카드**(다시 하기 / 메뉴로) — 조준 드웰 판정 공통 유틸.
 *
 * 왜 필요한가: 게임 모드에서 나가는 길이 '컨트롤러 위로 들기' 제스처와 패널 맨 아래
 * '뒤로' 카드뿐이었다. 둘 다 **플레이 중 오발동을 막으려고 조건을 빡빡하게 조여 둔 것**
 * (타격은 수직 ±18°·3초, 카드는 8°·1.6초)이라, 정작 세션이 끝나 나가려는 순간에도
 * 그 빡빡함이 그대로 남아 "끝났는데 돌아갈 수가 없다"가 된다.
 *
 * 세션이 끝난 뒤에는 오발동시킬 플레이 자체가 없다 — 공도 안 오고 스윙도 없다. 그래서
 * 종료 화면에서는 패널 행을 큼직한 카드 두 장으로 바꾸고, 넉넉한 각도로 겨누기만 하면
 * 고를 수 있게 한다. 모드 선택 화면(AModeSelectPawn)이 이미 쓰는 방식과 같은 판정이라
 * 플레이어가 배운 조작이 그대로 통한다.
 *
 * 소유 폰이 상태를 들고 매 프레임 Update() 를 호출한다. (컴포넌트 아님 — POD)
 */
struct FVREndCardMenu
{
	/** 카드를 이 시간(초) 동안 겨누고 있으면 확정. */
	float DwellSec = 1.5f;

	/** 이 각도(도) 안이면 그 카드를 겨눈 것으로 본다. '뒤로' 카드(8°)보다 넉넉하게. */
	float AngleDeg = 11.0f;

	/** 카드 블록이 놓인 첫 행 인덱스 (Update 가 채운다). */
	int32 FirstRow = 0;

	/** 현재 겨누고 있는 카드 — 블록 기준 상대 인덱스. 없으면 INDEX_NONE. */
	int32 HoverCard = INDEX_NONE;

	/** 현재 카드에서 누적된 드웰 시간(초). 카드를 벗어나면 0 으로 리셋. */
	float DwellTimer = 0.0f;

	/** 겨눔/드웰 상태를 비운다 (세션 재시작 등). */
	void Reset()
	{
		HoverCard = INDEX_NONE;
		DwellTimer = 0.0f;
	}

	/**
	 * 겨눔 판정 + 드웰 누적.
	 *
	 * @param Panel        카드를 그린 패널 (행 텍스트 위치로 겨눔을 판정한다).
	 * @param InFirstRow   카드 블록의 첫 행 인덱스.
	 * @param InCardCount  카드 수.
	 * @param AimOrigin    조준 광선의 시작점 (컨트롤러/배트 끝).
	 * @param AimDir       조준 방향 (월드).
	 * @param bTracked     추적 여부. 끊기면 겨눔 없음 + 진행 리셋.
	 * @return 이번 프레임에 확정된 카드의 상대 인덱스. 없으면 INDEX_NONE.
	 */
	int32 Update(const UVRInfoPanel* Panel, int32 InFirstRow, int32 InCardCount,
		const FVector& AimOrigin, const FVector& AimDir, bool bTracked, float DeltaSeconds);

	/** 현재 카드의 드웰 진행도 0..1. */
	float Progress() const
	{
		return (DwellSec > 0.0f) ? FMath::Clamp(DwellTimer / DwellSec, 0.0f, 1.0f) : 0.0f;
	}

	/** 카드 라벨 — 겨누는 중이면 '>' 표시와 진행바를 붙인다 (폰트 글리프 걱정 없는 ASCII). */
	FString Label(int32 Card, const TCHAR* Text) const;

	/** 카드 색 — 겨누는 중이면 진행도에 따라 주황 → 초록으로 차오른다. */
	FColor Color(int32 Card) const;
};
