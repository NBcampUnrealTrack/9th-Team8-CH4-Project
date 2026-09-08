#include "P48PCGDeathVolumeSettings.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Engine/World.h"
#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "P48PCGDeathVolumeSettings"

namespace P48DeathVolume
{
	// AP48DeathFloor의 기본 Box 전체 크기와 일치합니다.
	constexpr float BaseBoxSize = 100.0f;
	constexpr float BaseBoxExtent = BaseBoxSize * 0.5f;
}

#if WITH_EDITOR
FText UP48PCGDeathVolumeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "P48 Death Volume Point");
}

FText UP48PCGDeathVolumeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Creates one Spawn Actor point sized to the input spatial XY bounds, with its top placed at the configured world Z height.");
}
#endif

TArray<FPCGPinProperties> UP48PCGDeathVolumeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
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
	const FVector BoundsSize = CombinedBounds.GetSize();
	const FVector VolumeSize(
		FMath::Max(1.0f, BoundsSize.X * CoverageScale + Padding * 2.0f),
		FMath::Max(1.0f, BoundsSize.Y * CoverageScale + Padding * 2.0f),
		VolumeDepth);
	const FVector VolumeCenter(
		CombinedBounds.GetCenter().X,
		CombinedBounds.GetCenter().Y,
		Rules.DeathZ - VolumeDepth * 0.5f);

	FPCGPoint Point;
	Point.Transform = FTransform(
		FQuat::Identity,
		VolumeCenter,
		VolumeSize / P48DeathVolume::BaseBoxSize);
	Point.Density = 1.0f;
	Point.BoundsMin = FVector(-P48DeathVolume::BaseBoxExtent);
	Point.BoundsMax = FVector(P48DeathVolume::BaseBoxExtent);
	Point.Steepness = 1.0f;
	Point.Seed = HashCombine(GetTypeHash(VolumeCenter), GetTypeHash(VolumeSize));

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
