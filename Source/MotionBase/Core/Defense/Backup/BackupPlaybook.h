#pragma once

#include "CoreMinimal.h"
#include "Core/Defense/Backup/BackupTypes.h"
#include "Core/Defense/Backup/BaseballField.h"
#include "BackupPlaybook.generated.h"

/**
 * 백업 케이스 → 정답 규칙 카탈로그 (희소 2테이블 + 리졸버).
 *
 * 왜 이 모양인가: 시나리오마다 "포지션 → 정답 zone" 맵을 손으로 채우면 시나리오 하나에
 * 7칸씩 저작해야 하고, `(포지션 × 타구존 × 주자상황)` 매트릭스로 짜면 **주자 상황이
 * 배치를 정하지 않고 송구 목적지를 정하며 송구 목적지가 배치를 정한다**는 사실을 무시해
 * 테이블이 3배로 불어난다. 그래서 여기선:
 *   Table A(FBackupPlay)            — 포지션과 무관한 "경기 상황" 자체.
 *   Table B(FBackupAssignmentRule)  — 한 포지션이 그 상황을 만났을 때의 행동.
 * 이렇게 나누고 `Resolve()` 로 묶는다. 규칙이 없는 (포지션,플레이) 조합은 자동으로
 * Hold(제자리 사수)로 떨어진다 — "이 공은 내 담당이 아니다"가 기본값이라 126칸을
 * 다 채울 필요가 없다.
 *
 * ⚠️ 이 카탈로그는 **규칙표다 — LLM 이 만들지 않는다.** 잘못된 백업 규칙은 잘못된
 *    야구를 가르친다 (DrillCatalog 의 "LLM 은 운동을 자유 생성하지 않는다"와 같은 이유).
 */
UCLASS()
class MOTIONBASE_API UBackupPlaybook : public UObject
{
	GENERATED_BODY()

public:
	/** 전체 플레이(Table A). 포지션 무관 — 매 호출 같은 목록을 돌려준다(불변 데이터). */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Backup")
	static TArray<FBackupPlay> BuildPlayTable();

	/** 전체 규칙(Table B). */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Backup")
	static TArray<FBackupAssignmentRule> BuildRuleTable();

	/**
	 * 플레이 + 포지션 → 정답 규칙. 필터를 더 많이 만족하는(=더 구체적인) 규칙이 우선한다.
	 * 매칭되는 규칙이 없으면 Hold 기본 규칙(일반 설명 포함)을 돌려준다 — 절대 실패하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Backup")
	static FBackupAssignmentRule Resolve(const TArray<FBackupAssignmentRule>& RuleTable,
		const FBackupPlay& Play, EFieldPosition Position);

	/**
	 * 이 포지션이 이 플레이에서 Hold 가 아닌(=실제로 움직여야 하는) 정답을 받는지.
	 * 시행 추첨(플레이어 폰)이 "이 포지션에 의미 있는 플레이"를 고를 때 쓴다.
	 */
	static bool IsEligibleForPosition(const TArray<FBackupAssignmentRule>& RuleTable,
		const FBackupPlay& Play, EFieldPosition Position);

	/**
	 * 이번 시행을 완전히 조립한다 — 정답 존 + 방향 판단용 후보 존 + 제한 시간.
	 *
	 * 후보 집합은 **"이 포지션이 플레이북 전체에서 배정받을 수 있는 존들"** 로 한정한다
	 * (이 플레이가 다른 포지션에게 주는 존까지 넣지 않는다) — 그래야 후보 간 각도差가
	 * 충분히 벌어진다. 각 후보는 이 Position 이 플레이북 어디선가 받을 수 있는
	 * (역할, 앵커) 조합을 **이번 플레이의 실제 지리**(투수/송구 방향 등)로 다시 해석해
	 * 계산한다 — 그래야 "3루 뒤 백업"이 매번 실제 송구 방향에 맞는 좌표로 나온다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Backup")
	static FBackupTrial BuildTrial(const FBaseballField& Field, const TArray<FBackupAssignmentRule>& RuleTable,
		EFieldPosition Position, const FBackupPlay& Play);

	/**
	 * 저작 데이터 검증 — 포지션마다 Hold 아닌 플레이가 최소 MinNonHoldPerPosition 개 있는지.
	 * 실패하면 그 포지션 이름을 OutErrors 에 담는다. 자동화 테스트가 이걸 불러
	 * "커버리지 미달이 데모가 아니라 빌드에서 터지게" 한다.
	 */
	static bool ValidateCoverage(const TArray<FBackupPlay>& PlayTable, const TArray<FBackupAssignmentRule>& RuleTable,
		int32 MinNonHoldPerPosition, TArray<FString>& OutErrors);

	/** 전체 7개 포지션 목록 (반복용). */
	static const TArray<EFieldPosition>& AllPositions();

	/** 영문 표시 이름 (3D 텍스트는 한글 폰트가 없어 영어로 표기). */
	static FString PositionName(EFieldPosition Pos);
};
