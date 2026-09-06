#include "P48MeshBoundsComponent.h"

#include "Engine/StaticMesh.h"

UP48MeshBoundsComponent::UP48MeshBoundsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UP48MeshBoundsComponent::MeasureMesh(UStaticMesh* Mesh, const FVector MeshScale, FP48MeasuredMeshBounds& OutBounds) const
{
	OutBounds = FP48MeasuredMeshBounds();
	if (!Mesh)
	{
		return false;
	}

	const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
	OutBounds.LocalOrigin = MeshBounds.Origin * MeshScale;
	OutBounds.ScaledSize = MeshBounds.BoxExtent * 2.0f * MeshScale.GetAbs();
	return OutBounds.IsValid();
}
