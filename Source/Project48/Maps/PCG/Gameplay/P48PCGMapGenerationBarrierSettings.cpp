#include "P48PCGMapGenerationBarrierSettings.h"

#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "P48PCGMapGenerationBarrier"

const FName UP48PCGMapGenerationBarrierSettings::IslandPointsPin(TEXT("IslandPoints"));
const FName UP48PCGMapGenerationBarrierSettings::DependenciesPin(TEXT("MapDependencies"));

#if WITH_EDITOR
FText UP48PCGMapGenerationBarrierSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "P48 Map Generation Barrier");
}

FText UP48PCGMapGenerationBarrierSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Waits for IslandPoints and every connected MapDependencies branch, then forwards IslandPoints to the PlayerStart stage.");
}
#endif

TArray<FPCGPinProperties> UP48PCGMapGenerationBarrierSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(IslandPointsPin, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace_GetRef(DependenciesPin, EPCGDataType::Any).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UP48PCGMapGenerationBarrierSettings::OutputPinProperties() const
{
	return { FPCGPinProperties(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point) };
}

FPCGElementPtr UP48PCGMapGenerationBarrierSettings::CreateElement() const
{
	return MakeShared<FP48PCGMapGenerationBarrierElement>();
}

bool FP48PCGMapGenerationBarrierElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(UP48PCGMapGenerationBarrierSettings::IslandPointsPin))
	{
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
