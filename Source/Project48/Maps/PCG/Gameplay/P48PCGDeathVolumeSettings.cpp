#include "P48PCGDeathVolumeSettings.h"

#include "../Common/P48PCGSeedHelpers.h"
#include "../../Objects/Gameplay/P48DeathFloor.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Engine/World.h"
#include "Helpers/PCGHelpers.h"
#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "P48PCGDeathVolumeSettings"

#if WITH_EDITOR
FText UP48PCGDeathVolumeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "P48 Death Volume Point");
}

FText UP48PCGDeathVolumeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Creates one Spawn Actor point below the input spatial bounds for player death overlap detection.");
}
#endif

TArray<FPCGPinProperties> UP48PCGDeathVolumeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	Pins.Emplace(P48PCGSeedNames::InputPin, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UP48PCGDeathVolumeSettings::OutputPinProperties() const
{
	return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UP48PCGDeathVolumeSettings::CreateElement() const
{
	return MakeShared<FP48PCGDeathVolumeElement>();
}

bool FP48PCGDeathVolumeElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const UP48PCGDeathVolumeSettings* Settings = Context->GetInputSettings<UP48PCGDeathVolumeSettings>();
	if (!Settings)
	{
		return true;
	}

	UWorld* World = Context->ExecutionSource.IsValid()
		? Context->ExecutionSource->GetExecutionState().GetWorld()
		: nullptr;

	// 게임 클라이언트에는 판정 볼륨을 만들지 않습니다. 서버가 캐릭터 이동과 Overlap을 판정합니다.
	if (World && World->IsGameWorld() && World->GetNetMode() == NM_Client)
	{
		return true;
	}

	FBox CombinedBounds(ForceInit);
	TSet<FString> CombinedTags;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
		if (!SpatialData)
		{
			continue;
		}

		const FBox InputBounds = SpatialData->GetBounds();
		if (InputBounds.IsValid)
		{
			CombinedBounds += InputBounds;
			CombinedTags.Append(Input.Tags);
		}
	}

	if (!CombinedBounds.IsValid)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidBounds", "P48 Death Volume Point requires valid spatial bounds."));
		return true;
	}

	const FP48DeathVolumeGenerationSettings& Rules = Settings->GenerationSettings;
	const float CoverageScale = FMath::Max(1.0f, Rules.CoverageScale);
	const float Padding = FMath::Max(0.0f, Rules.HorizontalPadding);
	const float VolumeDepth = FMath::Max(1.0f, Rules.Depth);
	const FVector2D MinimumHorizontalSize = Rules.MinimumHorizontalSize.GetAbs();
	const FVector BoundsSize = CombinedBounds.GetSize();
	const FVector VolumeSize(
		FMath::Max(MinimumHorizontalSize.X, BoundsSize.X * CoverageScale + Padding * 2.0f),
		FMath::Max(MinimumHorizontalSize.Y, BoundsSize.Y * CoverageScale + Padding * 2.0f),
		VolumeDepth);
	const FVector VolumeCenter(
		CombinedBounds.GetCenter().X,
		CombinedBounds.GetCenter().Y,
		CombinedBounds.Min.Z - VolumeDepth * 0.5f);
	const FVector BaseBoxSize = AP48DeathFloor::DefaultCollisionExtent * 2.0f;

	FPCGPoint Point;
	Point.Transform = FTransform(
		FQuat::Identity,
		VolumeCenter,
		VolumeSize / BaseBoxSize);
	Point.Density = 1.0f;
	Point.BoundsMin = -AP48DeathFloor::DefaultCollisionExtent;
	Point.BoundsMax = AP48DeathFloor::DefaultCollisionExtent;
	Point.Steepness = 1.0f;
	Point.Seed = PCGHelpers::ComputeSeed(
		P48ReadNetworkSeed(Context),
		HashCombine(GetTypeHash(VolumeCenter), GetTypeHash(VolumeSize)));

	UPCGBasePointData* Output = FPCGContext::NewPointData_AnyThread(Context);
	Output->SetNumPoints(1, false);
	Output->AllocateProperties(EPCGPointNativeProperties::All);
	FPCGPointValueRanges Ranges(Output, false);
	Ranges.SetFromPoint(0, Point);

	FPCGTaggedData& TaggedOutput = Context->OutputData.TaggedData.Emplace_GetRef();
	TaggedOutput.Data = Output;
	TaggedOutput.Pin = PCGPinConstants::DefaultOutputLabel;
	TaggedOutput.Tags = MoveTemp(CombinedTags);

	if (Settings->bLogDiagnostics)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[P48DeathVolume] Center=%s Size=%s InputBounds=%s"),
			*VolumeCenter.ToCompactString(), *VolumeSize.ToCompactString(), *CombinedBounds.ToString());
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
