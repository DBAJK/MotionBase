#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FielderCrowd.generated.h"

class AFielderMarker;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * 동료 수비수들의 **몸을 한꺼번에 그리는 인스턴싱 렌더러.**
 *
 * 왜 필요한가 — 성능. AFielderMarker 는 몸을 엔진 기본 도형 31개(팔·다리·몸통·모자…)로
 * 조립하고, 필드에는 본인을 뺀 6명이 선다. 부위마다 별개 UStaticMeshComponent 라
 * **186 드로우콜**이 되고, VR 은 스테레오라 실질 372 다. 게다가 6명이 내야~외야에 흩어져
 * 있어 홈 쪽을 보면 전부 시야에 들어와 컬링도 안 먹는다. 송구 모드엔 없는 부하라
 * "백업 모드만 렉이 심하다"로 나타났다.
 *
 * 해결 — 같은 (메시 × 색 그룹) 부위를 하나의 인스턴싱 컴포넌트로 모은다. 도형은 원기둥·구
 * 두 종류, 색 그룹은 6종이므로 **최대 12 드로우콜**로 필드 전체가 그려진다.
 *
 * ⚠️ 몸의 **모양은 여전히 AFielderMarker 가 정의한다.** 여기서 좌표를 다시 적지 않고
 *    마커가 만들어 둔 부위 컴포넌트의 트랜스폼을 그대로 읽어 인스턴스로 옮긴 뒤, 원본
 *    컴포넌트를 숨긴다. 자세를 고칠 일이 생겨도 손댈 곳은 마커 한 곳뿐이다.
 */
UCLASS()
class MOTIONBASE_API AFielderCrowd : public AActor
{
	GENERATED_BODY()

public:
	AFielderCrowd();

	/**
	 * 마커들의 몸을 인스턴스로 흡수하고 원본 부위 컴포넌트를 끈다.
	 * 이름표(TextRender)는 건드리지 않는다 — 마커마다 뷰어 쪽으로 따로 돌려야 한다.
	 */
	void Rebuild(const TArray<TObjectPtr<AFielderMarker>>& Markers);

	/** 인스턴스를 모두 비운다. */
	void Clear();

protected:
	/**
	 * (메시 × 색 그룹) 조합마다 인스턴싱 컴포넌트를 하나씩 만든다(있으면 재사용).
	 * 색은 그룹당 MID 하나를 만들어 붙인다 — 인스턴스별 색이 필요 없으므로 이걸로 충분하다.
	 */
	UInstancedStaticMeshComponent* GetOrCreateBatch(UStaticMesh* Mesh, uint8 Group,
		const FLinearColor& Color, UMaterialInterface* BaseMaterial);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	/** 키 = (메시, 색 그룹) 해시. */
	UPROPERTY()
	TMap<uint32, TObjectPtr<UInstancedStaticMeshComponent>> Batches;
};
