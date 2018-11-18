// Copyright 2018 Kokku. All Rights Reserved.

#include "AndroidFlurry.h" 

#include "AndroidFlurryProvider.h" 
#include "Android/AndroidJNI.h" 
#include "Android/AndroidApplication.h"
#include "AndroidJava.h"

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

		UE_LOG(LogAnalytics, Log, TEXT("FFlurryEventMap()"));

		
		if (CreateMethod == nullptr)
		{
			check(Env != NULL);

			UE_LOG(LogAnalytics, Log, TEXT("FFlurryEventMap() - init CreateMethod"));
			
			CreateMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
				"AndroidThunk_Flurry_CreateEventMap", "()Ljava/util/Map;", true);
			check(CreateMethod != NULL);
			
			PutMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID,
				"AndroidThunk_Flurry_MapPut", "(Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;)V", true);
			check(CreateMethod != NULL);
		}

		auto MapLocal = FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, CreateMethod);
		Map = Env->NewGlobalRef(MapLocal);
		Env->DeleteLocalRef(MapLocal);

		UE_LOG(LogAnalytics, Log, TEXT("FFlurryEventMap() - Map=%p"), Map);
	}
	~FFlurryEventMap()
	{
		UE_LOG(LogAnalytics, Log, TEXT("~FFlurryEventMap() - Map=%p"), Map);

		JNIEnv* Env = FAndroidApplication::GetJavaEnv(true);
		Env->DeleteGlobalRef(Map);
		Map = nullptr;
	}

	void Put(const FString& Key, const FString& Value)
	{
		jstringWrapper JKey(Key);
		jstringWrapper JValue(Value);

		UE_LOG(LogAnalytics, Log, TEXT("FFlurryEventMap::Put()"));


		JNIEnv* Env = FAndroidApplication::GetJavaEnv(true);

		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, 
			PutMethod, JKey.Get(), JValue.Get(), Map);
	}

	jobject GetJObject() {
		return Map;
	}
};
jmethodID FFlurryEventMap::CreateMethod = nullptr;
jmethodID FFlurryEventMap::PutMethod = nullptr;


#define CALL_FLURRY_VOID(name, ...) \
FAndroidApplication::GetJavaEnv()->CallStaticVoidMethod(Flurry::Class, Flurry::name, __VA_ARGS__)

#define CALL_FLURRY_OBJECT(name, ...) \
FAndroidApplication::GetJavaEnv()->CallStaticObjectMethod(Flurry::Class, Flurry::name, __VA_ARGS__)

void FAnalyticsAndroidFlurry::StartupModule()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv(true);

	UE_LOG(LogAnalytics, Log, TEXT("FAnalyticsAndroidFlurry::StartupModule, Env=%p"), Env);


	jclass LocalFlurryAgentClass = FAndroidApplication::FindJavaClass("com/flurry/android/FlurryAgent");
	check(LocalFlurryAgentClass);	
	Flurry::Class = (jclass)Env->NewGlobalRef(LocalFlurryAgentClass);
	Env->DeleteLocalRef(LocalFlurryAgentClass);

#define GET_FLURRY_METHOD(name, signature) \
	Flurry::name = FJavaWrapper::FindStaticMethod(Env, Flurry::Class, #name, signature, true);\
	check(Flurry::name)


	GET_FLURRY_METHOD(setUserId, "(Ljava/lang/String;)V");	
	GET_FLURRY_METHOD(setGender, "(B)V");
	GET_FLURRY_METHOD(setAge, "(I)V");
	GET_FLURRY_METHOD(setLocation, "(FF)V");
	GET_FLURRY_METHOD(getSessionId, "()Ljava/lang/String;");	
	GET_FLURRY_METHOD(logEvent, "(Ljava/lang/String;Ljava/util/Map;)Lcom/flurry/android/FlurryEventRecordStatus;");
	
#undef GET_FLURRY_METHOD

}

void FAnalyticsAndroidFlurry::ShutdownModule()
{
	FAnalyticsProviderFlurry::Destroy();
}

TSharedPtr<IAnalyticsProvider> FAnalyticsAndroidFlurry::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
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

	jstringWrapper LatWrap(Lat), LongWrap(Long);
	CALL_FLURRY_OBJECT(setLocation, Latitude, Longitude, 0.0, 0.0);

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
		UE_LOG(LogAnalytics, Warning, TEXT("FAnalyticsAndroidFlurry::RecordEvent, EventName=%s"), *EventName);

		jstringWrapper ConvertedEventName(EventName);
		FFlurryEventMap ConvertedAttributes;

		const int32 AttrCount = Attributes.Num();
		for (auto Attr : Attributes)
		{
			UE_LOG(LogAnalytics, Log, TEXT("FAnalyticsAndroidFlurry::RecordEvent, Attr:%s=%s"), *Attr.AttrName, *Attr.AttrValueString);
			ConvertedAttributes.Put(Attr.AttrName, Attr.AttrValueString);
		}

		CALL_FLURRY_OBJECT(logEvent, ConvertedEventName.Get(), ConvertedAttributes.GetJObject());
	}
}

void FAnalyticsProviderFlurry::RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
	FString EventName = "Item Purchase";
	
	// Build the dictionary
	TArray<FAnalyticsEventAttribute> AttributesDict;
	AttributesDict.Add(FAnalyticsEventAttribute("ItemId", ItemId));
	AttributesDict.Add(FAnalyticsEventAttribute("Currency", Currency));
	AttributesDict.Add(FAnalyticsEventAttribute("PerItemCost", FString::FromInt(PerItemCost)));
	AttributesDict.Add(FAnalyticsEventAttribute("ItemQuantity", FString::FromInt(ItemQuantity)));

	// Send the event
	RecordEvent(EventName, AttributesDict);

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordItemPurchase('%s', '%s', %d, %d)"), *ItemId, *Currency, PerItemCost, ItemQuantity);
}

void FAnalyticsProviderFlurry::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
	FString EventName = "Currency Purchase";

	// Build the dictionary
	TArray<FAnalyticsEventAttribute> AttributesDict;
	AttributesDict.Add(FAnalyticsEventAttribute("GameCurrencyType", GameCurrencyType));
	AttributesDict.Add(FAnalyticsEventAttribute("GameCurrencyAmount", FString::FromInt(GameCurrencyAmount)));
	AttributesDict.Add(FAnalyticsEventAttribute("RealCurrencyType", RealCurrencyType));
	AttributesDict.Add(FAnalyticsEventAttribute("RealMoneyCost", FString::Printf(TEXT("%.02f"), RealMoneyCost)));
	AttributesDict.Add(FAnalyticsEventAttribute("PaymentProvider", PaymentProvider));

	// Send the event
	RecordEvent(EventName, AttributesDict);

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordCurrencyPurchase('%s', %d, '%s', %.02f, %s)"), *GameCurrencyType, GameCurrencyAmount, *RealCurrencyType, RealMoneyCost, *PaymentProvider);
}

void FAnalyticsProviderFlurry::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount)
{
	FString EventName = "Currency Given";

	// Build the dictionary
	TArray<FAnalyticsEventAttribute> AttributesDict;
	AttributesDict.Add(FAnalyticsEventAttribute("GameCurrencyType", GameCurrencyType));
	AttributesDict.Add(FAnalyticsEventAttribute("GameCurrencyAmount", FString::FromInt(GameCurrencyAmount)));
	
	// Send the event
	RecordEvent(EventName, AttributesDict);

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordCurrencyGiven('%s', %d)"), *GameCurrencyType, GameCurrencyAmount);
}

void FAnalyticsProviderFlurry::RecordItemPurchase(const FString& ItemId, int ItemQuantity, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
	FString EventName = "Item Purchase";

	// Build the dictionary
	TArray<FAnalyticsEventAttribute> AttributesDict;
	AttributesDict.Add(FAnalyticsEventAttribute("ItemId", ItemId));
	AttributesDict.Add(FAnalyticsEventAttribute("ItemQuantity", FString::FromInt(ItemQuantity)));
	AttributesDict.Append(EventAttrs);

	// Send the event
	RecordEvent(EventName, AttributesDict);

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordItemPurchase('%s', %d, %d)"), *ItemId, ItemQuantity, EventAttrs.Num());
}

void FAnalyticsProviderFlurry::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
	FString EventName = "Currency Purchase";

	// Build the dictionary
	TArray<FAnalyticsEventAttribute> AttributesDict;
	AttributesDict.Add(FAnalyticsEventAttribute("GameCurrencyType", GameCurrencyType));
	AttributesDict.Add(FAnalyticsEventAttribute("GameCurrencyAmount", FString::FromInt(GameCurrencyAmount)));
	AttributesDict.Append(EventAttrs);

	// Send the event
	RecordEvent(EventName, AttributesDict);

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordCurrencyPurchase('%s', %d, %d)"), *GameCurrencyType, GameCurrencyAmount, EventAttrs.Num());
}

void FAnalyticsProviderFlurry::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
	FString EventName = "Currency Given";

	// Build the dictionary
	TArray<FAnalyticsEventAttribute> AttributesDict;
	AttributesDict.Add(FAnalyticsEventAttribute("GameCurrencyType", GameCurrencyType));
	AttributesDict.Add(FAnalyticsEventAttribute("GameCurrencyAmount", FString::FromInt(GameCurrencyAmount)));
	AttributesDict.Append(EventAttrs);

	// Send the event
	RecordEvent(EventName, AttributesDict);

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordCurrencyGiven('%s', %d, %d)"), *GameCurrencyType, GameCurrencyAmount, EventAttrs.Num());
}

void FAnalyticsProviderFlurry::RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
	FString EventName = "Error";

	// Build the dictionary
	TArray<FAnalyticsEventAttribute> AttributesDict;
	AttributesDict.Add(FAnalyticsEventAttribute("Error", Error));
	AttributesDict.Append(EventAttrs);

	// Send the event
	RecordEvent(EventName, AttributesDict);

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordError('%s', %d)"), *Error, EventAttrs.Num());
}

void FAnalyticsProviderFlurry::RecordProgress(const FString& ProgressType, const FString& ProgressHierarchy, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
	FString EventName = "Progress";

	// Build the dictionary
	TArray<FAnalyticsEventAttribute> AttributesDict;
	AttributesDict.Add(FAnalyticsEventAttribute("ProgressType", ProgressType));
	AttributesDict.Add(FAnalyticsEventAttribute("ProgressHierarchy", ProgressHierarchy));
	AttributesDict.Append(EventAttrs);

	// Send the event
	RecordEvent(EventName, AttributesDict);

	UE_LOG(LogAnalytics, Display, TEXT("AndroidFlurry::RecordProgress('%s', %s, %d)"), *ProgressType, *ProgressHierarchy, EventAttrs.Num());
}
