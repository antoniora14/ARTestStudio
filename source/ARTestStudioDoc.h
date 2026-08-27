
// ARTestStudioDoc.h : interface of the CARTestStudioDoc class
//


#pragma once

#include "Domain/DiagramModel.h"


class CARTestStudioDoc : public CDocument
{
protected: // create from serialization only
	CARTestStudioDoc() noexcept;
	DECLARE_DYNCREATE(CARTestStudioDoc)

// Attributes
public:

// Operations
public:
	[[nodiscard]] arteststudio::domain::DiagramModel& GetDiagram() noexcept { return m_diagram; }
	[[nodiscard]] const arteststudio::domain::DiagramModel& GetDiagram() const noexcept { return m_diagram; }

// Overrides
public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);

#ifdef SHARED_HANDLERS
	virtual void InitializeSearchContent();
	virtual void OnDrawThumbnail(CDC& dc, LPRECT lprcBounds);
#endif // SHARED_HANDLERS

// Implementation
public:
	virtual ~CARTestStudioDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	arteststudio::domain::DiagramModel m_diagram;

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()

#ifdef SHARED_HANDLERS
	// Helper function that sets search content for a Search Handler
	void SetSearchContent(const CString& value);
#endif // SHARED_HANDLERS
};
