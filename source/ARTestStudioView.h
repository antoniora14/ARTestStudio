// ARTestStudioView.h : interface of the CARTestStudioView class

#pragma once

#include "Domain/DiagramModel.h"

#include <optional>

class CARTestStudioDoc;

class CARTestStudioView : public CView
{
private:
	COleDropTarget m_dropTarget;

	bool m_drawingLine = false;
	std::optional<arteststudio::domain::NodeId> m_startNodeId;
	arteststudio::domain::PortId m_startPortId = arteststudio::domain::PortId::Top;
	CPoint m_tempEndPoint;

	std::optional<arteststudio::domain::NodeId> m_draggingNodeId;
	std::optional<arteststudio::domain::NodeId> m_rightClickNodeId;
	CPoint m_lastMousePoint;

	std::optional<arteststudio::domain::ConnectionId> m_rightClickConnectionId;

	std::optional<arteststudio::domain::Node> m_clipboardNode;
	CPoint m_lastRightClickPoint;

#ifdef _DEBUG
	CPoint m_mousePosition;
	CFont m_smallFont;
	BOOL m_fontInitialized = false;
#endif

protected:
	CARTestStudioView();
	DECLARE_DYNCREATE(CARTestStudioView)

public:
	CARTestStudioDoc* GetDocument() const;

	virtual void OnDraw(CDC* pDC);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

protected:
	virtual int OnCreate(LPCREATESTRUCT lpCreateStruct);
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

public:
	virtual ~CARTestStudioView();

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	virtual DROPEFFECT OnDragEnter(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
	virtual DROPEFFECT OnDragOver(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
	virtual BOOL OnDrop(COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);

protected:
	afx_msg void OnFilePrintPreview();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnDestroy();

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnEditDelete();
	afx_msg void OnEditCopy();
	afx_msg void OnEditCut();
	afx_msg void OnEditPaste();
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG
inline CARTestStudioDoc* CARTestStudioView::GetDocument() const
{
	return reinterpret_cast<CARTestStudioDoc*>(m_pDocument);
}
#endif
