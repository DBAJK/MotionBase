#pragma once

#include "CoreMinimal.h"
#include "Core/Defense/Backup/BackupTypes.h"
#include "BaseballField.generated.h"

/**
 * 실측 규격 다이아몬드 좌표 + 백업 존 기하 계산 (계산 계층, UE 액터/렌더 비의존).
 *
 * ⚠️ 이 좌표는 `Content/Maps/BattingTest.umap` 에 배치된 실제 베이스 액터와 일치해야 한다
 *    (홈 원점, +X = 2루 방향, +Y = 1루 쪽). 레벨을 다시 배치하면 여기도 같이 맞출 것.
 *    액터를 이름으로 찾아 좌표를 읽어오지 않는 이유: `AActor::GetActorLabel()` 은
 *    **에디터 전용**이라 패키징 빌드에서 사라진다 — 좌표는 상수로 들고 다녀야 한다.
 *
 * `FScoringConfig` 와 같은 패턴이다: EditAnywhere 필드를 가진 순수 USTRUCT 로 두고,
 * 소유 폰이 멤버로 들고 있다가 UI 에서 실측값으로 캘리브레이션한다 (CLAUDE §규칙 —
 * 캘리브레이션 상수 하드코딩 금지).
 */
USTRUCT(BlueprintType)
struct FBaseballField
{
	GENERATED_BODY()

	// ── 베이스 (실측 다이아몬드 — 레벨과 반드시 일치) ──

	/** 베이스 간 거리 (cm). 90ft = 2743.2cm. */
	UPROPERTY(EditAnywhere, Category = "Field|Bases")
	float BasePathCm = 2743.2f;

	/** 지면 높이 (cm). 레벨 베이스 액터의 Z(≈5)와 맞춘다 — 디버그 드로우 기준일 뿐,
	 *  판정은 아래 모든 거리 계산이 Dist2D/세그먼트 거리라 Z 에 영향받지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Field|Bases")
	float GroundZ = 5.0f;

	// ── 내야 수비 위치 (포지션별 실측 깊이가 서로 달라 개별 좌표로 둔다) ──
	UPROPERTY(EditAnywhere, Category = "Field|Infield")
	FVector2D FirstBaseSpot = FVector2D(2600.0f, 1450.0f);

	UPROPERTY(EditAnywhere, Category = "Field|Infield")
	FVector2D SecondBaseSpot = FVector2D(3950.0f, 1550.0f);

	UPROPERTY(EditAnywhere, Category = "Field|Infield")
	FVector2D ShortSpot = FVector2D(3950.0f, -1550.0f);

	UPROPERTY(EditAnywhere, Category = "Field|Infield")
	FVector2D ThirdBaseSpot = FVector2D(2600.0f, -1450.0f);

	// ── 외야 수비 위치 (단일 깊이 손잡이 — 세 자리 모두 이 반지름 위에 대칭 배치) ──

	/**
	 * 외야 깊이 (cm, 홈 기준). 백업 이동 거리를 좌우하는 가장 중요한 노브다.
	 * 실측 깊이(82~88m)면 백업 이동이 10~63m 로 벌어져 내야는 항상 통과·외야는 항상 실패한다
	 * (제한 시간은 거리에서 파생되므로 자체는 괜찮지만, 체감 난이도가 포지션 추첨이 된다).
	 * 62m(6200) 정도면 10~44m 로 압축돼 플레이 가능한 범위가 된다. ⚠️ 실측 캘리브레이션 대상.
	 */
	UPROPERTY(EditAnywhere, Category = "Field|Outfield", meta = (ClampMin = "3000.0"))
	float OutfieldDepthCm = 6200.0f;

	/** 좌익수/우익수가 중견수(정면) 기준 좌우로 벌어지는 각도 (도). */
	UPROPERTY(EditAnywhere, Category = "Field|Outfield", meta = (ClampMin = "5.0", ClampMax = "44.0"))
	float OutfieldBearingDeg = 22.5f;

	/** 페어 지역 클램프에 쓰는 펜스 반지름 (cm) — 이보다 멀리는 존을 만들지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Field|Outfield", meta = (ClampMin = "3000.0"))
	float FenceRadiusCm = 9800.0f;

	// ── 백업 존 형태 상수 (⚠️ 전부 실측 캘리브레이션 대상 — 하드코딩 확정 아님) ──

	/** 베이스 뒤로 이만큼 물러난 자리가 BackUpBase 정답 중심. */
	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "100.0"))
	float BackupDistanceCm = 500.0f;

	/** BackUpBase 성공 반경. */
	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "50.0"))
	float BackupRadiusCm = 350.0f;

	/** CoverBase 성공 반경 (베이스 자체를 지키므로 더 좁게). */
	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "50.0"))
	float CoverRadiusCm = 200.0f;

	/** 중계(CutoffRelay) 라인의 유효 구간 — 던지는 사람↔베이스 사이 이 비율 구간만 정답. */
	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CutoffBandStart = 0.30f;
	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CutoffBandEnd = 0.55f;

	/** 중계 라인의 좌우 허용폭 (반폭, cm). */
	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "50.0"))
	float CutoffCorridorHalfWidthCm = 400.0f;

	/** 다른 야수 뒤로 물러나는 거리 (BackUpFielder). */
	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "100.0"))
	float BackUpFielderOffsetCm = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "50.0"))
	float BackUpFielderRadiusCm = 500.0f;

	/** Hold(제자리 사수) 성공 반경 — 안 움직였다고 인정할 여유. */
	UPROPERTY(EditAnywhere, Category = "Field|Zones", meta = (ClampMin = "50.0"))
	float HoldRadiusCm = 400.0f;

	// ── 타이밍 (⚠️ 전부 실측 캘리브레이션 대상) ──

	/** 이 시간(초) 안에 이동을 시작하면 판단 속도 만점. 실제 이동 개시는 0.4~0.9초대. */
	UPROPERTY(EditAnywhere, Category = "Field|Timing", meta = (ClampMin = "0.1"))
	float TargetDecisionSec = 0.9f;

	/** 백업 이동 속도 (cm/s). 클럽 레벨 야수 실주력(5.5~6.5 m/s) 대역. */
	UPROPERTY(EditAnywhere, Category = "Field|Timing", meta = (ClampMin = "200.0"))
	float MoveSpeedCms = 700.0f;

	/** 제한 시간 파생 시 이동 소요시간에 곱하는 여유 배율. */
	UPROPERTY(EditAnywhere, Category = "Field|Timing", meta = (ClampMin = "1.0"))
	float SlackFactor = 1.35f;

	/** 파생된 제한 시간의 하한 (초) — 아주 가까운 Hold/Cover 존까지 너무 촉박해지지 않게. */
	UPROPERTY(EditAnywhere, Category = "Field|Timing", meta = (ClampMin = "1.0"))
	float MinTimeLimitSec = 2.5f;

	// ── 조회 ──

	/** 베이스 → 월드 위치 (Z=GroundZ). */
	FVector GetBaseLocation(EBaseType Base) const
	{
		const float H = BasePathCm / FMath::Sqrt(2.0f);
		switch (Base)
		{
		case EBaseType::First:  return FVector(H, H, GroundZ);
		case EBaseType::Second: return FVector(H * 2.0f, 0.0f, GroundZ);
		case EBaseType::Third:  return FVector(H, -H, GroundZ);
		case EBaseType::Home:
		default:                return FVector(0.0f, 0.0f, GroundZ);
		}
	}

	/** 외야 수비 위치 — 중견수 기준 bearing(도)만큼 좌/우로 벌어진 자리. */
	FVector OutfieldSpot(float BearingDeg) const
	{
		const float Rad = FMath::DegreesToRadians(BearingDeg);
		return FVector(OutfieldDepthCm * FMath::Cos(Rad), OutfieldDepthCm * FMath::Sin(Rad), GroundZ);
	}

	/** 포지션 → 월드 수비 위치 (Z=GroundZ). */
	FVector GetFieldingSpot(EFieldPosition Pos) const
	{
		switch (Pos)
		{
		case EFieldPosition::First:  return FVector(FirstBaseSpot, GroundZ);
		case EFieldPosition::Second: return FVector(SecondBaseSpot, GroundZ);
		case EFieldPosition::Short:  return FVector(ShortSpot, GroundZ);
		case EFieldPosition::Third:  return FVector(ThirdBaseSpot, GroundZ);
		case EFieldPosition::Left:   return OutfieldSpot(-OutfieldBearingDeg);
		case EFieldPosition::Center: return OutfieldSpot(0.0f);
		case EFieldPosition::Right:  return OutfieldSpot(OutfieldBearingDeg);
		default:                     return FVector(0.0f, 0.0f, GroundZ);
		}
	}

	/**
	 * 규칙 + 플레이 → 실제 판정 가능한 존.
	 * BackUpBase/CutoffRelay 의 앵커는 Play(송구 목적지·발신자)에서, CoverBase/BackUpFielder 는
	 * Rule(실제로 지키는 대상)에서 가져온다 — 이 둘이 늘 같지는 않기 때문 (BackupTypes.h 참고).
	 */
	FBackupZone ResolveZone(const FBackupAssignmentRule& Rule, const FBackupPlay& Play) const
	{
		FBackupZone Z;
		Z.Role = Rule.Role;
		Z.Explain = Rule.Explain;
		Z.bKeyScenario = Rule.bKeyScenario;

		switch (Rule.Role)
		{
		case EBackupRole::BackUpBase:
		{
			// 어느 베이스를 받치나 — 보통은 송구 목적지, 규칙이 지정하면 그쪽(내야 기본 로테이션).
			const EBaseType TargetBase = Rule.bAnchorBaseOverride ? Rule.AnchorBase : Play.ThrowTo;
			const FVector Base = GetBaseLocation(TargetBase);

			// 어느 쪽 "뒤"인가 — 송구가 그 베이스로 갈 땐 송구 라인의 연장선, 송구와 무관한
			// 베이스를 받칠 땐(오버라이드) 홈 반대쪽 = 외야 쪽이 백업 자리다.
			const bool bFromHome = Rule.bAnchorBaseOverride || !Play.bHasThrowFrom;
			const FVector Origin = bFromHome ? GetBaseLocation(EBaseType::Home) : GetFieldingSpot(Play.ThrowFrom);
			const FVector Dir = (Base - Origin).GetSafeNormal2D();

			// ⚠️ 페어 지역 클램프를 쓰지 않는다. 1루·3루는 정확히 45° 파울선 위에 있어서,
			//    그 뒤로 물러난 자리는 **원래 파울 지역**이다 (1루 뒤 백업은 파울 라인 밖에 선다).
			//    클램프를 걸면 베이스 쪽으로 끌려와 BackupDistanceCm 이 500→371cm 로 줄어든다
			//    (BaseballFieldTest.ZoneGeometry 가 잡던 실패). 거리 상한만 지킨다.
			Z.Center = ClampToFieldRadius(Base + Dir * BackupDistanceCm);
			Z.RadiusCm = BackupRadiusCm;
			break;
		}
		case EBackupRole::CoverBase:
			Z.Center = GetBaseLocation(Rule.AnchorBase);
			Z.RadiusCm = CoverRadiusCm;
			break;
		case EBackupRole::CutoffRelay:
		{
			const FVector From = Play.bHasThrowFrom ? GetFieldingSpot(Play.ThrowFrom) : GetBaseLocation(EBaseType::Home);
			const FVector To = GetBaseLocation(Play.ThrowTo);
			Z.SegmentA = FMath::Lerp(From, To, CutoffBandStart);
			Z.SegmentB = FMath::Lerp(From, To, CutoffBandEnd);
			Z.RadiusCm = CutoffCorridorHalfWidthCm;
			break;
		}
		case EBackupRole::BackUpFielder:
		{
			const FVector Spot = GetFieldingSpot(Rule.AnchorFielder);
			const FVector Dir = (Spot - GetBaseLocation(EBaseType::Home)).GetSafeNormal2D();
			Z.Center = ClampToFairTerritory(Spot + Dir * BackUpFielderOffsetCm);
			Z.RadiusCm = BackUpFielderRadiusCm;
			break;
		}
		case EBackupRole::Hold:
		default:
			// Hold 는 "자기 포지션을 지킨다" — Rule.Position 이 곧 이 시행을 요청한 그 선수다.
			Z.Center = GetFieldingSpot(Rule.Position);
			Z.RadiusCm = HoldRadiusCm;
			break;
		}
		return Z;
	}

	/** 펜스 반경 안으로만 눌러 담는다 (파울 지역은 허용 — 베이스 뒤 백업 자리가 그쪽이다). */
	FVector ClampToFieldRadius(const FVector& Point) const
	{
		const FVector2D XY(Point.X, Point.Y);
		if (XY.Size() <= FenceRadiusCm)
		{
			return Point;
		}
		const FVector2D Capped = XY.GetSafeNormal() * FenceRadiusCm;
		return FVector(Capped.X, Capped.Y, Point.Z);
	}

	/** 페어 지역(양쪽 파울선 45° 안, 펜스 반경 이내)으로 좌표를 눌러 담는다. */
	FVector ClampToFairTerritory(const FVector& Point) const
	{
		const FVector2D XY(Point.X, Point.Y);
		const float Dist = FMath::Min(XY.Size(), FenceRadiusCm);
		const float BearingDeg = FMath::Clamp(
			FMath::RadiansToDegrees(FMath::Atan2(XY.Y, XY.X)), -45.0f, 45.0f);
		const float Rad = FMath::DegreesToRadians(BearingDeg);
		return FVector(FMath::Cos(Rad) * Dist, FMath::Sin(Rad) * Dist, Point.Z);
	}

	/** 도착 제한 시간을 이동 거리에서 파생한다 — 내야/외야가 같은 상수를 쓰면 안 되는 이유는
	 *  설계 노트(계획 문서) 참고: 6배 차이 나는 거리에 상수 시간을 쓰면 판단 측정이 아니라
	 *  포지션 추첨이 된다. */
	float DeriveTimeLimit(float PathDistanceCm) const
	{
		return FMath::Max(
			TargetDecisionSec + (PathDistanceCm / FMath::Max(MoveSpeedCms, 1.0f)) * SlackFactor,
			MinTimeLimitSec);
	}
};
