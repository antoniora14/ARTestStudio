// ARTestStudioView.cpp : implementation of the CARTestStudioView class

#include "pch.h"
#include "framework.h"

#ifndef SHARED_HANDLERS
#include "ARTestStudio.h"
#endif

#include "ARTestStudioDoc.h"
#include "ARTestStudioView.h"
#include "Application/InteractionController.h"
#include "Domain/DiagramGeometry.h"

#include <cmath>
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using arteststudio::application::InteractionController;
using arteststudio::application::SelectionModel;
using arteststudio::domain::Connection;
using arteststudio::domain::ConnectionEndpoint;
using arteststudio::domain::DiagramModel;
using arteststudio::domain::Node;
using arteststudio::domain::NodeKind;
using arteststudio::domain::Point;
using arteststudio::domain::PortId;

namespace
{
	constexpr int ConnectionPointCount = 4;

	void MarkDiagramChanged(CARTestStudioDoc& document)
	{
		document.SetModifiedFlag(TRUE);
		document.UpdateAllViews(nullptr);
	}

	CPoint ToMfcPoint(Point point)
	{
		return {point.x, point.y};
	}

	Point ToDomainPoint(CPoint point)
	{
		return {point.x, point.y};
	}

	CRect GetMfcNodeBounds(const Node& node)
	{
		const arteststudio::domain::Rect bounds = arteststudio::domain::GetNodeBounds(node);
		return {bounds.left, bounds.top, bounds.right, bounds.bottom};
	}

	CPoint GetMfcConnectionPoint(const Node& node, PortId portId)
	{
		return ToMfcPoint(arteststudio::domain::GetConnectionPoint(node, portId));
	}

	bool TryGetConnectionPoint(
		const DiagramModel& diagram,
		const ConnectionEndpoint& endpoint,
		CPoint& point)
	{
		const Node* node = diagram.FindNode(endpoint.nodeId);
		if (node == nullptr)
		{
			return false;
		}

		point = GetMfcConnectionPoint(*node, endpoint.portId);
		return true;
	}

	void DrawNode(CDC* deviceContext, const Node& node, bool selected)
	{
		CBrush fillBrush(RGB(200, 200, 255));
		CPen borderPen(
			PS_SOLID,
			selected ? 2 : 1,
			selected ? RGB(35, 105, 190) : RGB(0, 0, 0));
		CBrush* oldBrush = deviceContext->SelectObject(&fillBrush);
		CPen* oldPen = deviceContext->SelectObject(&borderPen);
		const CRect bounds = GetMfcNodeBounds(node);

		if (node.kind == NodeKind::Diamond)
		{
			POINT points[4]{
				{node.position.x, bounds.top},
				{bounds.right, node.position.y},
				{node.position.x, bounds.bottom},
				{bounds.left, node.position.y}};
			deviceContext->Polygon(points, 4);
		}
		else
		{
			deviceContext->Rectangle(&bounds);
		}

		CString label(node.label.c_str());
		CRect textBounds = bounds;
		deviceContext->SetBkMode(TRANSPARENT);
		deviceContext->DrawText(label, textBounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		for (int index = 0; index < ConnectionPointCount; ++index)
		{
			const CPoint connectionPoint = GetMfcConnectionPoint(node, static_cast<PortId>(index));
			deviceContext->Ellipse(
				connectionPoint.x - 3,
				connectionPoint.y - 3,
				connectionPoint.x + 3,
				connectionPoint.y + 3);
		}

		deviceContext->SelectObject(oldBrush);
		deviceContext->SelectObject(oldPen);
	}

	void DrawArrowHead(CDC* deviceContext, const CPoint& from, const CPoint& to)
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double ArrowLength = 10.0;
		const double angle = std::atan2(
			static_cast<double>(to.y - from.y),
			static_cast<double>(to.x - from.x));

		const CPoint left(
			static_cast<int>(to.x - ArrowLength * std::cos(angle - Pi / 6.0)),
			static_cast<int>(to.y - ArrowLength * std::sin(angle - Pi / 6.0)));
		const CPoint right(
			static_cast<int>(to.x - ArrowLength * std::cos(angle + Pi / 6.0)),
			static_cast<int>(to.y - ArrowLength * std::sin(angle + Pi / 6.0)));

		deviceContext->MoveTo(to);
		deviceContext->LineTo(left);
		deviceContext->MoveTo(to);
		deviceContext->LineTo(right);
	}

	void DrawArrow(CDC* deviceContext, CPoint start, CPoint end, COLORREF color)
	{
		CPen pen(PS_SOLID, 1, color);
		CPen* oldPen = deviceContext->SelectObject(&pen);
		deviceContext->MoveTo(start);
		deviceContext->LineTo(end);
		DrawArrowHead(deviceContext, start, end);
		deviceContext->SelectObject(oldPen);
	}

	void DrawConnection(
		CDC* deviceContext,
		const DiagramModel& diagram,
		const Connection& connection,
		bool selected)
	{
		CPoint start;
		CPoint end;
		if (!TryGetConnectionPoint(diagram, connection.from, start) ||
			!TryGetConnectionPoint(diagram, connection.to, end))
		{
			return;
		}

		CPen pen(
			PS_SOLID,
			selected ? 2 : 1,
			selected ? RGB(35, 105, 190) : RGB(0, 0, 0));
		CPen* oldPen = deviceContext->SelectObject(&pen);
		std::vector<CPoint> points{start};
		points.reserve(connection.intermediatePoints.size() + 2);
		for (const Point point : connection.intermediatePoints)
		{
			points.push_back(ToMfcPoint(point));
		}
		points.push_back(end);

		for (std::size_t index = 0; index + 1 < points.size(); ++index)
		{
			deviceContext->MoveTo(points[index]);
			deviceContext->LineTo(points[index + 1]);
		}

		DrawArrowHead(deviceContext, points[points.size() - 2], end);
		deviceContext->SelectObject(oldPen);
	}
}

IMPLEMENT_DYNCREATE(CARTestStudioView, CView)

BEGIN_MESSAGE_MAP(CARTestStudioView, CView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CARTestStudioView::OnFilePrintPreview)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_EDIT_COPY, &CARTestStudioView::OnEditCopy)
	ON_COMMAND(ID_EDIT_CUT, &CARTestStudioView::OnEditCut)
	ON_COMMAND(ID_EDIT_PASTE, &CARTestStudioView::OnEditPaste)
	ON_COMMAND(ID_EDIT_DELETE, &CARTestStudioView::OnEditDelete)
	ON_COMMAND(ID_EDIT_UNDO, &CARTestStudioView::OnEditUndo)
	ON_COMMAND(ID_EDIT_REDO, &CARTestStudioView::OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, &CARTestStudioView::OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, &CARTestStudioView::OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, &CARTestStudioView::OnUpdateEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, &CARTestStudioView::OnUpdateEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, &CARTestStudioView::OnUpdateEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, &CARTestStudioView::OnUpdateEditRedo)
END_MESSAGE_MAP()

CARTestStudioView::CARTestStudioView() = default;

CARTestStudioView::~CARTestStudioView() = default;

BOOL CARTestStudioView::PreCreateWindow(CREATESTRUCT& createStruct)
{
	return CView::PreCreateWindow(createStruct);
}

void CARTestStudioView::OnDraw(CDC* deviceContext)
{
	CARTestStudioDoc* document = GetDocument();
	ASSERT_VALID(document);
	if (document == nullptr)
	{
		return;
	}

	CMemDC memoryDeviceContext(*deviceContext, this);
	CDC* drawingContext = &memoryDeviceContext.GetDC();
	if (!deviceContext->IsPrinting())
	{
		CRect clientBounds;
		GetClientRect(&clientBounds);
		drawingContext->FillSolidRect(&clientBounds, ::GetSysColor(COLOR_WINDOW));
	}

	const DiagramModel& diagram = document->GetDiagram();
	const SelectionModel& selection = document->GetInteractionController().Selection();
	for (const Node& node : diagram.Nodes())
	{
		DrawNode(drawingContext, node, selection.IsSelected(node.id));
	}

	for (const Connection& connection : diagram.Connections())
	{
		DrawConnection(drawingContext, diagram, connection, selection.IsSelected(connection.id));
	}

	const InteractionController& controller = document->GetInteractionController();
	const auto previewStart = controller.ConnectionPreviewStart();
	const auto previewEnd = controller.ConnectionPreviewEnd();
	if (previewStart.has_value() && previewEnd.has_value())
	{
		DrawArrow(
			drawingContext,
			ToMfcPoint(*previewStart),
			ToMfcPoint(*previewEnd),
			RGB(70, 100, 160));
	}

#ifdef _DEBUG
	if (!m_fontInitialized)
	{
		m_smallFont.CreatePointFont(80, _T("Segoe UI"));
		m_fontInitialized = true;
	}

	CString coordinates;
	coordinates.Format(_T("X: %d, Y: %d"), m_mousePosition.x, m_mousePosition.y);
	CFont* oldFont = drawingContext->SelectObject(&m_smallFont);
	const CSize textSize = drawingContext->GetTextExtent(coordinates);
	CRect bounds(
		m_mousePosition.x + 10,
		m_mousePosition.y + 20,
		m_mousePosition.x + 18 + textSize.cx,
		m_mousePosition.y + 24 + textSize.cy);
	CBrush brush(RGB(245, 245, 220));
	drawingContext->FillRect(&bounds, &brush);
	drawingContext->DrawEdge(&bounds, EDGE_RAISED, BF_RECT);
	drawingContext->SetBkMode(TRANSPARENT);
	drawingContext->TextOut(bounds.left + 4, bounds.top + 2, coordinates);
	drawingContext->SelectObject(oldFont);
#endif
}

void CARTestStudioView::OnMouseMove(UINT flags, CPoint point)
{
	CARTestStudioDoc* document = GetDocument();
	InteractionController& controller = document->GetInteractionController();
	const bool wasActive = controller.IsPointerActionActive();
	if (controller.UpdatePrimaryAction(ToDomainPoint(point)))
	{
		MarkDiagramChanged(*document);
	}
	else if (wasActive)
	{
		Invalidate(FALSE);
	}

#ifdef _DEBUG
	m_mousePosition = point;
	Invalidate(FALSE);
#endif

	CView::OnMouseMove(flags, point);
}

void CARTestStudioView::OnLButtonDown(UINT flags, CPoint point)
{
	InteractionController& controller = GetDocument()->GetInteractionController();
	controller.BeginPrimaryAction(ToDomainPoint(point));
	if (controller.IsPointerActionActive())
	{
		SetCapture();
	}
	Invalidate(FALSE);
	CView::OnLButtonDown(flags, point);
}

void CARTestStudioView::OnLButtonUp(UINT flags, CPoint point)
{
	CARTestStudioDoc* document = GetDocument();
	if (document->GetInteractionController().EndPrimaryAction(ToDomainPoint(point)))
	{
		MarkDiagramChanged(*document);
	}
	else
	{
		Invalidate(FALSE);
	}

	if (GetCapture() == this)
	{
		ReleaseCapture();
	}
	CView::OnLButtonUp(flags, point);
}

void CARTestStudioView::OnRButtonDown(UINT flags, CPoint point)
{
	GetDocument()->GetInteractionController().SelectAt(ToDomainPoint(point));
	Invalidate(FALSE);
	CView::OnRButtonDown(flags, point);
}

void CARTestStudioView::OnEditCopy()
{
	(void)GetDocument()->GetInteractionController().CopySelection();
}

void CARTestStudioView::OnEditCut()
{
	CARTestStudioDoc* document = GetDocument();
	if (document->GetInteractionController().CutSelection())
	{
		MarkDiagramChanged(*document);
	}
}

void CARTestStudioView::OnEditPaste()
{
	CARTestStudioDoc* document = GetDocument();
	if (document->GetInteractionController().Paste())
	{
		MarkDiagramChanged(*document);
	}
}

void CARTestStudioView::OnEditDelete()
{
	CARTestStudioDoc* document = GetDocument();
	if (document->GetInteractionController().DeleteSelection())
	{
		MarkDiagramChanged(*document);
	}
}

void CARTestStudioView::OnEditUndo()
{
	CARTestStudioDoc* document = GetDocument();
	if (document->GetInteractionController().Undo())
	{
		MarkDiagramChanged(*document);
	}
}

void CARTestStudioView::OnEditRedo()
{
	CARTestStudioDoc* document = GetDocument();
	if (document->GetInteractionController().Redo())
	{
		MarkDiagramChanged(*document);
	}
}

void CARTestStudioView::OnUpdateEditDelete(CCmdUI* commandUi)
{
	commandUi->Enable(GetDocument()->GetInteractionController().CanDelete());
}

void CARTestStudioView::OnUpdateEditCopy(CCmdUI* commandUi)
{
	commandUi->Enable(GetDocument()->GetInteractionController().CanCopy());
}

void CARTestStudioView::OnUpdateEditCut(CCmdUI* commandUi)
{
	commandUi->Enable(GetDocument()->GetInteractionController().CanCopy());
}

void CARTestStudioView::OnUpdateEditPaste(CCmdUI* commandUi)
{
	commandUi->Enable(GetDocument()->GetInteractionController().CanPaste());
}

void CARTestStudioView::OnUpdateEditUndo(CCmdUI* commandUi)
{
	commandUi->Enable(GetDocument()->GetInteractionController().CanUndo());
}

void CARTestStudioView::OnUpdateEditRedo(CCmdUI* commandUi)
{
	commandUi->Enable(GetDocument()->GetInteractionController().CanRedo());
}

void CARTestStudioView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
	AFXPrintPreview(this);
#endif
}

int CARTestStudioView::OnCreate(LPCREATESTRUCT createStruct)
{
	if (CView::OnCreate(createStruct) == -1)
	{
		return -1;
	}
	return m_dropTarget.Register(this) ? 0 : -1;
}

BOOL CARTestStudioView::OnPreparePrinting(CPrintInfo* printInfo)
{
	return DoPreparePrinting(printInfo);
}

void CARTestStudioView::OnBeginPrinting(CDC*, CPrintInfo*)
{
}

void CARTestStudioView::OnEndPrinting(CDC*, CPrintInfo*)
{
}

void CARTestStudioView::OnRButtonUp(UINT, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CARTestStudioView::OnContextMenu(CWnd*, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}

void CARTestStudioView::OnDestroy()
{
	m_dropTarget.Revoke();
	CView::OnDestroy();
}

DROPEFFECT CARTestStudioView::OnDragEnter(COleDataObject* dataObject, DWORD, CPoint)
{
	return dataObject != nullptr && dataObject->IsDataAvailable(CF_UNICODETEXT)
		? DROPEFFECT_COPY
		: DROPEFFECT_NONE;
}

DROPEFFECT CARTestStudioView::OnDragOver(COleDataObject* dataObject, DWORD, CPoint)
{
	return dataObject != nullptr && dataObject->IsDataAvailable(CF_UNICODETEXT)
		? DROPEFFECT_COPY
		: DROPEFFECT_NONE;
}

BOOL CARTestStudioView::OnDrop(COleDataObject* dataObject, DROPEFFECT, CPoint point)
{
	if (dataObject == nullptr || !dataObject->IsDataAvailable(CF_UNICODETEXT))
	{
		return FALSE;
	}

	STGMEDIUM medium{};
	if (!dataObject->GetData(CF_UNICODETEXT, &medium))
	{
		return FALSE;
	}

	BOOL succeeded = FALSE;
	const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(medium.hGlobal));
	if (text != nullptr)
	{
		CARTestStudioDoc* document = GetDocument();
		if (document->GetInteractionController().AddNode(
			NodeKind::Rectangle,
			ToDomainPoint(point),
			text))
		{
			MarkDiagramChanged(*document);
			succeeded = TRUE;
		}
		GlobalUnlock(medium.hGlobal);
	}

	ReleaseStgMedium(&medium);
	return succeeded;
}

#ifdef _DEBUG
void CARTestStudioView::AssertValid() const
{
	CView::AssertValid();
}

void CARTestStudioView::Dump(CDumpContext& dumpContext) const
{
	CView::Dump(dumpContext);
}

CARTestStudioDoc* CARTestStudioView::GetDocument() const
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CARTestStudioDoc)));
	return static_cast<CARTestStudioDoc*>(m_pDocument);
}
#endif
