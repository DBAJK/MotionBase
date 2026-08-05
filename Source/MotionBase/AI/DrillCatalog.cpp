#include "AI/DrillCatalog.h"

namespace
{
	FTrainingDrill MakeDrill(EWeaknessAxis Axis, const TCHAR* Name, const TCHAR* Desc, const TCHAR* Cue)
	{
		FTrainingDrill D;
		D.TargetAxis = Axis;
		D.Name = Name;
		D.Description = Desc;
		D.FocusCue = Cue;
		return D;
	}
}

TArray<FTrainingDrill> UDrillCatalog::DrillsForAxis(EWeaknessAxis Axis)
{
	switch (Axis)
	{
	case EWeaknessAxis::ContactRate:
		return {
			MakeDrill(EWeaknessAxis::ContactRate, TEXT("트래킹 드릴"),
				TEXT("공을 릴리스부터 임팩트까지 눈으로 끝까지 좇으며 10구 지켜보기(스윙 없이)."),
				TEXT("공을 오래 본다")),
			MakeDrill(EWeaknessAxis::ContactRate, TEXT("소프트 토스 컨택"),
				TEXT("가까운 거리에서 느리게 토스된 공을 맞히는 데만 집중, 20구."),
				TEXT("맞히기 우선")),
		};

	case EWeaknessAxis::Timing:
		return {
			MakeDrill(EWeaknessAxis::Timing, TEXT("리듬 스텝 드릴"),
				TEXT("투수 릴리스에 맞춰 앞발 스텝 타이밍을 일정하게 반복, 15스윙."),
				TEXT("릴리스=스텝")),
			MakeDrill(EWeaknessAxis::Timing, TEXT("구속 변화 소프트토스"),
				TEXT("느린 공과 빠른 공을 섞어 토스, 타이밍을 공에 맞춰 조절."),
				TEXT("공에 맞춰 기다린다")),
		};

	case EWeaknessAxis::ContactAccuracy:
		return {
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("티 배팅 정밀 컨택"),
				TEXT("고정 티의 공 중심을 배트 스위트스팟으로 정확히 맞히기, 20스윙."),
				TEXT("배트 중심")),
			MakeDrill(EWeaknessAxis::ContactAccuracy, TEXT("존 구분 소프트토스"),
				TEXT("높낮이·좌우 코스를 나눠 토스, 각 코스에서 정확히 컨택."),
				TEXT("코스별 조준")),
		};

	case EWeaknessAxis::BatSpeed:
		return {
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("회전 파워 스로우"),
				TEXT("메디신볼을 타격 방향으로 힘껏 던져 하체·코어 회전력 강화, 10회 3세트."),
				TEXT("하체부터 회전")),
			MakeDrill(EWeaknessAxis::BatSpeed, TEXT("저항밴드 스윙"),
				TEXT("밴드 저항을 걸고 스윙 궤도를 유지하며 가속, 12스윙 3세트."),
				TEXT("임팩트에서 가속")),
		};

	case EWeaknessAxis::Consistency:
		return {
			MakeDrill(EWeaknessAxis::Consistency, TEXT("루틴 고정 반복"),
				TEXT("셋업→스텝→스윙 루틴을 동일하게 10스윙 반복, 매 스윙 감각 체크."),
				TEXT("같은 동작 반복")),
			MakeDrill(EWeaknessAxis::Consistency, TEXT("체크포인트 스윙"),
				TEXT("느린 스윙으로 자세 체크포인트(그립·팔꿈치·회전)를 매번 확인."),
				TEXT("자세 점검")),
		};

	default:
		return {};
	}
}

TArray<FTrainingDrill> UDrillCatalog::Recommend(const FWeaknessReport& Report, int32 MaxDrills)
{
	TArray<FTrainingDrill> Out;
	MaxDrills = FMath::Max(MaxDrills, 1);

	if (!Report.bValid)
	{
		return Out;
	}

	// 약점이 없으면 유지용 기본 드릴.
	if (Report.Weaknesses.Num() == 0)
	{
		Out.Add(MakeDrill(EWeaknessAxis::Consistency, TEXT("경기 감각 유지"),
			TEXT("실전 페이스로 라이브 배팅을 이어가며 현재 밸런스를 유지."),
			TEXT("현재 감각 유지")));
		return Out;
	}

	// 시급한 약점부터(리포트가 이미 정렬됨) 드릴을 뽑되, 이름 중복은 건너뛴다.
	TSet<FString> Seen;
	for (const FWeakness& W : Report.Weaknesses)
	{
		for (const FTrainingDrill& D : DrillsForAxis(W.Axis))
		{
			if (Out.Num() >= MaxDrills)
			{
				return Out;
			}
			if (!Seen.Contains(D.Name))
			{
				Seen.Add(D.Name);
				Out.Add(D);
			}
		}
	}
	return Out;
}
