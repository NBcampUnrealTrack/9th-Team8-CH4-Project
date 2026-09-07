#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "P48UIManagerComponent.generated.h"


UCLASS( Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT48_API UP48UIManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UP48UIManagerComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
