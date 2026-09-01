
// ARTestStudio.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "ARTestStudio.h"
#include "MainFrm.h"

#include "ChildFrm.h"
#include "ARTestStudioDoc.h"
#include "ARTestStudioView.h"
#include "Application/FaultService.h"
#include "Application/UnexpectedCloseRecovery.h"
#include "Infrastructure/FileFaultReporter.h"

#include <afxdatarecovery.h>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	using arteststudio::application::Fault;
	using arteststudio::application::FaultCategory;
	using arteststudio::application::FaultService;
	using arteststudio::application::FaultSeverity;
	using arteststudio::application::PendingRecoveryDisposition;
	using arteststudio::application::PendingRecoverySession;
	using arteststudio::infrastructure::FileFaultReporter;

	constexpr LPCTSTR RecoveryProfileSection = _T("UnexpectedCloseRecovery");
	constexpr LPCTSTR RecoveryRestartIdentifierEntry = _T("RestartIdentifier");
	constexpr LPCTSTR RecoveryProcessIdEntry = _T("ProcessId");

	[[nodiscard]] FileFaultReporter& GetDefaultFaultReporter() noexcept
	{
		static FileFaultReporter reporter;
		return reporter;
	}

	[[nodiscard]] std::wstring GetMfcExceptionDetail(CException* exception) noexcept
	{
		if (exception == nullptr)
		{
			return L"MFC did not provide exception information.";
		}

		try
		{
			wchar_t buffer[1024]{};
			if (exception->GetErrorMessage(buffer, static_cast<UINT>(std::size(buffer))))
			{
				return buffer;
			}
		}
		catch (...)
		{
		}
		return L"MFC exception details are unavailable.";
	}

	[[nodiscard]] std::wstring GetStandardExceptionDetail(const std::exception& exception)
	{
		const std::string_view narrow = exception.what() == nullptr ? std::string_view{} : exception.what();
		std::wstring detail;
		detail.reserve(narrow.size());
		for (const unsigned char character : narrow)
		{
			detail.push_back(static_cast<wchar_t>(character));
		}
		return detail;
	}

	void ReportApplicationFault(
		FaultSeverity severity,
		FaultCategory category,
		std::wstring_view code,
		std::wstring_view operation,
		std::wstring_view message,
		std::wstring_view detail = {}) noexcept
	{
		try
		{
			FaultService::Report(Fault{
				severity,
				category,
				std::wstring{code},
				std::wstring{operation},
				std::wstring{message},
				std::wstring{detail}});
		}
		catch (...)
		{
		}
	}

	void ClearPendingRecoveryProfile() noexcept
	{
		try
		{
			CWinApp* application = AfxGetApp();
			if (application != nullptr)
			{
				application->WriteProfileString(
					RecoveryProfileSection,
					RecoveryRestartIdentifierEntry,
					nullptr);
				application->WriteProfileString(
					RecoveryProfileSection,
					RecoveryProcessIdEntry,
					nullptr);
			}
		}
		catch (...)
		{
		}
	}

	void ClearPendingRecoveryProfileForCurrentProcess() noexcept
	{
		CWinApp* application = AfxGetApp();
		if (application == nullptr)
		{
			return;
		}

		const UINT recordedProcessId = application->GetProfileInt(
			RecoveryProfileSection,
			RecoveryProcessIdEntry,
			0);
		if (recordedProcessId == ::GetCurrentProcessId())
		{
			ClearPendingRecoveryProfile();
		}
	}

	[[nodiscard]] bool IsProcessRunning(DWORD processId) noexcept
	{
		if (processId == 0)
		{
			return false;
		}

		HANDLE process = ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
		if (process == nullptr)
		{
			return ::GetLastError() == ERROR_ACCESS_DENIED;
		}

		const DWORD waitResult = ::WaitForSingleObject(process, 0);
		::CloseHandle(process);
		return waitResult == WAIT_TIMEOUT;
	}

	class CPersistentDataRecoveryHandler final : public CDataRecoveryHandler
	{
	public:
		CPersistentDataRecoveryHandler(DWORD supportFlags, int autosaveInterval)
			: CDataRecoveryHandler(supportFlags, autosaveInterval)
		{
		}

		BOOL AutosaveDocumentInfo(CDocument* document, BOOL resetModifiedFlag = TRUE) override
		{
			const BOOL result = CDataRecoveryHandler::AutosaveDocumentInfo(document, resetModifiedFlag);
			if (result && resetModifiedFlag)
			{
				PersistCatalog();
			}
			else if (!result)
			{
				ReportApplicationFault(
					FaultSeverity::Warning,
					FaultCategory::Storage,
					L"RECOVERY_AUTOSAVE_FAILED",
					L"Autosave session",
					L"The periodic recovery copy could not be created.");
			}
			return result;
		}

		BOOL RemoveDocumentInfo(CDocument* document) override
		{
			const BOOL result = CDataRecoveryHandler::RemoveDocumentInfo(document);
			PersistCatalog();
			return result;
		}

		BOOL DeleteAllAutosavedFiles() override
		{
			const BOOL result = CDataRecoveryHandler::DeleteAllAutosavedFiles();
			ClearPendingRecoveryProfileForCurrentProcess();
			return result;
		}

	private:
		[[nodiscard]] bool HasAutosavedDocument() const
		{
			POSITION position = m_mapDocNameToAutosaveName.GetStartPosition();
			while (position != nullptr)
			{
				CString document;
				CString autosave;
				m_mapDocNameToAutosaveName.GetNextAssoc(position, document, autosave);
				if (!autosave.IsEmpty())
				{
					return true;
				}
			}
			return false;
		}

		void DeletePersistedCatalog() noexcept
		{
			try
			{
				CWinApp* application = AfxGetApp();
				if (application != nullptr && !GetRestartIdentifier().IsEmpty())
				{
					CRegKey applicationKey(application->GetAppRegistryKey());
					applicationKey.RecurseDeleteKey(GetRestartIdentifier());
				}
			}
			catch (...)
			{
			}
		}

		void PersistCatalog() noexcept
		{
			if (!HasAutosavedDocument())
			{
				DeletePersistedCatalog();
				ClearPendingRecoveryProfileForCurrentProcess();
				return;
			}

			DeletePersistedCatalog();
			if (!CDataRecoveryHandler::SaveOpenDocumentList())
			{
				ReportApplicationFault(
					FaultSeverity::Warning,
					FaultCategory::Storage,
					L"RECOVERY_AUTOSAVE_CATALOG_FAILED",
					L"Catalog autosave",
					L"The periodic copy exists but could not be registered for a manual restart.");
				return;
			}

			CWinApp* application = AfxGetApp();
			if (application == nullptr
				|| !application->WriteProfileString(
					RecoveryProfileSection,
					RecoveryRestartIdentifierEntry,
					GetRestartIdentifier())
				|| !application->WriteProfileInt(
					RecoveryProfileSection,
					RecoveryProcessIdEntry,
					::GetCurrentProcessId()))
			{
				ReportApplicationFault(
					FaultSeverity::Warning,
					FaultCategory::Storage,
					L"RECOVERY_AUTOSAVE_CATALOG_FAILED",
					L"Catalog autosave",
					L"The session identity could not be retained for a manual restart.");
			}
		}
	};

	void ShowUnexpectedFailure() noexcept
	{
		try
		{
			AfxMessageBox(
				L"ARTestStudio encountered an unexpected error and must close. "
				L"Diagnostic details were recorded in the log file.",
				MB_OK | MB_ICONERROR);
		}
		catch (...)
		{
		}
	}
}


// CARTestStudioApp

BEGIN_MESSAGE_MAP(CARTestStudioApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CARTestStudioApp::OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, &CWinAppEx::OnFilePrintSetup)
END_MESSAGE_MAP()


// CARTestStudioApp construction

CARTestStudioApp::CARTestStudioApp() noexcept
{
	FaultService::Configure(&GetDefaultFaultReporter());
	m_bHiColorIcons = TRUE;


	m_nAppLook = 0;
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_ALL_ASPECTS;
	// Keep crash-recovery autosaves frequent enough to make unexpected-close
	// recovery useful without coupling the visible document path to autosave files.
	m_nAutosaveInterval = 60 * 1000;
#ifdef _MANAGED
	// If the application is built using Common Language Runtime support (/clr):
	//     1) This additional setting is needed for Restart Manager support to work properly.
	//     2) In your project, you must add a reference to System.Windows.Forms in order to build.
	System::Windows::Forms::Application::SetUnhandledExceptionMode(System::Windows::Forms::UnhandledExceptionMode::ThrowException);
#endif

	// TODO: replace application ID string below with unique ID string; recommended
	// format for string is CompanyName.ProductName.SubProduct.VersionInformation
	SetAppID(_T("ARTestStudio.AppID.NoVersion"));

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

// The one and only CARTestStudioApp object

CARTestStudioApp theApp;


// CARTestStudioApp initialization

BOOL CARTestStudioApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();


	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		ReportApplicationFault(
			FaultSeverity::Critical,
			FaultCategory::Application,
			L"APP_OLE_INIT_FAILED",
			L"Application initialization",
			L"The OLE libraries could not be initialized.");
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	EnableTaskbarInteraction();

	// AfxInitRichEdit2() is required to use RichEdit control
	// AfxInitRichEdit2();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));
	LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)


	InitContextMenuManager();

	InitKeyboardManager();

	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CMultiDocTemplate* pDocTemplate;
	pDocTemplate = new CMultiDocTemplate(IDR_ARTestStudioTYPE,
		RUNTIME_CLASS(CARTestStudioDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CARTestStudioView));
	if (!pDocTemplate)
	{
		ReportApplicationFault(
			FaultSeverity::Critical,
			FaultCategory::Application,
			L"APP_DOCUMENT_TEMPLATE_FAILED",
			L"Application initialization",
			L"The document template could not be created.");
		return FALSE;
	}
	AddDocTemplate(pDocTemplate);

	// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
	{
		ReportApplicationFault(
			FaultSeverity::Critical,
			FaultCategory::Application,
			L"APP_MAIN_FRAME_FAILED",
			L"Application initialization",
			L"The main window could not be created.");
		delete pMainFrame;
		return FALSE;
	}
	m_pMainWnd = pMainFrame;


	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);
	bool restoredPersistentSession = false;
	if (cmdInfo.m_nShellCommand == CCommandLineInfo::FileNew)
	{
		const CString pendingRestartIdentifier = GetProfileString(
			RecoveryProfileSection,
			RecoveryRestartIdentifierEntry);
		const UINT pendingProcessId = GetProfileInt(
			RecoveryProfileSection,
			RecoveryProcessIdEntry,
			0);
		const PendingRecoverySession pendingSession{
			std::wstring{pendingRestartIdentifier.GetString()},
			static_cast<std::uint32_t>(pendingProcessId)};
		const PendingRecoveryDisposition disposition =
			arteststudio::application::EvaluatePendingRecoverySession(
				pendingSession,
				static_cast<std::uint32_t>(::GetCurrentProcessId()),
				IsProcessRunning(pendingProcessId));

		if (disposition == PendingRecoveryDisposition::RecoverPreviousSession)
		{
			CDataRecoveryHandler* recoveryHandler = GetDataRecoveryHandler();
			if (recoveryHandler != nullptr)
			{
				const CString currentRestartIdentifier = recoveryHandler->GetRestartIdentifier();
				recoveryHandler->SetRestartIdentifier(pendingRestartIdentifier);
				ReportApplicationFault(
					FaultSeverity::Information,
					FaultCategory::Storage,
					L"RECOVERY_UNEXPECTED_CLOSE_DETECTED",
					L"Restore session",
					L"A previous session that ended without a normal shutdown was detected.");

				restoredPersistentSession = RestartInstance() != FALSE;
				recoveryHandler->SetRestartIdentifier(currentRestartIdentifier);
				ClearPendingRecoveryProfile();

				ReportApplicationFault(
					restoredPersistentSession ? FaultSeverity::Information : FaultSeverity::Warning,
					FaultCategory::Storage,
					restoredPersistentSession
						? L"RECOVERY_UNEXPECTED_CLOSE_RESTARTED"
						: L"RECOVERY_UNEXPECTED_CLOSE_FAILED",
					L"Restore session",
					restoredPersistentSession
						? L"ARTestStudio processed the previous session during the manual restart."
						: L"The previous session could not be reconstructed from the autosave catalog.");
			}
		}
		else if (disposition == PendingRecoveryDisposition::Invalid)
		{
			ClearPendingRecoveryProfile();
			ReportApplicationFault(
				FaultSeverity::Warning,
				FaultCategory::Storage,
				L"RECOVERY_AUTOSAVE_CATALOG_INVALID",
				L"Restore session",
				L"An incomplete or invalid autosave catalog was discarded.");
		}
	}

	if (cmdInfo.m_nShellCommand == CCommandLineInfo::RestartByRestartManager)
	{
		ReportApplicationFault(
			FaultSeverity::Information,
			FaultCategory::Storage,
			L"RECOVERY_UNEXPECTED_CLOSE_RESTARTED",
			L"Restore session",
			L"Windows Restart Manager restarted ARTestStudio to restore documents and autosaves.");
	}



	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!restoredPersistentSession && !ProcessShellCommand(cmdInfo))
		return FALSE;
	if (cmdInfo.m_nShellCommand == CCommandLineInfo::RestartByRestartManager)
	{
		ClearPendingRecoveryProfile();
	}
	// The main window has been initialized, so show and update it
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	return TRUE;
}

int CARTestStudioApp::ExitInstance()
{
	//TODO: handle additional resources you may have added
	ClearPendingRecoveryProfileForCurrentProcess();
	AfxOleTerm(FALSE);

	return CWinAppEx::ExitInstance();
}

CDataRecoveryHandler* CARTestStudioApp::GetDataRecoveryHandler()
{
	if ((SupportsRestartManager() || SupportsApplicationRecovery())
		&& m_pDataRecoveryHandler == nullptr)
	{
		auto* handler = new CPersistentDataRecoveryHandler(
			m_dwRestartManagerSupportFlags,
			m_nAutosaveInterval);
		if (!handler->Initialize())
		{
			delete handler;
			return nullptr;
		}
		m_pDataRecoveryHandler = handler;
	}

	return m_pDataRecoveryHandler;
}

int CARTestStudioApp::Run()
{
	try
	{
		return CWinAppEx::Run();
	}
	catch (CException* exception)
	{
		const std::wstring detail = GetMfcExceptionDetail(exception);
		ReportApplicationFault(
			FaultSeverity::Critical,
			FaultCategory::Unexpected,
			L"APP_UNHANDLED_MFC_EXCEPTION",
			L"Application message loop",
			L"An MFC exception reached the global boundary.",
			detail);
		if (exception != nullptr)
		{
			exception->Delete();
		}
		ShowUnexpectedFailure();
		return EXIT_FAILURE;
	}
	catch (const std::exception& exception)
	{
		const std::wstring detail = GetStandardExceptionDetail(exception);
		ReportApplicationFault(
			FaultSeverity::Critical,
			FaultCategory::Unexpected,
			L"APP_UNHANDLED_STANDARD_EXCEPTION",
			L"Application message loop",
			L"A standard exception reached the global boundary.",
			detail);
		ShowUnexpectedFailure();
		return EXIT_FAILURE;
	}
	catch (...)
	{
		ReportApplicationFault(
			FaultSeverity::Critical,
			FaultCategory::Unexpected,
			L"APP_UNHANDLED_UNKNOWN_EXCEPTION",
			L"Application message loop",
			L"An unknown exception reached the global boundary.");
		ShowUnexpectedFailure();
		return EXIT_FAILURE;
	}
}

LRESULT CARTestStudioApp::ProcessWndProcException(CException* exception, const MSG* message)
{
	std::wstring operation = L"Window message processing";
	if (message != nullptr)
	{
		operation += L" (message ";
		operation += std::to_wstring(message->message);
		operation += L")";
	}

	ReportApplicationFault(
		FaultSeverity::Error,
		FaultCategory::UserInterface,
		L"UI_MFC_MESSAGE_EXCEPTION",
		operation,
		L"MFC caught an exception while processing the user interface.",
		GetMfcExceptionDetail(exception));
	return CWinAppEx::ProcessWndProcException(exception, message);
}

// CARTestStudioApp message handlers


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg() noexcept;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// App command to run the dialog
void CARTestStudioApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// CARTestStudioApp customization load/save methods

void CARTestStudioApp::PreLoadState()
{
	BOOL bNameValid;
	CString strName;
	bNameValid = strName.LoadString(IDS_EDIT_MENU);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EDIT);
	bNameValid = strName.LoadString(IDS_EXPLORER);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EXPLORER);
}

void CARTestStudioApp::LoadCustomState()
{
}

void CARTestStudioApp::SaveCustomState()
{
}

// CARTestStudioApp message handlers
