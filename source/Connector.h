#pragma once
#include <afxwin.h>
#include "Block.h"


class CConnector
{
public:
	CBlock* m_pSource;
	CBlock* m_pTarget;

	CConnector(CBlock* pSource, CBlock* pTarget)
		: m_pSource(pSource), m_pTarget(pTarget) {}

	void Draw(CDC* pDC)
	{
		if (m_pSource && m_pTarget)
		{
			CPoint ptSource = m_pSource->m_Rect.CenterPoint();
			CPoint ptTarget = m_pTarget->m_Rect.CenterPoint();
			//CPoint ptTarget = m_pTarget->m_Rect.
			pDC->MoveTo(ptSource);
			pDC->LineTo(ptTarget);
		}
	}
};