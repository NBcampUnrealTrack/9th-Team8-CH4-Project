#include "P48PCGSeedHelpers.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

int32 P48ReadNetworkSeed(FPCGContext* Context)
{
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(P48PCGSeedNames::InputPin))
	{
		if (const UPCGParamData* Data = Cast<UPCGParamData>(Input.Data))
		{
			if (const auto* Seed = Data->Metadata->GetConstTypedAttribute<int32>(P48PCGSeedNames::Seed)) { return Seed->GetValueFromItemKey(0); }
		}
	}
	return Context->GetSeed();
}
