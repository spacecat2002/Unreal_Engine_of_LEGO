// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// This file is modified from Microsoft's AzureSpatialAnchors DirectX sample.

#include "stdafx.h"

#include "AzureSpatialAnchors.h"

#include "SpatialAnchorHelper.h"

#include <ppltasks.h>
#include <string>
#include <sstream>
#include <winrt\Windows.Foundation.Metadata.h>
#include <winrt\Windows.Security.Authorization.AppCapabilityAccess.h>
#include <winrt\Windows.System.Threading.h>

using namespace WindowsMixedReality;
using namespace concurrency;
using namespace Microsoft::WRL;
using namespace std::placeholders;
using namespace std::literals::chrono_literals;
using namespace winrt::Windows::Foundation::Metadata;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::UI::Input::Spatial;
using namespace winrt::Windows::Security::Authorization::AppCapabilityAccess;
using namespace winrt::Windows::System::Threading;
using namespace winrt::Microsoft::Azure::SpatialAnchors;

class AzureSpatialAnchorsInteropImpl : public AzureSpatialAnchorsInterop
{
public:
	static void Create(WindowsMixedReality::MixedRealityInterop& interop, void(*LogFunctionPointer)(const wchar_t* LogMsg));
	static AzureSpatialAnchorsInteropImpl& Get();
	static void Release();

	virtual bool CreateSession() override;
	virtual bool ConfigSession(const wchar_t* accountId, const wchar_t* accountKey, int logVerbosity) override;
	virtual bool StartSession() override;
	virtual void StopSession() override;
	virtual void DestroySession() override;

	virtual bool HasEnoughDataForSaving() override;
	virtual bool CreateCloudAnchor(const wchar_t* anchorId) override;
	virtual bool SetCloudAnchorExpiration(const wchar_t* anchorId, int minutesFromNow) override;
	virtual bool SaveCloudAnchor(const wchar_t* anchorId, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize) override;
	virtual bool DeleteCloudAnchor(const wchar_t* anchorId, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize) override;
	virtual bool LoadCloudAnchorByID(const wchar_t* anchorId, const wchar_t* localAnchorId, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize) override;

protected:
	AzureSpatialAnchorsInteropImpl(WindowsMixedReality::MixedRealityInterop& interop, void(*FunctionPointer)(const wchar_t* LogMsgFunc));
	virtual ~AzureSpatialAnchorsInteropImpl();
	AzureSpatialAnchorsInteropImpl(const AzureSpatialAnchorsInteropImpl&) = delete;
	AzureSpatialAnchorsInteropImpl& operator=(const AzureSpatialAnchorsInteropImpl&) = delete;

private:
	bool CheckForSession(const wchar_t* contex) const;
	winrt::fire_and_forget SaveAnchor(const wchar_t* anchorId, winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* cloudAnchor, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize);
	winrt::fire_and_forget DeleteAnchor(const wchar_t* anchorId, winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* cloudAnchor, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize);

	void AddEventListeners();
	void RemoveEventListeners();

	void Log(const wchar_t* LogMsg) const;
	void Log(std::wstringstream& stream) const;

	WindowsMixedReality::MixedRealityInterop& m_mixedRealityInterop;

	/** Function pointer for logging */
	void(*OnLog)(const wchar_t*);

	static AzureSpatialAnchorsInteropImpl* Instance;

	bool m_enoughDataForSaving{ false };
	bool m_asyncOpInProgress{ false };
	bool m_sessionStarted{ false };

	std::wstring m_logText;

	winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSession m_cloudSession{ nullptr };

	winrt::event_revoker<winrt::Microsoft::Azure::SpatialAnchors::ICloudSpatialAnchorSession> m_anchorLocatedToken;
	winrt::event_revoker<winrt::Microsoft::Azure::SpatialAnchors::ICloudSpatialAnchorSession> m_locateAnchorsCompletedToken;
	winrt::event_revoker<winrt::Microsoft::Azure::SpatialAnchors::ICloudSpatialAnchorSession> m_sessionUpdatedToken;
	winrt::event_revoker<winrt::Microsoft::Azure::SpatialAnchors::ICloudSpatialAnchorSession> m_errorToken;
	winrt::event_revoker<winrt::Microsoft::Azure::SpatialAnchors::ICloudSpatialAnchorSession> m_onLogDebugToken;

	std::map<std::wstring, winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> m_cloudAnchors;
	std::map<std::wstring, std::wstring> m_loadingCloudAnchorIdToLocalAnchorIDMap;
	winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* GetCloudAnchor(const wchar_t* anchorId);

	std::mutex m_mutex;
};

AzureSpatialAnchorsInteropImpl* AzureSpatialAnchorsInteropImpl::Instance = nullptr;

void AzureSpatialAnchorsInterop::Create(WindowsMixedReality::MixedRealityInterop& interop, void(*LogFunctionPointer)(const wchar_t* LogMsg))
{
	AzureSpatialAnchorsInteropImpl::Create(interop, LogFunctionPointer);
}

void AzureSpatialAnchorsInteropImpl::Create(WindowsMixedReality::MixedRealityInterop& interop, void(*LogFunctionPointer)(const wchar_t* LogMsg))
{
	assert(AzureSpatialAnchorsInteropImpl::Instance == nullptr);
	AzureSpatialAnchorsInteropImpl::Instance = new AzureSpatialAnchorsInteropImpl(interop, LogFunctionPointer);
	assert(AzureSpatialAnchorsInteropImpl::Instance);
}

AzureSpatialAnchorsInterop& AzureSpatialAnchorsInterop::Get()
{
	return AzureSpatialAnchorsInteropImpl::Get();
}

AzureSpatialAnchorsInteropImpl& AzureSpatialAnchorsInteropImpl::Get()
{
	assert(AzureSpatialAnchorsInteropImpl::Instance);

	return *AzureSpatialAnchorsInteropImpl::Instance;
}

void AzureSpatialAnchorsInterop::Release()
{
	AzureSpatialAnchorsInteropImpl::Release();
}

void AzureSpatialAnchorsInteropImpl::Release()
{
	if (AzureSpatialAnchorsInteropImpl::Instance != nullptr)
	{
		delete AzureSpatialAnchorsInteropImpl::Instance;
		AzureSpatialAnchorsInteropImpl::Instance = nullptr;
	}
}

bool AzureSpatialAnchorsInteropImpl::CheckForSession(const wchar_t* context) const
{
	if (m_cloudSession == nullptr)
	{
		{ std::wstringstream string; string << context << L" called, but session does not exist!  Ignoring."; Log(string); }
		return false;
	}

	if (m_sessionStarted == false)
	{
		{ std::wstringstream string; string << context << L" called, but session is already started.  Ignoring."; Log(string); }
		return false;
	}

	return true;
}

bool AzureSpatialAnchorsInteropImpl::CreateSession()
{
	Log(L"CreateSession");

	if (m_cloudSession != nullptr)
	{
		Log(L"CreateSession called, but session already exists!  Ignoring.");
		return false;
	}

	//CreateSession
	m_enoughDataForSaving = false;
	m_cloudSession = CloudSpatialAnchorSession();

	return m_cloudSession != nullptr;
}

bool AzureSpatialAnchorsInteropImpl::ConfigSession(const wchar_t* accountId, const wchar_t* accountKey, int logVerbosity)
{
	Log(L"ConfigSession");

	if (m_cloudSession == nullptr)
	{
		Log(L"ConfigSession called, but session does not exist!  Ignoring.");
		return false;
	}

	if (accountId == nullptr || *accountId == 0)
	{
		Log(L"ConfigSession called, but accountId is null or empty!  This session should not be useable.");
	}

	if (accountKey == nullptr || *accountKey == 0)
	{
		Log(L"ConfigSession called, but accountId is null or empty!  This session should not be useable.");
	}
	
	// Session configuration takes effect on Start() according to documentation.

	//ConfigSession
	auto configuration = m_cloudSession.Configuration();
	configuration.AccountId(accountId);
	configuration.AccountKey(accountKey);

	SessionLogLevel logLevel = (SessionLogLevel)logVerbosity;
	if ((logLevel < SessionLogLevel::None) || (logLevel > SessionLogLevel::All))
	{
		logLevel = (logLevel < SessionLogLevel::None) ? SessionLogLevel::None : SessionLogLevel::All;
		{ std::wstringstream string; string << L"ConfigSession called with invalid log level " << logVerbosity << ".  Clamping the value to " << (int)logLevel; Log(string); }
	}

	m_cloudSession.LogLevel(logLevel);
	AddEventListeners();

	return true;
}

bool AzureSpatialAnchorsInteropImpl::StartSession() 
{
	Log(L"StartSession");

	if (m_cloudSession == nullptr)
	{
		Log(L"StartSession called, but session does not exist!  Ignoring.");
		return false;
	}

	if (m_sessionStarted == true)
	{
		Log(L"StartSession called, but session is already started.  Ignoring.");
		return true;
	}

	//StartSession
	m_cloudSession.Start();
	m_sessionStarted = true;

	return true;
}

void AzureSpatialAnchorsInteropImpl::StopSession()
{
	Log(L"StopSession");

	if (m_cloudSession == nullptr)
	{
		Log(L"StopSession called, but session has already been cleaned up.  Ignoring.");
		return;
	}

	if (m_sessionStarted == false)
	{
		Log(L"StopSession called, but session is not started.  Ignoring.");
		return;
	}

	//StopSession
	m_sessionStarted = false;
	m_cloudSession.Stop();
}

void AzureSpatialAnchorsInteropImpl::DestroySession()
{
	Log(L"StopSession");

	if (m_cloudSession == nullptr)
	{
		Log(L"StartSession called, but session does not exist!  Ignoring.");
		return;
	}

	//DestroySession
	auto lock = std::unique_lock<std::mutex>(m_mutex);
	RemoveEventListeners();
	m_cloudAnchors.clear();
	m_sessionStarted = false;
	m_cloudSession = nullptr;
}
bool AzureSpatialAnchorsInteropImpl::HasEnoughDataForSaving()
{
	return m_enoughDataForSaving;
}

bool AzureSpatialAnchorsInteropImpl::CreateCloudAnchor(const wchar_t* anchorId)
{
	{ std::wstringstream string; string << L"CreateCloudAnchor from a local anchor " << anchorId; Log(string); }
	auto lock = std::unique_lock<std::mutex>(m_mutex);

	if (anchorId == nullptr)
	{
		Log(L"CreateCloudAnchor failed because anchorId is null!");
		return false;
	}
	std::shared_ptr<class SpatialAnchorHelper> spatialAnchorHelper = GetSpatialAnchorHelper();
	winrt::Windows::Perception::Spatial::SpatialAnchor* localAnchor = spatialAnchorHelper->GetSpatialAnchor(anchorId);
	if (localAnchor == nullptr)
	{
		{ std::wstringstream string; string << L"CreateCloudAnchor failed because anchorId " << anchorId << L" does not exist!  You must create the local anchor first."; Log(string); }
		return false;
	}
	else
	{
		winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor newCloudAnchor = CloudSpatialAnchor();
		newCloudAnchor.LocalAnchor(*localAnchor);
		m_cloudAnchors.insert(std::make_pair(anchorId, newCloudAnchor));
		return true;
	}
}

bool AzureSpatialAnchorsInteropImpl::SetCloudAnchorExpiration(const wchar_t* anchorId, int minutesFromNow)
{
	{ std::wstringstream string; string << L"SetCloudAnchorExpiration for anchor " << anchorId; Log(string); }

	if (minutesFromNow <= 0)
	{
		{ std::wstringstream string; string << L"SetCloudAnchorExpiration failed because minutesFromNow is " << minutesFromNow << L" which is <= 0."; Log(string); }
		return false;
	}

	winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* cloudAnchor = GetCloudAnchor(anchorId);
	if (!cloudAnchor)
	{
		{ std::wstringstream string; string << L"SetCloudAnchorExpiration failed because anchorId " << anchorId << L" does not exist!  You must create the cloud anchor first."; Log(string); }
		return false;
	}
	else
	{
		const winrt::Windows::Foundation::DateTime minutesFromNowDateTime = winrt::Windows::Foundation::DateTime::clock::now() + std::chrono::minutes(minutesFromNow);
		cloudAnchor->Expiration(minutesFromNowDateTime);
		return true;
	}
}

bool AzureSpatialAnchorsInteropImpl::SaveCloudAnchor(const wchar_t* anchorId, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize)
{
	{ std::wstringstream string; string << L"SaveCloudAnchor for anchor " << anchorId; Log(string); }

	winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* cloudAnchor = GetCloudAnchor(anchorId);
	if (!cloudAnchor)
	{
		{ std::wstringstream string; string << L"SaveCloudAnchor failed because anchorId " << anchorId << L" does not exist!  You must create the cloud anchor first."; Log(string); }
		OutResult = SaveResult::FailNoLocalAnchor;
		return false;
	}

	if (!m_enoughDataForSaving)
	{
		Log(L"Cannot save AzureSpatialAnchor yet, not enough data. Look around more then try again.");
		OutResult = SaveResult::FailNotEnoughData;
		return false;
	}

	if (!CheckForSession(L"SaveCloudAnchor"))
	{
		OutResult = SaveResult::FailNoSession;
		return false;
	}

	SaveAnchor(anchorId, cloudAnchor, OutResult, OutError, OutErrorSize);

	OutResult = SaveResult::Started;
	return true;
}

winrt::fire_and_forget AzureSpatialAnchorsInteropImpl::SaveAnchor(const wchar_t* anchorId, winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* cloudAnchor, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize)
{
	assert(cloudAnchor);

	try
	{
		{ std::wstringstream string; string << L"SaveCloudAnchor saving anchor " << anchorId; Log(string); }

		m_asyncOpInProgress = true;
		co_await m_cloudSession.CreateAnchorAsync(*cloudAnchor);
		m_asyncOpInProgress = false;
		
		OutResult = SaveResult::Success;
		{ std::wstringstream string; string << L"SaveCloudAnchor saved anchor " << anchorId << L" with cloud ID [" << cloudAnchor->Identifier().c_str() << "]"; Log(string); }
	}
	catch (winrt::hresult_error e)
	{
		m_asyncOpInProgress = false;
		OutResult = SaveResult::FailSeeErrorString;
		wcsncpy_s(OutError, OutErrorSize, e.message().c_str(), OutErrorSize - 1);
		{ std::wstringstream string; string << L"SaveCloudAnchor failed to save anchor " << anchorId << L" message: " << e.message().c_str(); Log(string); }
	}
}

bool AzureSpatialAnchorsInteropImpl::DeleteCloudAnchor(const wchar_t* anchorId, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize)
{
	{ std::wstringstream string; string << L"SaveCloudAnchor for anchor " << anchorId; Log(string); }

	winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* cloudAnchor = GetCloudAnchor(anchorId);
	if (!cloudAnchor)
	{
		{ std::wstringstream string; string << L"SaveCloudAnchor failed because anchorId " << anchorId << L" does not exist!  You must create the cloud anchor first."; Log(string); }
		OutResult = SaveResult::FailNoLocalAnchor;
		return false;
	}

	DeleteAnchor(anchorId, cloudAnchor, OutResult, OutError, OutErrorSize);
	
	OutResult = SaveResult::Started;
	return true;
}

winrt::fire_and_forget AzureSpatialAnchorsInteropImpl::DeleteAnchor(const wchar_t* anchorId, winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* cloudAnchor, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize)
{
	try
	{
		m_asyncOpInProgress = true;
		co_await m_cloudSession.DeleteAnchorAsync(*cloudAnchor);
		m_asyncOpInProgress = false;
		m_cloudAnchors.erase(anchorId);

		OutResult = SaveResult::Success;
		{ std::wstringstream string; string << L"DeleteAnchor deleted anchor " << anchorId; Log(string); }
	}
	catch (winrt::hresult_error e)
	{
		m_asyncOpInProgress = false;
		OutResult = SaveResult::FailSeeErrorString;
		wcsncpy_s(OutError, OutErrorSize, e.message().c_str(), OutErrorSize - 1);
		{ std::wstringstream string; string << L"SaveCloudAnchor failed to save anchor " << anchorId << L" message: " << e.message().c_str(); Log(string); }
	}

	co_return;
}

bool AzureSpatialAnchorsInteropImpl::LoadCloudAnchorByID(const wchar_t* cloudAnchorId, const wchar_t* localAnchorId, SaveResult& OutResult, wchar_t*& OutError, int OutErrorSize)
{
	Log(L"FindCloudAnchorByID");

	if (!CheckForSession(L"SaveCloudAnchor"))
	{
		OutResult = SaveResult::FailNoSession;
		return false;
	}

	m_loadingCloudAnchorIdToLocalAnchorIDMap.insert(std::make_pair(cloudAnchorId, localAnchorId));

	AnchorLocateCriteria criteria = AnchorLocateCriteria();
	criteria.Identifiers({ cloudAnchorId });
	auto& watcher = m_cloudSession.CreateWatcher(criteria);

	OutResult = SaveResult::Started;
	{ std::wstringstream string; string << L"LoadCloudAnchorByID created watcher " << watcher.Identifier() << " for " << cloudAnchorId; Log(string); }
	return true;
}

void AzureSpatialAnchorsInteropImpl::AddEventListeners()
{
	if (m_cloudSession == nullptr)
	{
		return;
	}

	if (m_anchorLocatedToken)
	{
		// Event listeners have already been setup, possible if we 'configure' multiple times.
		return;
	}

	m_anchorLocatedToken = m_cloudSession.AnchorLocated(winrt::auto_revoke, [this](auto&&, auto&& args)
	{
		switch (args.Status())
		{
		case LocateAnchorStatus::Located:
		{
			auto lock = std::unique_lock<std::mutex>(m_mutex);
			{ std::wstringstream string; string << L"AnchorLocated status Located Id: " << args.Anchor().Identifier().c_str(); Log(string); }

			winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor loadedCloudAnchor = args.Anchor();

			const std::wstring& localAnchorId = m_loadingCloudAnchorIdToLocalAnchorIDMap.at(loadedCloudAnchor.Identifier().c_str());

			std::shared_ptr<class SpatialAnchorHelper> spatialAnchorHelper = GetSpatialAnchorHelper();
			if (!spatialAnchorHelper->StoreSpatialAnchor(localAnchorId.c_str(), args.Anchor().LocalAnchor()))
			{
				{ std::wstringstream string; string << L"AnchorLocated error.  Local anchor with id " << localAnchorId.c_str() << " already exists, so we cannot store this one!"; Log(string); }
				//TODO something better so we don't just fail here.
			}
			m_cloudAnchors.insert(std::make_pair(localAnchorId, loadedCloudAnchor));
			m_loadingCloudAnchorIdToLocalAnchorIDMap.erase(loadedCloudAnchor.Identifier().c_str());

			//TODO: pass info back to create the pin, etc
		}
		break;
		case LocateAnchorStatus::AlreadyTracked:
			{ std::wstringstream string; string << L"AnchorLocated status AlreadyTracked"; Log(string); }
			break;
		case LocateAnchorStatus::NotLocated:
			{ std::wstringstream string; string << L"AnchorLocated status NotLocated"; Log(string); }
			break;
		case LocateAnchorStatus::NotLocatedAnchorDoesNotExist:
			{ std::wstringstream string; string << L"AnchorLocated status NotLocatedAnchorDoesNotExist"; Log(string); }
			break;
		}
	});

	m_locateAnchorsCompletedToken = m_cloudSession.LocateAnchorsCompleted(winrt::auto_revoke, [this](auto&&, auto&& args)
	{
		{ std::wstringstream string; string << L"LocateAnchorsCompleted watcher " << args.Watcher().Identifier() << " has completed."; Log(string); }
		//if (m_step >= static_cast<DemoStep>(static_cast<uint32_t>(DemoStep::CreateForQuery) + 1))
		//{
		//	m_locateCount += 1;
		//	m_logText = L"locate from completed ", std::to_wstring(m_locateCount);
		//	OutputDebugStringW(m_logText.c_str());
		//}
	});

	m_sessionUpdatedToken = m_cloudSession.SessionUpdated(winrt::auto_revoke, [this](auto&&, auto&& args)
	{
		auto status = args.Status();
		m_enoughDataForSaving = status.RecommendedForCreateProgress() >= 1.0f;
		//m_statusText = StatusToString(status);
	});

	m_errorToken = m_cloudSession.Error(winrt::auto_revoke, [this](auto&&, auto&& args)
	{
		m_logText = args.ErrorMessage();
		Log(m_logText.c_str());
	});

	m_onLogDebugToken = m_cloudSession.OnLogDebug(winrt::auto_revoke, [this](auto&&, auto&& args)
	{
		m_logText = args.Message();
		Log(m_logText.c_str());
	});
}

void AzureSpatialAnchorsInteropImpl::RemoveEventListeners()
{
	if (m_cloudSession != nullptr)
	{
		m_anchorLocatedToken.revoke();
		m_locateAnchorsCompletedToken.revoke();
		m_sessionUpdatedToken.revoke();
		m_errorToken.revoke();
		m_onLogDebugToken.revoke();
	}
}

winrt::Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor* AzureSpatialAnchorsInteropImpl::GetCloudAnchor(const wchar_t* anchorId)
{
	auto& iterator = m_cloudAnchors.find(anchorId);
	if (iterator == m_cloudAnchors.end())
	{
		return nullptr;
	}
	return &(iterator->second);
}


//Log(L"testlog");
//{ std::wstringstream string; string << L"testlog function foo failed with error code " << ErrorCode; Log(string); }
void AzureSpatialAnchorsInteropImpl::Log(const wchar_t* LogMsg) const
{
	if (OnLog)
	{
		OnLog(LogMsg);
	}
}

void AzureSpatialAnchorsInteropImpl::Log(std::wstringstream& stream) const
{
	Log(stream.str().c_str());
}

AzureSpatialAnchorsInteropImpl::AzureSpatialAnchorsInteropImpl(WindowsMixedReality::MixedRealityInterop& interop, void(*LogFunctionPointer)(const wchar_t* LogMsg))
	: AzureSpatialAnchorsInterop(),
	m_mixedRealityInterop(interop),
	OnLog(LogFunctionPointer)
{
}

AzureSpatialAnchorsInteropImpl::~AzureSpatialAnchorsInteropImpl() 
{
}