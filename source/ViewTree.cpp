
#include "pch.h"
#include "framework.h"
#include "ViewTree.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CViewTree
IMPLEMENT_DYNAMIC(CViewTree, CTreeCtrl)


CViewTree::CViewTree() noexcept
{
}

CViewTree::~CViewTree()
{
}

BEGIN_MESSAGE_MAP(CViewTree, CTreeCtrl)
	ON_NOTIFY_REFLECT(TVN_BEGINDRAG, &CViewTree::OnBeginDrag)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CViewTree message handlers

BOOL CViewTree::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	BOOL bRes = CTreeCtrl::OnNotify(wParam, lParam, pResult);

	NMHDR* pNMHDR = (NMHDR*)lParam;
	ASSERT(pNMHDR != nullptr);

#pragma warning(suppress : 26454)
	if (pNMHDR && pNMHDR->code == TTN_SHOW && GetToolTips() != nullptr)
	{
		GetToolTips()->SetWindowPos(&wndTop, -1, -1, -1, -1, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
	}

	return bRes;
}

void CViewTree::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMTREEVIEW* pNMTreeView = reinterpret_cast<NMTREEVIEW*>(pNMHDR);
    HTREEITEM hItem = pNMTreeView->itemNew.hItem;
    if (hItem != nullptr)
    {
        CString strItemText = GetItemText(hItem);

        // Prepara el COleDataSource para iniciar el arrastre
        COleDataSource dataSource;

        // Usamos CF_UNICODETEXT para trabajar en Unicode
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (strItemText.GetLength() + 1) * sizeof(wchar_t));
        if (hGlobal)
        {
            wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hGlobal));
            if (pText)
            {
                wcscpy_s(pText, strItemText.GetLength() + 1, strItemText);
                GlobalUnlock(hGlobal);

                // Cacheamos los datos en el formato Unicode
                dataSource.CacheGlobalData(CF_UNICODETEXT, hGlobal);
                // Iniciamos la operación de drag & drop (solo copia en este ejemplo)
                dataSource.DoDragDrop(DROPEFFECT_COPY| DROPEFFECT_MOVE);
            }
        }
    }
    *pResult = 0;
}
