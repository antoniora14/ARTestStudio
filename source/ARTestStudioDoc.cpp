
// ARTestStudioDoc.cpp : implementation of the CARTestStudioDoc class
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "ARTestStudio.h"
#endif

#include "ARTestStudioDoc.h"
#include "Application/DiagramRecoveryService.h"
#include "Application/DiagramStorage.h"
#include "Application/FaultService.h"
#include "Infrastructure/TextDiagramStorage.h"

#include <filesystem>
#include <propkey.h>
#include <string>
#include <string_view>
#include <utility>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	using arteststudio::application::DescribeStorageError;
	using arteststudio::application::DescribeRecoveryReason;
	using arteststudio::application::DescribeRecoverySource;
	using arteststudio::application::DiagramRecoveryService;
	using arteststudio::application::Fault;
	using arteststudio::application::FaultCategory;
	using arteststudio::application::FaultService;
	using arteststudio::application::FaultSeverity;
	using arteststudio::application::RecoveryCandidate;
	using arteststudio::application::StorageResult;
	using arteststudio::infrastructure::TextDiagramStorage;

	[[nodiscard]] TextDiagramStorage& GetDiagramStorage() noexcept
	{
		static TextDiagramStorage storage;
		return storage;
	}

	[[nodiscard]] DiagramRecoveryService& GetRecoveryService() noexcept
	{
		static DiagramRecoveryService service{GetDiagramStorage()};
		return service;
	}

	[[nodiscard]] int AskRecoveryDecision(
		const std::filesystem::path& destination,
		const RecoveryCandidate& candidate)
	{
		std::wstring message = L"ARTestStudio found a valid recovery copy for:\n\n";
		message += destination.native();
		message += L"\n\nSource: ";
		message += DescribeRecoverySource(candidate.source);
		message += L".\nReason: ";
		message += DescribeRecoveryReason(candidate.reason);
		message += L".\n\nYes: recover the copy and replace the primary document.";
		message += L"\nNo: attempt to open the primary document without recovery.";
		message += L"\nCancel: do not open the document.";
		return AfxMessageBox(message.c_str(), MB_YESNOCANCEL | MB_ICONWARNING);
	}

	void ReportStorageFailure(
		std::wstring_view operation,
		const std::filesystem::path& path,
		const StorageResult& result)
	{
		std::wstring message = L"Could not ";
		message.append(operation);
		message += L" the diagram";
		if (!path.empty())
		{
			message += L":\n";
			message += path.native();
		}
		message += L"\n\n";
		message += DescribeStorageError(result.error);
		if (!result.detail.empty())
		{
			message += L"\n";
			message += result.detail;
		}

		std::wstring technicalDetail;
		if (!path.empty())
		{
			technicalDetail = L"Path: ";
			technicalDetail += path.native();
		}
		if (!result.detail.empty())
		{
			if (!technicalDetail.empty())
			{
				technicalDetail += L" | ";
			}
			technicalDetail += result.detail;
		}
		FaultService::Report(Fault{
			FaultSeverity::Warning,
			FaultCategory::Storage,
			L"STORAGE_" + std::to_wstring(static_cast<int>(result.error)),
			std::wstring{operation},
			std::wstring{DescribeStorageError(result.error)},
			std::move(technicalDetail)});

		AfxMessageBox(message.c_str(), MB_OK | MB_ICONERROR);
	}
}

// CARTestStudioDoc

IMPLEMENT_DYNCREATE(CARTestStudioDoc, CDocument)

BEGIN_MESSAGE_MAP(CARTestStudioDoc, CDocument)
END_MESSAGE_MAP()


// CARTestStudioDoc construction/destruction

CARTestStudioDoc::CARTestStudioDoc() noexcept
	: m_interactionController(m_diagram)
{
}

CARTestStudioDoc::~CARTestStudioDoc()
{
}

BOOL CARTestStudioDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	m_diagram.Clear();
	m_interactionController.Reset();

	return TRUE;
}




// CARTestStudioDoc serialization

void CARTestStudioDoc::Serialize(CArchive& ar)
{
	// File persistence is handled by the storage adapter in OnOpenDocument and
	// OnSaveDocument. Keep this override for MFC document compatibility.
	(void)ar;
}

BOOL CARTestStudioDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	const std::filesystem::path path = lpszPathName == nullptr
		? std::filesystem::path{}
		: std::filesystem::path{lpszPathName};
	if (path.empty())
	{
		const StorageResult result{arteststudio::application::StorageError::InvalidPath};
		ReportStorageFailure(L"open", path, result);
		return FALSE;
	}

	arteststudio::domain::DiagramModel loadedDiagram;
	StorageResult result;
	const arteststudio::application::RecoveryInspection inspection =
		GetRecoveryService().Inspect(path);
	if (inspection.candidate.available)
	{
		const int decision = AskRecoveryDecision(path, inspection.candidate);
		if (decision == IDCANCEL)
		{
			GetRecoveryService().RecordCancelled(path, inspection.candidate);
			return FALSE;
		}
		if (decision == IDYES)
		{
			result = GetRecoveryService().Recover(path, inspection.candidate, loadedDiagram);
		}
		else
		{
			GetRecoveryService().RecordDeclined(path, inspection.candidate);
			result = GetDiagramStorage().Load(path, loadedDiagram);
		}
	}
	else
	{
		result = GetDiagramStorage().Load(path, loadedDiagram);
	}
	if (!result)
	{
		ReportStorageFailure(L"open", path, result);
		return FALSE;
	}

	DeleteContents();
	m_diagram = std::move(loadedDiagram);
	m_interactionController.Reset();
	SetPathName(lpszPathName, TRUE);
	SetModifiedFlag(FALSE);
	UpdateAllViews(nullptr);
	return TRUE;
}

BOOL CARTestStudioDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	const std::filesystem::path path = lpszPathName == nullptr
		? std::filesystem::path{}
		: std::filesystem::path{lpszPathName};
	const StorageResult result = GetDiagramStorage().Save(path, m_diagram);
	if (!result)
	{
		ReportStorageFailure(L"save", path, result);
		return FALSE;
	}

	// CDocument::DoSave owns the visible document path. In particular, MFC's
	// recovery manager calls DoSave(autosavePath, FALSE), which must save a copy
	// without replacing the document name with the generated autosave filename.
	SetModifiedFlag(FALSE);
	return TRUE;
}

#ifdef SHARED_HANDLERS

// Support for thumbnails
void CARTestStudioDoc::OnDrawThumbnail(CDC& dc, LPRECT lprcBounds)
{
	// Modify this code to draw the document's data
	dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

	CString strText = _T("TODO: implement thumbnail drawing here");
	LOGFONT lf;

	CFont* pDefaultGUIFont = CFont::FromHandle((HFONT) GetStockObject(DEFAULT_GUI_FONT));
	pDefaultGUIFont->GetLogFont(&lf);
	lf.lfHeight = 36;

	CFont fontDraw;
	fontDraw.CreateFontIndirect(&lf);

	CFont* pOldFont = dc.SelectObject(&fontDraw);
	dc.DrawText(strText, lprcBounds, DT_CENTER | DT_WORDBREAK);
	dc.SelectObject(pOldFont);
}

// Support for Search Handlers
void CARTestStudioDoc::InitializeSearchContent()
{
	CString strSearchContent;
	// Set search contents from document's data.
	// The content parts should be separated by ";"

	// For example:  strSearchContent = _T("point;rectangle;circle;ole object;");
	SetSearchContent(strSearchContent);
}

void CARTestStudioDoc::SetSearchContent(const CString& value)
{
	if (value.IsEmpty())
	{
		RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
	}
	else
	{
		CMFCFilterChunkValueImpl *pChunk = nullptr;
		ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
		if (pChunk != nullptr)
		{
			pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
			SetChunkValue(pChunk);
		}
	}
}

#endif // SHARED_HANDLERS

// CARTestStudioDoc diagnostics

#ifdef _DEBUG
void CARTestStudioDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CARTestStudioDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CARTestStudioDoc commands
