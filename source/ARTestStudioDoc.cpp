
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
#include <propkey.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CARTestStudioDoc

IMPLEMENT_DYNCREATE(CARTestStudioDoc, CDocument)

BEGIN_MESSAGE_MAP(CARTestStudioDoc, CDocument)
END_MESSAGE_MAP()


// CARTestStudioDoc construction/destruction

CARTestStudioDoc::CARTestStudioDoc() noexcept
{
	// TODO: add one-time construction code here

}

CARTestStudioDoc::~CARTestStudioDoc()
{
}

BOOL CARTestStudioDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	m_diagram.Clear();

	return TRUE;
}




// CARTestStudioDoc serialization

void CARTestStudioDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

BOOL CARTestStudioDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	return 0;
}

BOOL CARTestStudioDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	return 0;
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
