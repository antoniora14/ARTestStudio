#pragma once
#include <afxwin.h>


class CBlock
{
public:
	CRect	m_Rect;
	CString m_label;
	CPoint	m_pt;
	BOOL	m_isSelected;

	CBlock(const CRect& rect, const CString& label)
		: m_Rect(rect), m_label(label), m_isSelected(false) {}

	void Draw(CDC* pDC)
	{
		pDC->Rectangle(m_Rect);
		pDC->DrawText(m_label, m_Rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	BOOL HitTest(const CPoint& pt) const { return m_Rect.PtInRect(pt); }
};