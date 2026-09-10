#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "P48NicknameDatabaseSubsystem.generated.h"

class FSQLiteDatabase;

UENUM(BlueprintType)
enum class EP48NicknameRegistrationResult : uint8
{
	Success,
	InvalidUserID,
	InvalidFormat,
	UserAlreadyRegistered,
	DuplicateNickname,
	DatabaseError
};

UENUM(BlueprintType)
enum class EP48NicknameLookupResult : uint8
{
	Success,
	NotFound,
	InvalidUserID,
	DatabaseError
};

UCLASS()
class PROJECT48_API UP48NicknameDatabaseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UP48NicknameDatabaseSubsystem();
	
	UP48NicknameDatabaseSubsystem(FVTableHelper& Helper);
	
	virtual ~UP48NicknameDatabaseSubsystem() override;
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	EP48NicknameRegistrationResult TryRegisterNickname(
		const FString& UserID,
		const FString& Nickname);
	
	EP48NicknameLookupResult FindNicknameByUserID(const FString& UserID, FString& OutNickname);
	
private:
	static constexpr int32 MinNicknameLength = 2;
	static constexpr int32 MaxNicknameLength = 12;
	
	bool OpenDatabase();
	bool InitializeSchema();
	void CloseDatabase();
	
	static FString NormalizeNickname(const FString& Nickname);
	static bool IsNicknameValid(const FString& Nickname);
	
	bool IsUserRegistered(const FString& UserID, bool& bOutIsRegistered);
	bool IsNicknameTaken(const FString& NormalizedNickname, bool& bOutIsTaken);
	
	TUniquePtr<FSQLiteDatabase> Database;
	
#if !UE_BUILD_SHIPPING
	void RunDatabaseTests();
#endif
};
