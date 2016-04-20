// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidFlurryPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnalytics, Display, All);

IMPLEMENT_MODULE( FAnalyticsAndroidFlurry, AndroidFlurry )

TSharedPtr<IAnalyticsProvider> FAnalyticsProviderFlurry::Provider;


// Stores the flurry methods
namespace ANDROIDFLURRY_API Flurry
{
	static jclass Class = nullptr;	

	static jmethodID setUserId = nullptr;
	static jmethodID setGender = nullptr;
	static jmethodID setAge = nullptr;
	static jmethodID setLocation = nullptr;
	static jmethodID getSessionId = nullptr;
	static jmethodID logEvent = nullptr;
}

// Utility to create java strings from FStrings
struct jstringWrapper
{
private:
	jstring JavaString;

public:
	jstringWrapper(const FString& InStr)
	{
		JavaString = FJavaClassObject::GetJString(InStr);
	}
	~jstringWrapper()
	{
		JNIEnv*	JEnv = FAndroidApplication::GetJavaEnv();
		JEnv->DeleteGlobalRef(JavaString);
		JavaString = nullptr;
	}
	jstring Get()
	{
		return JavaString;
	}
};

// Utility to create Java maps
class FFlurryEventMap
{
	static jmethodID CreateMethod;
	static jmethodID PutMethod;

	jobject Map;
public:
	FFlurryEventMap()
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv(true);
		
		if (CreateMethod == nullptr)
		{
			check(Env != NULL);
			
			CreateMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
				"AndroidThunk_Flurry_CreateEventMap", "()Ljava/util/Map<Ljava/lang/String;Ljava/lang/String;>;", true);
			check(CreateMethod != NULL);
			
			PutMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
				"AndroidThunk_Flurry_MapPut", "(Ljava/lang/String;Ljava/lang/String;Ljava/util/Map<Ljava/lang/String;Ljava/lang/String;>;)V", true);
			check(CreateMethod != NULL);
		}

		auto MapLocal = FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, CreateMethod);
		Map = Env->NewGlobalRef(MapLocal);
		Env->DeleteLocalRef(MapLocal);
	}
	~FFlurryEventMap()
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv(true);

		Env->DeleteGlobalRef(Map);
	}

	void Put(const FString& Key, const FString& Value)
	{
		jstringWrapper JKey(Key);
		jstringWrapper JValue(Value);

		JNIEnv* Env = FAndroidApplication::GetJavaEnv(true);

		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, 
			PutMethod, JKey.Get(), JValue.Get());
	}

	jobject GetJObject() {
		return Map;
	}
};
jmethodID FFlurryEventMap::CreateMethod = nullptr;
jmethodID FFlurryEventMap::PutMethod = nullptr;


#define CALL_FLURRY_VOID(name, ...) \
FAndroidApplication::GetJavaEnv(true)->CallStaticVoidMethod(Flurry::Class, Flurry::name, __VA_ARGS__)

#define CALL_FLURRY_OBJECT(name, ...) \
FAndroidApplication::GetJavaEnv(true)->CallStaticObjectMethod(Flurry::Class, Flurry::name, __VA_ARGS__)

void FAnalyticsAndroidFlurry::StartupModule()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv(true);

	jclass LocalFlurryAgentClass = FAndroidApplication::FindJavaClass("com/flurry/android/FlurryAgent");
	check(LocalFlurryAgentClass);	
	Flurry::Class = (jclass)Env->NewGlobalRef(LocalFlurryAgentClass);
	Env->DeleteLocalRef(LocalFlurryAgentClass);

#define GET_FLURRY_METHOD(name, signature) \
	Flurry::name = FJavaWrapper::FindStaticMethod(Env, Flurry::Class, #name, signature, true)


	GET_FLURRY_METHOD(setUserId, "(Ljava/lang/String;)V");	
	GET_FLURRY_METHOD(setGender, "(B)V");
	GET_FLURRY_METHOD(setAge, "(I)V");
	GET_FLURRY_METHOD(setLocation, "(FF)V");
	GET_FLURRY_METHOD(getSessionId, "()Ljava/lang/String;");	
	GET_FLURRY_METHOD(logEvent, "(Ljava/lang/String;Ljava/util/Maplang/String;Ljava/lang/String;>;)Lcom/flurry/android/FlurryEventRecordStatus;");
	
#undef GET_FLURRY_METHOD

}

void FAnalyticsAndroidFlurry::ShutdownModule()
{
	FAnalyticsProviderFlurry::Destroy();
}

TSharedPtr<IAnalyticsProvider> FAnalyticsAndroidFlurry::CreateAnalyticsProvider(const FAnalytics::FProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		const FString Key = GetConfigValue.Execute(TEXT("FlurryApiKey"), true);
		return FAnalyticsProviderFlurry::Create(Key);
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("AndroidFlurry::CreateAnalyticsProvider called with an unbound config delegate"));
	}
	return nullptr;
}

// Provider

FAnalyticsProviderFlurry::FAnalyticsProviderFlurry(const FString Key) :
	ApiKey(Key)
{
#if WITH_FLURRY

	// Support more config options here

#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

FAnalyticsProviderFlurry::~FAnalyticsProviderFlurry()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FAnalyticsProviderFlurry::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	return true;
}

void FAnalyticsProviderFlurry::EndSession()
{
#if WITH_FLURRY
	// Flurry doesn't support ending a session
	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::EndSession - ignoring call"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::FlushEvents()
{
#if WITH_FLURRY
	// Flurry doesn't support flushing a session
	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::FlushEvents - ignoring call"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::SetUserID(const FString& InUserID)
{
	jstringWrapper javaUserId(InUserID);
	CALL_FLURRY_VOID(setUserId, javaUserId.Get());
}

FString FAnalyticsProviderFlurry::GetUserID() const
{
	return FString();
}

void FAnalyticsProviderFlurry::SetGender(const FString& InGender)
{
}

void FAnalyticsProviderFlurry::SetAge(const int32 InAge)
{
}

void FAnalyticsProviderFlurry::SetLocation(const FString& InLocation)
{
#if WITH_FLURRY
	FString Lat, Long;
	InLocation.Split(TEXT(","), &Lat, &Long);

	double Latitude = FCString::Atod(*Lat);
	double Longitude = FCString::Atod(*Long);

	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry setLatitude:Latitude longitude:Longitude horizontalAccuracy:0.0 verticalAccuracy:0.0];
	});

	UE_LOG(LogAnalytics, Display, TEXT("Parsed \"lat, long\" string in AndroidFlurry::SetLocation(%s) as \"%f, %f\""), *InLocation, Latitude, Longitude);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

FString FAnalyticsProviderFlurry::GetSessionID() const
{
	return FString();
}

bool FAnalyticsProviderFlurry::SetSessionID(const FString& InSessionID)
{
	return false;
}

void FAnalyticsProviderFlurry::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (EventName.Len() > 0)
	{
		jstringWrapper ConvertedEventName(EventName);
		FFlurryEventMap ConvertedAttributes;

		const int32 AttrCount = Attributes.Num();
		for (auto Attr : Attributes)
		{
			ConvertedAttributes.Put(Attr.AttrName, Attr.AttrValue);
		}

		CALL_FLURRY_OBJECT(logEvent, ConvertedEventName.Get(), ConvertedAttributes.GetJObject());
	}
}

void FAnalyticsProviderFlurry::RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
#if WITH_FLURRY
	NSString* EventName = @"Item Purchase";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:4];
	[AttributesDict setValue:[NSString stringWithFString:ItemId] forKey:@"ItemId"];
	[AttributesDict setValue:[NSString stringWithFString:Currency] forKey:@"Currency"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", PerItemCost] forKey:@"PerItemCost"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", ItemQuantity] forKey:@"ItemQuantity"];
	// Send the event
	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry logEvent:EventName withParameters:AttributesDict];
	});

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordItemPurchase('%s', '%s', %d, %d)"), *ItemId, *Currency, PerItemCost, ItemQuantity);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
#if WITH_FLURRY
	NSString* EventName = @"Currency Purchase";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:5];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	[AttributesDict setValue:[NSString stringWithFString:RealCurrencyType] forKey:@"RealCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%.02f", RealMoneyCost] forKey:@"RealMoneyCost"];
	[AttributesDict setValue:[NSString stringWithFString:PaymentProvider] forKey:@"PaymentProvider"];
	// Send the event
	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry logEvent:EventName withParameters:AttributesDict];
	});

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordCurrencyPurchase('%s', %d, '%s', %.02f, %s)"), *GameCurrencyType, GameCurrencyAmount, *RealCurrencyType, RealMoneyCost, *PaymentProvider);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount)
{
#if WITH_FLURRY
	NSString* EventName = @"Currency Given";
	// Build the dictionary
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:2];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	// Send the event
	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry logEvent:EventName withParameters:AttributesDict];
	});

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordCurrencyGiven('%s', %d)"), *GameCurrencyType, GameCurrencyAmount);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordItemPurchase(const FString& ItemId, int ItemQuantity, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Item Purchase";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 2;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:ItemId] forKey:@"ItemId"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", ItemQuantity] forKey:@"Quantity"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.AttrValue];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry logEvent:EventName withParameters:AttributesDict];
	});

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordItemPurchase('%s', %d, %d)"), *ItemId, ItemQuantity, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Currency Purchase";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 2;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.AttrValue];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry logEvent:EventName withParameters:AttributesDict];
	});
	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordCurrencyPurchase('%s', %d, %d)"), *GameCurrencyType, GameCurrencyAmount, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Currency Given";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 2;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:GameCurrencyType] forKey:@"GameCurrencyType"];
	[AttributesDict setValue:[NSString stringWithFormat:@"%d", GameCurrencyAmount] forKey:@"GameCurrencyAmount"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.AttrValue];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry logEvent:EventName withParameters:AttributesDict];
	});

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordCurrencyGiven('%s', %d, %d)"), *GameCurrencyType, GameCurrencyAmount, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Error";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 1;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:Error] forKey:@"Error"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.AttrValue];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry logEvent:EventName withParameters:AttributesDict];
	});

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordError('%s', %d)"), *Error, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderFlurry::RecordProgress(const FString& ProgressType, const FString& ProgressHierarchy, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_FLURRY
	NSString* EventName = @"Progress";
	// Build the dictionary
	int32 DictSize = EventAttrs.Num() + 2;
	NSDictionary* AttributesDict = [NSMutableDictionary dictionaryWithCapacity:DictSize];
	[AttributesDict setValue:[NSString stringWithFString:ProgressType] forKey:@"ProgressType"];
	[AttributesDict setValue:[NSString stringWithFString:ProgressHierarchy] forKey:@"ProgressHierarchy"];
	for	(auto Attr : EventAttrs)
	{
		NSString* AttrName = [NSString stringWithFString : Attr.AttrName];
		NSString* AttrValue = [NSString stringWithFString : Attr.AttrValue];
		[AttributesDict setValue:AttrValue forKey:AttrName];
	}
	// Send the event
	dispatch_async(dispatch_get_main_queue(), ^{
		[Flurry logEvent:EventName withParameters:AttributesDict];
	});

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordProgress('%s', %s, %d)"), *ProgressType, *ProgressHierarchy, EventAttrs.Num());
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_FLURRY=0. Are you missing the SDK?"));
#endif
}
