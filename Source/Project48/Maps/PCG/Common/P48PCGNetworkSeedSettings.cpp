#include "P48PCGNetworkSeedSettings.h"
#include "P48PCGSeedState.h"
#include "P48PCGSeedHelpers.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Engine/World.h"
#include "EngineUtils.h"

TArray<FPCGPinProperties> UP48PCGNetworkSeedSettings::OutputPinProperties() const
{
	return { FPCGPinProperties(P48PCGSeedNames::Seed, EPCGDataType::Param) };
}

FPCGElementPtr UP48PCGNetworkSeedSettings::CreateElement() const { return MakeShared<FP48PCGNetworkSeedElement>(); }

bool FP48PCGNetworkSeedElement::ExecuteInternal(FPCGContext* Context) const
{
	const auto* Settings = Context->GetInputSettings<UP48PCGNetworkSeedSettings>();
	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	if (!World || !Settings) { return true; }
	int32 Seed = Settings->EditorPreviewSeed;
	if (World->IsGameWorld())
	{
		AP48PCGSeedState* Manager = nullptr;
		for (TActorIterator<AP48PCGSeedState> It(World); It; ++It) { Manager = *It; break; }
		if (!Manager && World->GetNetMode() != NM_Client) { Manager = World->SpawnActor<AP48PCGSeedState>(); }
		// 클라이언트에서 별도 시드를 만들지 않고 서버의 복제 상태를 기다립니다.
		if (!Manager || Manager->State.Revision == 0) { return false; }
		Seed = Manager->State.Seed;
		Manager->RegisterConsumer(Cast<UPCGComponent>(Context->ExecutionSource.Get()));
		UE_LOG(LogTemp, Display, TEXT("[P48NetworkSeed] NetMode=%d Seed=%d Revision=%d"), static_cast<int32>(World->GetNetMode()), Seed, Manager->State.Revision);
	}
	UPCGParamData* Data = NewObject<UPCGParamData>();
	auto* Attribute = Data->Metadata->CreateAttribute<int32>(P48PCGSeedNames::Seed, Seed, false, false);
	Attribute->SetValue(Data->Metadata->AddEntry(), Seed);
	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Pin = P48PCGSeedNames::Seed;
	Output.Data = Data;
	return true;
}
