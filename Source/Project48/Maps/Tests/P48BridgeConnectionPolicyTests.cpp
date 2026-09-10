#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Datas/Structs/P48BridgeConnectionRules.h"
#include "../Utilities/P48BridgeConnectionPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FP48BridgeConnectionGeometryTest,
	"P48.Maps.Bridge.ConnectionGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48BridgeConnectionGeometryTest::RunTest(const FString& Parameters)
{
	FP48BridgeConnectionRules Rules;
	Rules.MinHorizontalDistance = 500.0f;
	Rules.MaxHeightDifference = 1000.0f;
	Rules.MaxBridgeLength = 2000.0f;
	Rules.MaxSlopeAngle = 45.0f;

	float Length = 0.0f;
	TestTrue(
		TEXT("Valid geometry passes the shared policy"),
		P48BridgeConnectionPolicy::IsGeometryValid(
			FVector::ZeroVector,
			FVector(1000.0, 0.0, 500.0),
			Rules,
			Length));
	TestTrue(TEXT("The shared policy reports three-dimensional length"), FMath::IsNearlyEqual(Length, FVector(1000.0, 0.0, 500.0).Length(), 0.01));

	TestFalse(
		TEXT("Too little horizontal distance is rejected"),
		P48BridgeConnectionPolicy::IsGeometryValid(FVector::ZeroVector, FVector(499.0, 0.0, 0.0), Rules, Length));
	TestFalse(
		TEXT("Too much height difference is rejected"),
		P48BridgeConnectionPolicy::IsGeometryValid(FVector::ZeroVector, FVector(1000.0, 0.0, 1001.0), Rules, Length));
	TestFalse(
		TEXT("Too much total length is rejected"),
		P48BridgeConnectionPolicy::IsGeometryValid(FVector::ZeroVector, FVector(2001.0, 0.0, 0.0), Rules, Length));
	TestFalse(
		TEXT("Too steep a slope is rejected"),
		P48BridgeConnectionPolicy::IsGeometryValid(FVector::ZeroVector, FVector(600.0, 0.0, 601.0), Rules, Length));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FP48BridgeEndpointDuplicateTest,
	"P48.Maps.Bridge.EndpointDuplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48BridgeEndpointDuplicateTest::RunTest(const FString& Parameters)
{
	const FVector Start(100.0, 200.0, 300.0);
	const FVector End(1000.0, 2000.0, 500.0);
	constexpr float Tolerance = 10.0f;

	TestTrue(
		TEXT("Equal endpoints are duplicates"),
		P48BridgeConnectionPolicy::AreSameUndirectedEndpoints(Start, End, Start, End, Tolerance));
	TestTrue(
		TEXT("Reversed endpoints are duplicates"),
		P48BridgeConnectionPolicy::AreSameUndirectedEndpoints(Start, End, End, Start, Tolerance));
	TestTrue(
		TEXT("Endpoints inside tolerance are duplicates"),
		P48BridgeConnectionPolicy::AreSameUndirectedEndpoints(
			Start,
			End,
			Start + FVector(5.0, 0.0, 0.0),
			End + FVector(0.0, 5.0, 0.0),
			Tolerance));
	TestFalse(
		TEXT("Distinct endpoints remain separate"),
		P48BridgeConnectionPolicy::AreSameUndirectedEndpoints(
			Start,
			End,
			Start + FVector(11.0, 0.0, 0.0),
			End,
			Tolerance));

	return true;
}

#endif
