// ARTestStudioView.cpp : implementation of the CARTestStudioView class

#include "pch.h"
#include "framework.h"

#ifndef SHARED_HANDLERS
#include "ARTestStudio.h"
#endif

#include "ARTestStudioDoc.h"
#include "ARTestStudioView.h"
#include "Domain/DiagramGeometry.h"
#include "Domain/OrthogonalRouter.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using arteststudio::domain::Connection;
using arteststudio::domain::ConnectionEndpoint;
using arteststudio::domain::ConnectionId;
using arteststudio::domain::DiagramError;
using arteststudio::domain::DiagramModel;
using arteststudio::domain::Node;
using arteststudio::domain::NodeId;
using arteststudio::domain::NodeKind;
using arteststudio::domain::OrthogonalRouter;
using arteststudio::domain::Point;
using arteststudio::domain::PortId;

namespace
{
	constexpr int ConnectionPointCount = 4;
	constexpr int ConnectionHitRadius = 5;
	constexpr int ConnectionLineTolerance = 6;

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

	bool HitTestNode(const Node& node, CPoint point)
	{
		const CRect bounds = GetMfcNodeBounds(node);
		if (node.kind != NodeKind::Diamond)
		{
			return bounds.PtInRect(point) == TRUE;
		}

		const double halfWidth = static_cast<double>(node.width) / 2.0;
		const double halfHeight = static_cast<double>(node.height) / 2.0;
		if (halfWidth <= 0.0 || halfHeight <= 0.0)
		{
			return false;
		}

		const double normalizedX = std::abs(point.x - node.position.x) / halfWidth;
		const double normalizedY = std::abs(point.y - node.position.y) / halfHeight;
		return normalizedX + normalizedY <= 1.0;
	}

	void DrawNode(CDC* deviceContext, const Node& node)
	{
		CBrush fillBrush(RGB(200, 200, 255));
		CPen borderPen(PS_SOLID, 1, RGB(0, 0, 0));
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

	void DrawConnection(CDC* deviceContext, const DiagramModel& diagram, const Connection& connection)
	{
		CPoint start;
		CPoint end;
		if (!TryGetConnectionPoint(diagram, connection.from, start) ||
			!TryGetConnectionPoint(diagram, connection.to, end))
		{
			return;
		}

		CPen pen(PS_SOLID, 1, RGB(0, 0, 0));
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

	double DistancePointToSegment(CPoint point, CPoint start, CPoint end)
	{
		const double dx = static_cast<double>(end.x - start.x);
		const double dy = static_cast<double>(end.y - start.y);
		if (dx == 0.0 && dy == 0.0)
		{
			return std::hypot(
				static_cast<double>(point.x - start.x),
				static_cast<double>(point.y - start.y));
		}

		const double projection = std::clamp(
			((point.x - start.x) * dx + (point.y - start.y) * dy) / (dx * dx + dy * dy),
			0.0,
			1.0);
		const double projectedX = start.x + projection * dx;
		const double projectedY = start.y + projection * dy;
		return std::hypot(point.x - projectedX, point.y - projectedY);
	}

	bool HitTestConnection(const DiagramModel& diagram, const Connection& connection, CPoint point)
	{
		CPoint start;
		CPoint end;
		if (!TryGetConnectionPoint(diagram, connection.from, start) ||
			!TryGetConnectionPoint(diagram, connection.to, end))
		{
			return false;
		}

		CPoint segmentStart = start;
		for (const Point intermediate : connection.intermediatePoints)
		{
			const CPoint segmentEnd = ToMfcPoint(intermediate);
			if (DistancePointToSegment(point, segmentStart, segmentEnd) <= ConnectionLineTolerance)
			{
				return true;
			}
			segmentStart = segmentEnd;
		}

		return DistancePointToSegment(point, segmentStart, end) <= ConnectionLineTolerance;
	}

	struct PortHit
	{
		NodeId nodeId;
		PortId portId = PortId::Top;
		CPoint point;
	};

	std::optional<PortHit> FindPortAt(const DiagramModel& diagram, CPoint point)
	{
		for (const Node& node : diagram.Nodes())
		{
			for (int index = 0; index < ConnectionPointCount; ++index)
			{
				const PortId portId = static_cast<PortId>(index);
				const CPoint connectionPoint = GetMfcConnectionPoint(node, portId);
				if (std::abs(connectionPoint.x - point.x) <= ConnectionHitRadius &&
					std::abs(connectionPoint.y - point.y) <= ConnectionHitRadius)
				{
					return PortHit{node.id, portId, connectionPoint};
				}
			}
		}
		return std::nullopt;
	}

	std::optional<NodeId> FindNodeAt(const DiagramModel& diagram, CPoint point)
	{
		for (auto node = diagram.Nodes().crbegin(); node != diagram.Nodes().crend(); ++node)
		{
			if (HitTestNode(*node, point))
			{
				return node->id;
			}
		}
		return std::nullopt;
	}

	std::optional<ConnectionId> FindConnectionAt(const DiagramModel& diagram, CPoint point)
	{
		for (const Connection& connection : diagram.Connections())
		{
			if (HitTestConnection(diagram, connection, point))
			{
				return connection.id;
			}
		}
		return std::nullopt;
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
	for (const Node& node : diagram.Nodes())
	{
		DrawNode(drawingContext, node);
	}

	for (const Connection& connection : diagram.Connections())
	{
		DrawConnection(drawingContext, diagram, connection);
	}

	if (m_drawingLine && m_startNodeId.has_value())
	{
		const Node* startNode = diagram.FindNode(*m_startNodeId);
		if (startNode != nullptr)
		{
			DrawArrow(
				drawingContext,
				GetMfcConnectionPoint(*startNode, m_startPortId),
				m_tempEndPoint,
				RGB(70, 100, 160));
		}
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
	DiagramModel& diagram = document->GetDiagram();

	if (m_drawingLine)
	{
		m_tempEndPoint = point;
		Invalidate(FALSE);
	}
	else if (m_draggingNodeId.has_value())
	{
		const int dx = point.x - m_lastMousePoint.x;
		const int dy = point.y - m_lastMousePoint.y;
		m_lastMousePoint = point;
		if (diagram.MoveNodeBy(*m_draggingNodeId, dx, dy) == DiagramError::None)
		{
			OrthogonalRouter::RouteAll(diagram);
			MarkDiagramChanged(*document);
		}
	}

#ifdef _DEBUG
	m_mousePosition = point;
	Invalidate(FALSE);
#endif

	CView::OnMouseMove(flags, point);
}

void CARTestStudioView::OnLButtonDown(UINT flags, CPoint point)
{
	DiagramModel& diagram = GetDocument()->GetDiagram();
	if (const std::optional<PortHit> port = FindPortAt(diagram, point); port.has_value())
	{
		m_drawingLine = true;
		m_startNodeId = port->nodeId;
		m_startPortId = port->portId;
		m_tempEndPoint = port->point;
		SetCapture();
		Invalidate(FALSE);
		return;
	}

	if (const std::optional<NodeId> nodeId = FindNodeAt(diagram, point); nodeId.has_value())
	{
		m_draggingNodeId = nodeId;
		m_lastMousePoint = point;
		SetCapture();
		return;
	}

	CView::OnLButtonDown(flags, point);
}

void CARTestStudioView::OnLButtonUp(UINT flags, CPoint point)
{
	CARTestStudioDoc* document = GetDocument();
	DiagramModel& diagram = document->GetDiagram();

	if (m_drawingLine && m_startNodeId.has_value())
	{
		if (const std::optional<PortHit> target = FindPortAt(diagram, point); target.has_value())
		{
			const auto added = diagram.AddConnection(
				{*m_startNodeId, m_startPortId},
				{target->nodeId, target->portId});
			if (added)
			{
				(void)OrthogonalRouter::RouteConnection(diagram, added.connectionId);
				MarkDiagramChanged(*document);
			}
		}

		m_drawingLine = false;
		m_startNodeId.reset();
		Invalidate(FALSE);
	}

	m_draggingNodeId.reset();
	if (GetCapture() == this)
	{
		ReleaseCapture();
	}

	CView::OnLButtonUp(flags, point);
}

void CARTestStudioView::OnRButtonDown(UINT flags, CPoint point)
{
	const DiagramModel& diagram = GetDocument()->GetDiagram();
	m_lastRightClickPoint = point;
	m_rightClickNodeId = FindNodeAt(diagram, point);
	m_rightClickConnectionId.reset();
	if (!m_rightClickNodeId.has_value())
	{
		m_rightClickConnectionId = FindConnectionAt(diagram, point);
	}

	CView::OnRButtonDown(flags, point);
}

void CARTestStudioView::OnEditCopy()
{
	if (!m_rightClickNodeId.has_value())
	{
		return;
	}

	const Node* node = GetDocument()->GetDiagram().FindNode(*m_rightClickNodeId);
	if (node != nullptr)
	{
		m_clipboardNode = *node;
	}
}

void CARTestStudioView::OnEditCut()
{
	OnEditCopy();
	OnEditDelete();
}

void CARTestStudioView::OnEditPaste()
{
	if (!m_clipboardNode.has_value())
	{
		return;
	}

	CARTestStudioDoc* document = GetDocument();
	const Node& copied = *m_clipboardNode;
	(void)document->GetDiagram().AddNode(
		copied.kind,
		ToDomainPoint(m_lastRightClickPoint),
		copied.width,
		copied.height,
		copied.label);
	OrthogonalRouter::RouteAll(document->GetDiagram());
	MarkDiagramChanged(*document);
}

void CARTestStudioView::OnEditDelete()
{
	CARTestStudioDoc* document = GetDocument();
	DiagramModel& diagram = document->GetDiagram();
	DiagramError result = DiagramError::None;
	bool attempted = false;

	if (m_rightClickNodeId.has_value())
	{
		attempted = true;
		result = diagram.RemoveNode(*m_rightClickNodeId);
	}
	else if (m_rightClickConnectionId.has_value())
	{
		attempted = true;
		result = diagram.RemoveConnection(*m_rightClickConnectionId);
	}

	if (attempted && result == DiagramError::None)
	{
		OrthogonalRouter::RouteAll(diagram);
		MarkDiagramChanged(*document);
	}

	m_rightClickNodeId.reset();
	m_rightClickConnectionId.reset();
	Invalidate(FALSE);
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
		(void)document->GetDiagram().AddNode(NodeKind::Rectangle, ToDomainPoint(point), text);
		OrthogonalRouter::RouteAll(document->GetDiagram());
		MarkDiagramChanged(*document);
		GlobalUnlock(medium.hGlobal);
		succeeded = TRUE;
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
#endif

#ifdef _DEBUG
CARTestStudioDoc* CARTestStudioView::GetDocument() const
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CARTestStudioDoc)));
	return static_cast<CARTestStudioDoc*>(m_pDocument);
}
#endif
