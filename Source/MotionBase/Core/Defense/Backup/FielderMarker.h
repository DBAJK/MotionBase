#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/MotionBaseTypes.h"
#include "FielderMarker.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;

/** 유니폼 색 그룹 — 부위마다 어느 색을 쓰는지. */
UENUM()
enum class EFielderPart : uint8
{
	Jersey,   // 상의·소매·어깨
	Accent,   // 모자·벨트·스타킹 (팀 컬러)
	Pants,    // 니커즈(무릎까지 오는 바지)·골반
	Skin,     // 머리·목·맨팔
	Glove,
	Shoe
};

/**
 * 수비 위치에 서 있는 **동료 수비수** — 야구 유니폼을 입은 사람.
 *
 * 왜 필요한가: 백업 판단은 "나 말고 누가 어디 있는가"를 보고 내리는 것이다. 예전에는
 * 정답 zone 을 초록 원으로 띄워 줘서 판단할 필요 없이 초록만 따라가면 됐다. 정답 강조를
 * 걷어내는 대신, 실제로 둘러보면 동료가 보이도록 이 마커를 세운다. HUD 미니맵은
 * **헤드셋 안에 렌더되지 않으므로**(UI/VRInfoPanel.h 참고) VR 에서 판단 근거가 되는 건
 * 결국 이 마커와 날아가는 타구뿐이다.
 *
 * ⚠️ **에셋 비의존** — 엔진 기본 도형(/Engine/BasicShapes 의 Cylinder·Sphere)만 조립한다.
 *    UE 5.8 엔진 컨텐츠에는 인체 스켈레탈 메시가 없어서(Engine/Content 에 Characters
 *    폴더 자체가 없음), 에디터 작업 없이 바로 보이게 하려면 이 방법뿐이다. 나중에 선수
 *    메시를 구하면 이 액터만 통째로 갈아끼우면 된다.
 *
 * **자세**: 야수 준비 자세 — 무릎 굽히고 상체를 홈 쪽(+X)으로 살짝 숙이고 글러브를 앞에.
 *   뼈대를 **관절 좌표로 정의**하고(아래 cm 표) 각 부위를 두 관절 사이에 놓는다. 회전은
 *   FRotationMatrix::MakeFromZ 로 방향에서 직접 뽑는다 — 회전자 부호를 눈으로 확인할 수
 *   없는 환경이라 각도를 손으로 적으면 관절이 꺾인 것처럼 보일 위험이 크기 때문.
 *   이음매가 각져 보이지 않게 어깨·팔꿈치·무릎엔 관절 구를 덮는다.
 *
 * **유니폼**: 모자(챙) · 반팔 저지 · 벨트 · 니커즈 · 스타킹 · 스파이크 · 글러브 ·
 *   등번호(야구 스코어링 번호 3~9).
 *
 * ⚠️ 충돌 없음 — 플레이어가 백업 존으로 뛰어갈 때 동료에게 막히면 안 된다.
 * ⚠️ 이름표 방향은 스스로 돌지 않는다. 소유 폰(ABackupPawn)이 매 프레임 플레이어
 *    위치를 넘겨 FaceLabelTowards 를 호출한다 — 마커 7개가 각자 Tick 하지 않게.
 */
UCLASS()
class MOTIONBASE_API AFielderMarker : public AActor
{
	GENERATED_BODY()

public:
	AFielderMarker();

	/**
	 * 마커의 정체를 정한다. SpawnActor 직후에 호출.
	 * @param InPosition   이 마커가 서 있는 수비 포지션
	 * @param InLabel      머리 위 이름표 ("2B", "SS" …)
	 * @param InBackNumber 등번호 (야구 스코어링 번호). 0 이면 숨긴다.
	 * @param bIsSelfSpot  플레이어 본인의 수비 위치인가 — 몸 전체를 숨기고 이름표만 남긴다.
	 *                     본인 자리에 사람을 세우면 VR 에서 자기 머릿속에 박혀 시야를 가린다.
	 */
	void Configure(EFieldPosition InPosition, const FString& InLabel, int32 InBackNumber, bool bIsSelfSpot);

	/** 이름표가 뷰어(플레이어)를 향하도록 yaw 만 돌린다. 기울지 않도록 pitch/roll 은 0 고정. */
	void FaceLabelTowards(const FVector& ViewerLocation);

	EFieldPosition GetPosition() const { return Position; }

	/** 본인 자리 이름표 색 (앰버) — HUD 미니맵의 MapPosSelf 와 톤을 맞춘다. */
	static const FLinearColor SelfTint;
	/** 동료 이름표 색. */
	static const FLinearColor MateTint;

protected:
	virtual void BeginPlay() override;

	// ── 유니폼 색 (BP 서브클래스로 팀 컬러를 바꿀 수 있게 노출) ──
	// ABackupPawn 의 FielderMarkerClass 에 서브클래스를 물리면 코드 수정 없이 바뀐다.

	UPROPERTY(EditAnywhere, Category = "Fielder|Uniform")
	FLinearColor JerseyColor = FLinearColor(0.88f, 0.88f, 0.84f, 1.0f);

	/** 모자·벨트·스타킹 — 팀 강조색. */
	UPROPERTY(EditAnywhere, Category = "Fielder|Uniform")
	FLinearColor AccentColor = FLinearColor(0.06f, 0.11f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Fielder|Uniform")
	FLinearColor PantsColor = FLinearColor(0.70f, 0.71f, 0.74f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Fielder|Uniform")
	FLinearColor SkinColor = FLinearColor(0.72f, 0.51f, 0.36f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Fielder|Uniform")
	FLinearColor GloveColor = FLinearColor(0.26f, 0.13f, 0.06f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Fielder|Uniform")
	FLinearColor ShoeColor = FLinearColor(0.05f, 0.05f, 0.06f, 1.0f);

	UPROPERTY(VisibleAnywhere, Category = "Fielder")
	TObjectPtr<USceneComponent> Root;

	/** 등에 붙는 번호 — 뒤에서 봐도 누구인지 읽히게. */
	UPROPERTY(VisibleAnywhere, Category = "Fielder")
	TObjectPtr<UTextRenderComponent> BackNumber;

	/** 머리 위에 떠서 항상 플레이어를 향하는 포지션 이름표. */
	UPROPERTY(VisibleAnywhere, Category = "Fielder")
	TObjectPtr<UTextRenderComponent> Label;

private:
	// ── 생성자 전용 조립 헬퍼 ──

	/** 두 관절 A→B 를 잇는 원기둥. 길이·방향을 좌표에서 계산한다. */
	void MakeBone(const TCHAR* Name, UStaticMesh* Mesh, EFielderPart Group,
		float DiameterCm, const FVector& A, const FVector& B);

	/** 한 점에 놓는 타원체(관절·머리·모자·글러브 등). RadiiCm 은 반지름. */
	void MakeBlob(const TCHAR* Name, UStaticMesh* Mesh, EFielderPart Group,
		const FVector& RadiiCm, const FVector& CenterCm);

	/** 공통 설정(부착·충돌 끄기·목록 등록). */
	UStaticMeshComponent* AddPart(const TCHAR* Name, UStaticMesh* Mesh, EFielderPart Group);

	/** 색 그룹 → 실제 색. */
	FLinearColor ColorFor(EFielderPart Group) const;

	/** 몸 전체(머리 위 이름표 제외) 표시/숨김. */
	void SetBodyVisible(bool bVisible);

	/** 조립된 부위들. PartGroups 와 인덱스가 1:1 대응한다. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	UPROPERTY()
	TArray<uint8> PartGroups;

	/** 엔진 기본 도형용 머티리얼 — 색 그룹마다 MID 를 하나씩만 만들어 공유한다. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	EFieldPosition Position = EFieldPosition::Second;
};
