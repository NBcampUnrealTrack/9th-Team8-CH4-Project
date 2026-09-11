#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "P48PlayerStartRegistrySubsystem.generated.h"

class AP48PlayerStart;

/** Server-owned selection, independent of Spawn Actor property override settings. */
struct FP48SelectedPlayerStart
{
	FVector Location = FVector::ZeroVector;
	int32 IslandIndex = INDEX_NONE;
};

UENUM()
enum class EP48PlayerStartLayoutState : uint8
{
	Pending,
	Ready,
	Failed
};

/** PCG가 선택한 PlayerStart 레이아웃과 실제 Spawn Actor 결과를 서버에서 검증합니다. */
UCLASS()
class PROJECT48_API UP48PlayerStartRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnReadinessChanged, int32 /* Revision */);

	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override
	{
		return Type == EWorldType::Game || Type == EWorldType::PIE;
	}

	void BeginGeneration(int32 Revision, int32 RequiredCount);
	void UpdateRequiredCount(int32 Revision, int32 RequiredCount);
	void ReportSelectedLayout(int32 Revision, int32 SelectedCount);
	void ReportSelectedStarts(int32 Revision, const TArray<FP48SelectedPlayerStart>& Starts);
	/** 현재 PCG 선택 결과가 보고된 뒤 생성된 PlayerStart만 등록합니다. */
	bool RegisterPlayerStart(AP48PlayerStart* PlayerStart);
	void UnregisterPlayerStart(AP48PlayerStart* PlayerStart);
	void SealRegistration(int32 Revision);

	EP48PlayerStartLayoutState GetState(int32 Revision) const;
	int32 GetRegisteredCount(int32 Revision) const;
	/** 현재 세대에서 준비가 끝난 PlayerStart를 SpawnSlotIndex 순으로 반환합니다. */
	bool GetReadyPlayerStarts(int32 Revision, TArray<AP48PlayerStart*>& OutPlayerStarts) const;

	FOnReadinessChanged OnReadinessChanged;

private:
	static constexpr float RegistrationGraceSeconds = 1.0f;

	int32 ActiveRevision = 0;
	int32 RequiredCount = 0;
	int32 SelectedCount = INDEX_NONE;
	bool bRegistrationSealed = false;
	EP48PlayerStartLayoutState LayoutState = EP48PlayerStartLayoutState::Pending;
	TSet<TWeakObjectPtr<AP48PlayerStart>> RegisteredPlayerStarts;
	TArray<FP48SelectedPlayerStart> SelectedStarts;
	FTimerHandle RegistrationTimeoutHandle;

	void EvaluateReadiness();
	void HandleRegistrationTimeout();
	void SetLayoutState(EP48PlayerStartLayoutState NewState);
	void PruneInvalidStarts();
};
