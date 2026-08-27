#include "pch.h"
#include "NodeShape.h"
#include <cmath>



// ----------------------------------------------------------------
// CDiamondNode implementation
// ----------------------------------------------------------------
CDiamondNode::CDiamondNode(int X, int Y, const CString& label) :
    m_X(X),
    m_Y(Y),
    labelText(label)
{
    center.x = m_X;
    center.y = m_Y;
    m_W = 50;
    m_H = 40;
}

void CDiamondNode::Draw(CDC* pDC)
{
    POINT pts[4] =
    {
         { center.x, center.y - m_H },
         { center.x + m_W, center.y  },
         { center.x, center.y + m_H },
         { center.x - m_W, center.y  }
    };
    pDC->Polygon(pts, 4);

    // Opcional: dibujar círculos pequeños en los puntos de conexión para visualizarlos
    for (int i = 0; i < 4; ++i)
    {
        CPoint pt = GetConnectionPoint(i);
        pDC->Ellipse(pt.x - 3, pt.y - 3, pt.x + 3, pt.y + 3);
    }
}

CPoint CDiamondNode::GetConnectionPoint(int index) const
{
    switch (index)
    {
    case 0: return CPoint(center.x, center.y - m_H);
    case 1: return CPoint(center.x + m_W, center.y);
    case 2: return CPoint(center.x, center.y + m_H);
    case 3: return CPoint(center.x - m_W, center.y);
    default: return center;
    }
}

bool CDiamondNode::HitTest(CPoint pt) const
{
    // Usamos el rectángulo contenedor para simplificar la prueba
    CRect bounds(center.x - m_W, center.y - m_H, center.x + m_W, center.y + m_H);
    return bounds.PtInRect(pt);
}

void CDiamondNode::MoveBy(int dx, int dy)
{
    center.x += dx;
    center.y += dy;
}

void CDiamondNode::MoveTo(int dx, int dy)
{
    center.x = dx;
    center.y = dy;
}



// ----------------------------------------------------------------
// CRectangleNode implementation
// ----------------------------------------------------------------
CRectangleNode::CRectangleNode(int X, int Y, const CString& label) :
    m_X(X),
    m_Y(Y),
    labelText(label)
{
    rect = CRect(m_X, m_Y, m_X + 150, m_Y + 100);
}

void CRectangleNode::Draw(CDC* pDC)
{
    CBrush normalBrush(RGB(200, 200, 255));
    CBrush selectedBrush(RGB(100, 150, 255));

    CBrush* pOldBrush = pDC->SelectObject(isSelected ? &selectedBrush : &normalBrush);

    CPen borderPen(PS_SOLID, isSelected ? 2 : 1, isSelected ? RGB(0, 0, 255) : RGB(0, 0, 0));
    CPen* pOldPen = pDC->SelectObject(&borderPen);

    pDC->Rectangle(&rect);
    pDC->DrawText(labelText, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Dibujar puntos de conexión
    for (int i = 0; i < 4; ++i)
    {
        CPoint pt = GetConnectionPoint(i);
        pDC->Ellipse(pt.x - 3, pt.y - 3, pt.x + 3, pt.y + 3);
    }

    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);
}

CPoint CRectangleNode::GetConnectionPoint(int index) const
{
    switch (index)
    {
    case 0: return CPoint((rect.left + rect.right) / 2, rect.top);
    case 1: return CPoint(rect.right, (rect.top + rect.bottom) / 2);
    case 2: return CPoint((rect.left + rect.right) / 2, rect.bottom);
    case 3: return CPoint(rect.left, (rect.top + rect.bottom) / 2);
    default: return rect.CenterPoint();
    }
}

bool CRectangleNode::HitTest(CPoint pt) const
{
    return rect.PtInRect(pt);
}

void CRectangleNode::MoveBy(int dx, int dy)
{
    rect.OffsetRect(dx, dy);
}

void CRectangleNode::MoveTo(int dx, int dy)
{
    POINT p;
    p.x = dx;
    p.y = dy;
    rect.MoveToXY(p);
}
