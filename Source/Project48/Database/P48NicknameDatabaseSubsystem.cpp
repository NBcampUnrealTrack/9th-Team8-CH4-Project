#include "P48NicknameDatabaseSubsystem.h"

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "SQLiteDatabase.h"

DEFINE_LOG_CATEGORY_STATIC(LogP48NicknameDatabase, Log, All);

UP48NicknameDatabaseSubsystem::UP48NicknameDatabaseSubsystem() = default;

UP48NicknameDatabaseSubsystem::UP48NicknameDatabaseSubsystem(FVTableHelper& Helper) : Super(Helper)
{
}

UP48NicknameDatabaseSubsystem::~UP48NicknameDatabaseSubsystem() = default;

void UP48NicknameDatabaseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UGameInstance* GameInstance = GetGameInstance();
	
	if (!IsValid(GameInstance) || !GameInstance->IsDedicatedServerInstance())
	{
		UE_LOG(LogP48NicknameDatabase, Verbose, TEXT("클라이언트에서는 Nickname DB를 초기화하지 않습니다."));
	
		return;
	}
	
	if (!OpenDatabase())
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("Nickname DB 초기화에 실패했습니다."));
		
		return;
	}
	
	if (!InitializeSchema())
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("Nickname DB 테이블 초기화에 실패했습니다."));
		
		CloseDatabase();
	}
}

void UP48NicknameDatabaseSubsystem::Deinitialize()
{
	CloseDatabase();
		
	Super::Deinitialize();
}

bool UP48NicknameDatabaseSubsystem::OpenDatabase()
{
	const FString DatabaseDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Database"));
	
	if (!IFileManager::Get().MakeDirectory(*DatabaseDirectory, true))
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("DB 폴더를 생성하지 못했습니다: %s"), *DatabaseDirectory);
		
		return false;
	}
	
	const FString DatabasePath = FPaths::Combine(DatabaseDirectory, TEXT("Nickname.db"));
	
	Database = MakeUnique<FSQLiteDatabase>();
	
	if (!Database->Open(
		*DatabasePath,
		ESQLiteDatabaseOpenMode::ReadWriteCreate))
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("Nickname DB를 열지 못했습니다: %s"), *DatabasePath);
		
		Database.Reset();
		
		return false;
	}
	
	UE_LOG(LogP48NicknameDatabase, Log, TEXT("Nickname DB 연결 성공: %s"), *DatabasePath);
	
	return true;
}

void UP48NicknameDatabaseSubsystem::CloseDatabase()
{
	if (!Database)
	{
		return;
	}
	
	if (Database->IsValid() && !Database->Close())
	{
		UE_LOG(LogP48NicknameDatabase, Warning, TEXT("Nickname DB를 정상적으로 닫지 못했습니다."));
	}
	
	Database.Reset();
	
	UE_LOG(LogP48NicknameDatabase, Log, TEXT("Nickname DB 연결 종료"));
}

bool UP48NicknameDatabaseSubsystem::InitializeSchema()
{
	if (!Database || !Database->IsValid())
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("테이블을 생성할 수 있는 DB 연결이 없습니다."));
		
		return false;
	}
	
	static const TCHAR* CreateNicknameRegistryQuery = TEXT(
		"CREATE TABLE IF NOT EXISTS NicknameRegistry ("
		"UserID TEXT NOT NULL PRIMARY KEY, "
		"Nickname TEXT NOT NULL, "
		"NormalizedNickname TEXT NOT NULL UNIQUE, "
		"CreatedAt TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
		")");
	
	if (!Database->Execute(CreateNicknameRegistryQuery))
	{
		UE_LOG(LogP48NicknameDatabase,
			Error,
			TEXT("NicknameRegistry 테이블 생성 실패: %s"),
			*Database->GetLastError());
		
		return false;
	}
	
	UE_LOG(LogP48NicknameDatabase, Log, TEXT("NicknameRegistry 테이블 준비 완료"));
	
	return true;
}

FString UP48NicknameDatabaseSubsystem::NormalizeNickname(const FString& Nickname)
{
	FString NormalizeNickname = Nickname.TrimStartAndEnd();
	NormalizeNickname.ToLowerInline();
	
	return NormalizeNickname;
}

bool UP48NicknameDatabaseSubsystem::IsNicknameValid(const FString& Nickname)
{
	const FString TrimmedNickname = Nickname.TrimStartAndEnd();
	
	if (TrimmedNickname.Len() < MinNicknameLength || TrimmedNickname.Len() > MaxNicknameLength)
	{
		return false;
	}
	
	for (const TCHAR Character : TrimmedNickname)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}
	
	return true;
}

bool UP48NicknameDatabaseSubsystem::IsUserRegistered(const FString& UserID, bool& bOutIsRegistered)
{
	bOutIsRegistered = false;
	
	if (!Database || !Database->IsValid())
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("사용자 등록 여부를 조회할 DB 연결이 없습니다."));
		
		return false;
	}
	
	FSQLitePreparedStatement Statement =
		Database->PrepareStatement(
			TEXT(
				"SELECT 1 "
				"FROM NicknameRegistry "
				"WHERE UserID = ? "
				"LIMIT 1;"));
	
	if (!Statement.IsValid())
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("사용자 조회문 준비 실패: %s"),
			*Database->GetLastError());
		
		return false;
	}
	
	if (!Statement.SetBindingValueByIndex(1, UserID))
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("사용자 조회값 설정 실패: %s"),
			*Database->GetLastError());
		
		return false;
	}
	
	const ESQLitePreparedStatementStepResult StepResult = Statement.Step();
	
	if (StepResult == ESQLitePreparedStatementStepResult::Row)
	{
		bOutIsRegistered = true;
		return true;
	}
	
	if (StepResult == ESQLitePreparedStatementStepResult::Done)
	{
		return true;
	}
	
	UE_LOG(
		LogP48NicknameDatabase,
		Error,
		TEXT("사용자 등록 여부 조회 실패: %s"),
		*Database->GetLastError());
	
	return false;
}

bool UP48NicknameDatabaseSubsystem::IsNicknameTaken(const FString& NormalizedNickname, bool& bOutIsTaken)
{
	bOutIsTaken = false;
	
	if (!Database || !Database->IsValid())
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("닉네임 중복 여부를 조회할 DB 연결이 없습니다."));
		
		return false;
	}
	
	FSQLitePreparedStatement Statement =
		Database->PrepareStatement(
			TEXT(
				"SELECT 1 "
				"FROM NicknameRegistry "
				"WHERE NormalizedNickname = ? "
				"LIMIT 1;"));
	
	if (!Statement.IsValid())
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("닉네임 조회문 준비 실패: %s"),
			*Database->GetLastError());
		
		return false;
	}
	
	if (!Statement.SetBindingValueByIndex(1, NormalizedNickname))
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("닉네임 조회값 설정 실패: %s"),
			*Database->GetLastError());
		
		return false;
	}
	
	const ESQLitePreparedStatementStepResult StepResult = Statement.Step();
	
	if (StepResult == ESQLitePreparedStatementStepResult::Row)
	{
		bOutIsTaken = true;
		return true;
	}
	
	if (StepResult == ESQLitePreparedStatementStepResult::Done)
	{
		return true;
	}
	
	UE_LOG(LogP48NicknameDatabase, Error, TEXT("닉네임 중복 여부 조회 실패: %s"), *Database->GetLastError());
	
	return false;
}

EP48NicknameRegistrationResult UP48NicknameDatabaseSubsystem::TryRegisterNickname(const FString& UserID, const FString& Nickname)
{
	if (UserID.IsEmpty())
	{
		return EP48NicknameRegistrationResult::InvalidUserID;
	}
	
	if (!IsNicknameValid(Nickname))
	{
		return EP48NicknameRegistrationResult::InvalidFormat;
	}
	
	const FString DisplayNickname = Nickname.TrimStartAndEnd();
	
	const FString NormalizedNickname = NormalizeNickname(Nickname);
	
	bool bIsUserRegistered = false;
	
	if (!IsUserRegistered(UserID, bIsUserRegistered))
	{
		return EP48NicknameRegistrationResult::DatabaseError;
	}
	
	if (bIsUserRegistered)
	{
		return EP48NicknameRegistrationResult::UserAlreadyRegistered;
	}
	
	bool bIsNicknameTaken = false;
	
	if (!IsNicknameTaken(NormalizedNickname, bIsNicknameTaken))
	{
		return EP48NicknameRegistrationResult::DatabaseError;
	}
	
	if (bIsNicknameTaken)
	{
		return EP48NicknameRegistrationResult::DuplicateNickname;
	}
	
	if (!Database || !Database->IsValid())
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("닉네임을 등록할 DB 연결이 없습니다."));
		
		
		return EP48NicknameRegistrationResult::DatabaseError;
	}
	
	FSQLitePreparedStatement Statement =
		Database->PrepareStatement(
			TEXT(
				"INSERT INTO NicknameRegistry "
				"(UserID, Nickname, NormalizedNickname) "
				"Values (?, ?, ?);"));
	
	if (!Statement.IsValid())
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("닉네임 등록문 준비 실패: %s"),
			*Database->GetLastError());
		
		return EP48NicknameRegistrationResult::DatabaseError;
	}
	
	const bool bBindingSucceeded =
		Statement.SetBindingValueByIndex(1, UserID) &&
		Statement.SetBindingValueByIndex(2, DisplayNickname) &&
		Statement.SetBindingValueByIndex(3, NormalizedNickname);
	
	if (!bBindingSucceeded)
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("닉네임 등록값 설정 실패: %s"),
			*Database->GetLastError());
		
		return EP48NicknameRegistrationResult::DatabaseError;
	}
	
	if (!Statement.Execute())
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("닉네임 등록 실패: %s"),
			*Database->GetLastError());
		
		return EP48NicknameRegistrationResult::DatabaseError;
	}
	
	UE_LOG(
		LogP48NicknameDatabase,
		Log,
		TEXT("닉네임 등록 성공: %s"),
		*DisplayNickname);
	
	return EP48NicknameRegistrationResult::Success;
		
}

EP48NicknameLookupResult UP48NicknameDatabaseSubsystem::FindNicknameByUserID(const FString& UserID, FString& OutNickname)
{
	OutNickname.Reset();
	
	if (UserID.IsEmpty())
	{
		return EP48NicknameLookupResult::InvalidUserID;
	}
	
	if (!Database || !Database->IsValid())
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("닉네임을 조회할 DB 연결이 없습니다."));
		
		return EP48NicknameLookupResult::DatabaseError;
	}
	
	FSQLitePreparedStatement Statement =
		Database->PrepareStatement(
			TEXT(
				"SELECT Nickname "
				"FROM NicknameRegistry "
				"WHERE UserID = ? "
				"LIMIT 1;"));
	
	if (!Statement.IsValid())
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("닉네임 조회문 준비 실패: %s"),
			*Database->GetLastError());
		
		return EP48NicknameLookupResult::DatabaseError;
	}
	
	if (!Statement.SetBindingValueByIndex(1, UserID))
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("닉네임 조회값 설정 실패: %s"),
			*Database->GetLastError());
		
		return EP48NicknameLookupResult::DatabaseError;
	}
	
	const ESQLitePreparedStatementStepResult StepResult = Statement.Step();
	
	if (StepResult == ESQLitePreparedStatementStepResult::Done)
	{
		return EP48NicknameLookupResult::NotFound;
	}
	
	if (StepResult != ESQLitePreparedStatementStepResult::Row)
	{
		UE_LOG(
			LogP48NicknameDatabase,
			Error,
			TEXT("사용자 닉네임 조회 실패: %s"),
			*Database->GetLastError());
		
		return EP48NicknameLookupResult::DatabaseError;
	}
	
	if (!Statement.GetColumnValueByIndex(0, OutNickname))
	{
		UE_LOG(LogP48NicknameDatabase, Error, TEXT("조회된 닉네임 값을 읽지 못했습니다."));
		
		OutNickname.Reset();
		
		return EP48NicknameLookupResult::DatabaseError;
	}
	
	return EP48NicknameLookupResult::Success;
}

#if !UE_BUILD_SHIPPING
void UP48NicknameDatabaseSubsystem::RunDatabaseTests()
{
	const FString TestSuffix = FString::Printf(TEXT("%06d"), FMath::RandRange(0, 999999));
	
	const FString TestUserA = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	
	const FString TestUserB = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	
	const FString TestNickname = TEXT("Test") + TestSuffix;
	
	const FString OtherNickname = TEXT("Other") + TestSuffix;
	
	// 최초 등록 성공 검사
	const EP48NicknameRegistrationResult FirstRegistration =
		TryRegisterNickname(TestUserA, TestNickname);
	
	UE_LOG(
		LogP48NicknameDatabase,
		Log,
		TEXT("[DB TEST] 최초 등록: %s"),
		FirstRegistration == EP48NicknameRegistrationResult::Success ? TEXT("PASS") : TEXT("FAIL"));
	
	// 다른 사용자가 같은 닉네임을 등록하면 실패
	const EP48NicknameRegistrationResult DuplicateRegistration =
		TryRegisterNickname(TestUserB, TestNickname);
	
	UE_LOG(
		LogP48NicknameDatabase,
		Log,
		TEXT("[DB TEST] 닉네임 중복 차단: %s"),
		DuplicateRegistration ==
		EP48NicknameRegistrationResult::DuplicateNickname ? TEXT("PASS") : TEXT("FAIL"));
	
	// 같은 사용자가 다른 닉네임을 다시 등록하면 실패
	const EP48NicknameRegistrationResult UserRegistration =
		TryRegisterNickname(TestUserA, OtherNickname);
	
	UE_LOG(
		LogP48NicknameDatabase,
		Log,
		TEXT("[DB TEST] 사용자 중복 등록 차단: %s"),
		UserRegistration ==
		EP48NicknameRegistrationResult::UserAlreadyRegistered ? TEXT("PASS") : TEXT("FAIL"));
	
	// 등록한 사용자의 닉네임을 다시 읽음
	FString LoadedNickname;
	
	const EP48NicknameLookupResult LookupResult =
		FindNicknameByUserID(TestUserA, LoadedNickname);
	
	const bool bLookupPassed =
		LookupResult == EP48NicknameLookupResult::Success &&
			LoadedNickname == TestNickname;
	
	UE_LOG(
		LogP48NicknameDatabase,
		Log,
		TEXT("[DB TEST] 등록 닉네임 조회: %s"),
		bLookupPassed ? TEXT("PASS") : TEXT("FAIL"));
	
	// 규칙에 마지 않는 닉네임 등록 차단
	const EP48NicknameRegistrationResult InvalidRegistration =
		TryRegisterNickname(TestUserB, TEXT("!"));
	
	UE_LOG(
		LogP48NicknameDatabase,
		Log,
		TEXT("[DB TEST] 잘못된 형식 차단: %s"),
		InvalidRegistration ==
		EP48NicknameRegistrationResult::InvalidFormat ? TEXT("PASS") : TEXT("FAIL"));
}
#endif