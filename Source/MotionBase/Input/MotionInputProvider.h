#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/MotionBaseTypes.h"
#include "Data/SwingSample.h"
#include "Data/BodyPose.h"
#include "MotionInputProvider.generated.h"

/**
 * 모션 입력 provider 베이스. (입력 추상화)
 *
 * 게임 로직은 이 인터페이스만 알고 실제 소스는 모른다.
 * → PC + Mock 으로 로직을 완성하고, 나중에 provider 만 교체하면 로직은 무수정.
 *
 * 구현체:
 *   - UMockMotionInputProvider  : PC 개발/테스트 (좌표 시퀀스 재생)
 *   - UViveMotionInputProvider  : HTC Vive 컨트롤러 (예정)
 *   - ULiDARMotionInputProvider : 뉴작 LiDAR (데이터 포맷 확인 후)
 */
UCLASS(Abstract, BlueprintType)
class MOTIONBASE_API UMotionInputProvider : public UObject
{
	GENERATED_BODY()

public:
	/** 세션 시작 시 1회. 트래킹 초기화·기준점 설정. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input")
	virtual bool Initialize() { return false; }

	/** 매 프레임. 내부 버퍼 갱신. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input")
	virtual void Tick(float DeltaSeconds) {}

	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input")
	virtual void Shutdown() {}

	UFUNCTION(BlueprintPure, Category = "MotionBase|Input")
	virtual EInputSource GetSourceType() const { return EInputSource::Mock; }

	/** 지금 유효한 데이터를 내고 있는지. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Input")
	virtual bool IsTracking() const { return false; }

	/**
	 * Initialize() 이후 경과 시간(초). 샘플의 TimeSeconds 와 같은 시간축.
	 * 스윙 타이밍 판정은 이 시간축 기준으로 비교해야 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Input")
	virtual float GetElapsedSeconds() const { return 0.0f; }

	/** 히스토리 링버퍼 크기 조정. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input")
	void SetHistoryCapacity(int32 InCapacity) { HistoryCapacity = FMath::Max(2, InCapacity); }

	// ── 타격 (Vive 컨트롤러 / Mock) ──

	/** 배트 끝(BatTip) 최신 샘플. */
	virtual bool GetBatTipSample(FSwingSample& OutSample) const { return false; }

	/** 최근 N개 배트 끝 샘플 (컨택 판정·궤적 분석 입력). */
	virtual bool GetBatTipHistory(TArray<FSwingSample>& OutSamples) const { return false; }

	// ── 전신 (LiDAR / 트래커 / Mock) ──

	virtual bool GetBodyPose(FBodyPoseSample& OutPose) const { return false; }

	virtual bool GetBodyPoseHistory(TArray<FBodyPoseSample>& OutPoses) const { return false; }

	/** 신체 인식(BodyScan) 결과 — 개인별 난이도 기준선. */
	virtual bool GetBodyScan(float& OutHeightCm, float& OutReachRadiusCm) const { return false; }

protected:
	/** 히스토리 버퍼 크기 (기본 20샘플 ≒ 90Hz 기준 약 0.22초). */
	UPROPERTY(EditAnywhere, Category = "MotionBase|Input")
	int32 HistoryCapacity = 20;

	/** 링버퍼 push 헬퍼. */
	template<typename T>
	static void PushRing(TArray<T>& Buffer, const T& Item, int32 Capacity)
	{
		Buffer.Add(Item);
		while (Buffer.Num() > Capacity)
		{
			Buffer.RemoveAt(0, 1, EAllowShrinking::No);
		}
	}
};


/**
 * Mock provider — Vive/LiDAR 없이 PC에서 로직·점수·UI를 개발/검증.
 *
 * 재생은 **샘플의 TimeSec 기준**이라 프레임레이트와 무관하다
 * (90Hz 녹화를 60fps 에서 재생해도 시간축이 어긋나지 않음).
 * 단위 테스트는 Tick 없이 PlayAll() 로 즉시 전체 주입하는 것을 권장.
 */
UCLASS(BlueprintType)
class MOTIONBASE_API UMockMotionInputProvider : public UMotionInputProvider
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Shutdown() override;

	virtual EInputSource GetSourceType() const override { return EInputSource::Mock; }
	virtual bool IsTracking() const override { return bInitialized; }
	virtual float GetElapsedSeconds() const override { return ElapsedSec; }

	virtual bool GetBatTipSample(FSwingSample& OutSample) const override;
	virtual bool GetBatTipHistory(TArray<FSwingSample>& OutSamples) const override;
	virtual bool GetBodyPose(FBodyPoseSample& OutPose) const override;
	virtual bool GetBodyPoseHistory(TArray<FBodyPoseSample>& OutPoses) const override;
	virtual bool GetBodyScan(float& OutHeightCm, float& OutReachRadiusCm) const override;

	/** 녹화된 배트 궤적 주입. 주입 즉시 t<=0 샘플이 히스토리에 반영된다. */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input|Mock")
	void FeedBatTipSequence(const TArray<FSwingSample>& InSamples);

	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input|Mock")
	void FeedBodyPoseSequence(const TArray<FBodyPoseSample>& InPoses);

	/** 시퀀스 전체를 즉시 히스토리에 밀어넣는다 (단위 테스트용 — Tick 불필요). */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input|Mock")
	void PlayAll();

	/** 재생 위치·시간 초기화 (시퀀스는 유지). */
	UFUNCTION(BlueprintCallable, Category = "MotionBase|Input|Mock")
	void ResetPlayback();

	/** 두 시퀀스 모두 끝까지 재생됐는지. */
	UFUNCTION(BlueprintPure, Category = "MotionBase|Input|Mock")
	bool IsPlaybackFinished() const;

	/** false면 재생하지 않고 외부에서 Feed* 로 직접 주입. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Input|Mock")
	bool bPlaybackMode = true;

	/** Mock 신체 기준값. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Input|Mock")
	float MockHeightCm = 175.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionBase|Input|Mock")
	float MockReachRadiusCm = 80.0f;

private:
	/** ElapsedSec 까지 도달한 샘플을 히스토리로 옮긴다. */
	void AdvancePlayback();

	bool bInitialized = false;
	float ElapsedSec = 0.0f;

	// 시퀀스 길이가 다를 수 있으므로 인덱스를 분리한다.
	int32 BatTipIndex = 0;
	int32 BodyPoseIndex = 0;

	TArray<FSwingSample> BatTipSequence;
	TArray<FBodyPoseSample> BodyPoseSequence;

	TArray<FSwingSample> BatTipHistory;
	TArray<FBodyPoseSample> BodyPoseHistory;
};
