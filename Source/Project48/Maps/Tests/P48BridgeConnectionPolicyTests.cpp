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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FP48BridgeCandidateSelectionTest,
	"P48.Maps.Bridge.CandidateSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48BridgeCandidateSelectionTest::RunTest(const FString& Parameters)
{
	TArray<FP48BridgeEndpointCandidate> Candidates;
	auto AddCandidate = [&Candidates](const FVector& Start, const FVector& End)
	{
		FP48BridgeEndpointCandidate& Candidate = Candidates.Emplace_GetRef();
		Candidate.Start = Start;
		Candidate.End = End;
		Candidate.Length = FVector::Distance(Start, End);
	};

	// 입력 순서와 무관하게 가장 높은 공통 높이를 먼저, 같은 높이에서는 더 수평인 후보를 먼저 사용합니다.
	AddCandidate(FVector(0.0, 300.0, 900.0), FVector(1000.0, 300.0, 900.0));
	AddCandidate(FVector(0.0, 100.0, 1000.0), FVector(1000.0, 100.0, 980.0));
	AddCandidate(FVector(0.0, 200.0, 1000.0), FVector(1000.0, 200.0, 1000.0));
	P48BridgeConnectionPolicy::SortEndpointCandidates(Candidates);

	TestTrue(TEXT("Highest and level candidate is first"), Candidates[0].Start.Equals(FVector(0.0, 200.0, 1000.0)));
	TestTrue(TEXT("Same-height alternate remains before lower candidate"), Candidates[1].Start.Equals(FVector(0.0, 100.0, 1000.0)));
	TestTrue(TEXT("Lower candidate is last"), Candidates[2].Start.Equals(FVector(0.0, 300.0, 900.0)));

	TMap<int32, TArray<FVector>> UsedEndpoints;
	UsedEndpoints.FindOrAdd(10).Add(Candidates[0].Start);
	UsedEndpoints.FindOrAdd(20).Add(Candidates[0].End);
	int32 SelectedIndex = P48BridgeConnectionPolicy::FindFirstAvailableCandidate(Candidates, 10, 20, UsedEndpoints, 50.0f);
	TestEqual(TEXT("Occupied highest point falls back to another high point"), SelectedIndex, 1);

	P48BridgeConnectionPolicy::ReserveCandidateEndpoints(Candidates[SelectedIndex], 10, 20, UsedEndpoints);
	SelectedIndex = P48BridgeConnectionPolicy::FindFirstAvailableCandidate(Candidates, 10, 20, UsedEndpoints, 50.0f);
	TestEqual(TEXT("Exhausted high points fall back to the next lower point"), SelectedIndex, 2);

	P48BridgeConnectionPolicy::ReserveCandidateEndpoints(Candidates[SelectedIndex], 10, 20, UsedEndpoints);
	SelectedIndex = P48BridgeConnectionPolicy::FindFirstAvailableCandidate(Candidates, 10, 20, UsedEndpoints, 50.0f);
	TestEqual(TEXT("All occupied points are never reused"), SelectedIndex, INDEX_NONE);

	TMap<int32, TArray<FVector>> OtherIslandUsage;
	OtherIslandUsage.FindOrAdd(99).Add(Candidates[0].Start);
	SelectedIndex = P48BridgeConnectionPolicy::FindFirstAvailableCandidate(Candidates, 10, 20, OtherIslandUsage, 50.0f);
	TestEqual(TEXT("Endpoint exclusion is scoped to each island"), SelectedIndex, 0);

	float ChildBaseHeight = 0.0f;
	TestTrue(
		TEXT("Different mesh anchor offsets can share one world bridge height"),
		P48BridgeConnectionPolicy::TryCalculateAlignedIslandHeight(500.0f, 300.0f, 100.0f, -1000.0f, 2000.0f, ChildBaseHeight));
	TestTrue(TEXT("Aligned child base height preserves the parent world anchor height"), FMath::IsNearlyEqual(ChildBaseHeight + 100.0f, 800.0f));
	TestFalse(
		TEXT("An aligned island outside the allowed height range is rejected instead of clamped"),
		P48BridgeConnectionPolicy::TryCalculateAlignedIslandHeight(1900.0f, 300.0f, 100.0f, -1000.0f, 2000.0f, ChildBaseHeight));

	return true;
}

#endif
