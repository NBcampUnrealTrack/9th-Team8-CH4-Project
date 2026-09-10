#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "P48PlayerStartRegistrySubsystem.generated.h"

class AP48PlayerStart;

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
	void ReportSelectedLayout(int32 Revision, int32 SelectedCount);
	void RegisterPlayerStart(AP48PlayerStart* PlayerStart);
	void UnregisterPlayerStart(AP48PlayerStart* PlayerStart);
	void SealRegistration(int32 Revision);

	EP48PlayerStartLayoutState GetState(int32 Revision) const;
	int32 GetRegisteredCount(int32 Revision) const;

	FOnReadinessChanged OnReadinessChanged;

private:
	static constexpr float RegistrationGraceSeconds = 1.0f;

	int32 ActiveRevision = 0;
	int32 RequiredCount = 0;
	int32 SelectedCount = INDEX_NONE;
	bool bRegistrationSealed = false;
	EP48PlayerStartLayoutState LayoutState = EP48PlayerStartLayoutState::Pending;
	TSet<TWeakObjectPtr<AP48PlayerStart>> RegisteredPlayerStarts;
	FTimerHandle RegistrationTimeoutHandle;

	void EvaluateReadiness();
	void HandleRegistrationTimeout();
	void SetLayoutState(EP48PlayerStartLayoutState NewState);
	void PruneInvalidStarts();
};
