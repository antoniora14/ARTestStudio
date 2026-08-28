
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
#include "Infrastructure/FileFaultReporter.h"

#include <cstdlib>
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
	using arteststudio::infrastructure::FileFaultReporter;

	[[nodiscard]] FileFaultReporter& GetDefaultFaultReporter() noexcept
	{
		static FileFaultReporter reporter;
		return reporter;
	}

	[[nodiscard]] std::wstring GetMfcExceptionDetail(CException* exception) noexcept
	{
		if (exception == nullptr)
		{
			return L"MFC no proporciono informacion de la excepcion.";
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
		return L"Excepcion MFC sin detalle disponible.";
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

	void ShowUnexpectedFailure() noexcept
	{
		try
		{
			AfxMessageBox(
				L"ARTestStudio encontro un error inesperado y debe cerrar. "
				L"El detalle fue registrado en el archivo de diagnostico.",
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
			L"Inicializacion de la aplicacion",
			L"No se pudieron inicializar las bibliotecas OLE.");
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
			L"Inicializacion de la aplicacion",
			L"No se pudo crear la plantilla de documentos.");
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
			L"Inicializacion de la aplicacion",
			L"No se pudo crear la ventana principal.");
		delete pMainFrame;
		return FALSE;
	}
	m_pMainWnd = pMainFrame;


	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);



	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;
	// The main window has been initialized, so show and update it
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	return TRUE;
}

int CARTestStudioApp::ExitInstance()
{
	//TODO: handle additional resources you may have added
	AfxOleTerm(FALSE);

	return CWinAppEx::ExitInstance();
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
			L"Bucle principal de la aplicacion",
			L"Una excepcion MFC alcanzo la frontera global.",
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
			L"Bucle principal de la aplicacion",
			L"Una excepcion estandar alcanzo la frontera global.",
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
			L"Bucle principal de la aplicacion",
			L"Una excepcion desconocida alcanzo la frontera global.");
		ShowUnexpectedFailure();
		return EXIT_FAILURE;
	}
}

LRESULT CARTestStudioApp::ProcessWndProcException(CException* exception, const MSG* message)
{
	std::wstring operation = L"Procesamiento de un mensaje de ventana";
	if (message != nullptr)
	{
		operation += L" (mensaje ";
		operation += std::to_wstring(message->message);
		operation += L")";
	}

	ReportApplicationFault(
		FaultSeverity::Error,
		FaultCategory::UserInterface,
		L"UI_MFC_MESSAGE_EXCEPTION",
		operation,
		L"MFC capturo una excepcion durante el procesamiento de la interfaz.",
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
