#include "winframe.h"

/*
 *  CWindowFrame
 *
 *  Implements the frame around windows: contains one or more child windows
 *  and displays their title bar(s).
 *
 */

// width/height of a border
#define BORDER_SIZE     4
// height of the caption
#define CAPTION_HEIGHT  14
// width/height of a diagonal sizing grip
#define GRIP_SIZE       (BORDER_SIZE + CAPTION_HEIGHT)

CWindowFrame::CWindowFrame(const windowdef_t* def, IWindowServer* srv) 
	: CWindow(def, srv)
{
	//m_bDrag = false;
}

/*WindowHit CWindowFrame::HitTest(int x, int y)
{
	Rectangle rect = GetPosition();

	if (!rect.PtInRect(x, y))
		return hitNone;

	//if (GetClient().PtInRect(x, y))
		//return hitClient;

	x -= rect.left;
	y -= rect.top;
	rect.Offset(-rect.left, -rect.top);

	if (x < BORDER_SIZE)
	{
		if (y < GRIP_SIZE)
			return hitTopLeft;
		else if (y >= rect.Height() - GRIP_SIZE)
			return hitBottomLeft;
		else
			return hitLeft;
	}

	if (x >= rect.Width() - BORDER_SIZE)
	{
		if (y < GRIP_SIZE)
			return hitTopRight;
		else if (y >= rect.Height() - GRIP_SIZE)
			return hitBottomRight;
		else
			return hitRight;
	}

	if (y < BORDER_SIZE)
	{
		if (x < GRIP_SIZE)
			return hitTopLeft;
		else if (x >= rect.Width() - GRIP_SIZE)
			return hitTopRight;
		else
			return hitTop;
	}

	if (y >= rect.Height() - BORDER_SIZE)
	{
		if (x < GRIP_SIZE)
			return hitBottomLeft;
		else if (x >= rect.Width() - GRIP_SIZE)
			return hitBottomRight;
		else
			return hitBottom;
	}

	rect.Inflate(-BORDER_SIZE, -BORDER_SIZE);
	rect.bottom = rect.top + CAPTION_HEIGHT;
	if (rect.PtInRect(x, y))
		return hitCaption;

	return hitNone;
}

bool CWindowFrame::PreDispatchMessage(Message* pMsg)
{
	MouseStat* pStat = (MouseStat*) (pMsg + 1);
	ISurface::DrawMode oldMode;

	if (m_bDrag)
	{
		if (pMsg->dwMessage >= WM_LBUTTONDOWN &&
			pMsg->dwMessage <= WM_MOUSEWHEEL)
		{
			switch (pMsg->dwMessage)
			{
			case WM_MOUSEMOVE:
				oldMode = g_pScreen->SetDrawMode(ISurface::modeNot);
				ConditShowCursor(m_rectDrag, false);
				g_pScreen->Rect(m_rectDrag, 0, 1);
				m_rectDrag.Offset(pStat->nXPos - m_ptDrag.x, pStat->nYPos - m_ptDrag.y);
				g_pScreen->Rect(m_rectDrag, 0, 1);
				ConditShowCursor(m_rectDrag, true);
				g_pScreen->SetDrawMode(oldMode);
				m_ptDrag = Point(pStat->nXPos, pStat->nYPos);
				break;
			case WM_LBUTTONUP:
				oldMode = g_pScreen->SetDrawMode(ISurface::modeNot);
				ConditShowCursor(m_rectDrag, false);
				g_pScreen->Rect(m_rectDrag, 0, 1);
				ConditShowCursor(m_rectDrag, true);
				g_pScreen->SetDrawMode(oldMode);

				m_bDrag = false;
				SetCapture(NULL);
				MoveWindow(m_rectDrag);
				break;
			}

			return false;
		}
	}

	return true;
}

void CWindowFrame::OnMessage(const Message* pMsg)
{
	MouseStat* pStat = (MouseStat*) (pMsg + 1);
	WindowHit nHit;
	ISurface::DrawMode oldMode;

	switch (pMsg->dwMessage)
	{
	case WM_LBUTTONDOWN:
		Window::OnMessage(pMsg);
		nHit = HitTest(pStat->nXPos, pStat->nYPos);
		if (nHit == hitCaption)
		{
			m_bDrag = true;
			m_rectDrag = GetPosition();
			m_ptDrag = Point(pStat->nXPos, pStat->nYPos);
			SetCapture(this);

			oldMode = g_pScreen->SetDrawMode(ISurface::modeNot);
			ConditShowCursor(m_rectDrag, false);
			g_pScreen->Rect(m_rectDrag, 0, 1);
			ConditShowCursor(m_rectDrag, true);
			g_pScreen->SetDrawMode(oldMode);
		}
		break;
	default:
		Window::OnMessage(pMsg);
	}
}*/

void CWindowFrame::RemoveChild(CWindow* child)
{
	CWindow::RemoveChild(child);

	if (m_first == NULL && m_last == NULL)
		Release();
}

/*
 * Code for Windows 95-style frame.
 * Boring.
 */

#if 0

void CWindowFrame::AdjustForFrame(rectangle_t* rect)
{
	rect->InflateRect(-BORDER_SIZE, -BORDER_SIZE);
	rect->top += CAPTION_HEIGHT;
}

void CWindowFrame::OnPaint(ISurface* pSurf, const rectangle_t* rectPaint)
{
	rectangle_t rect = *this;
	IFont* pFont = m_font;
	int y;
	point_t size;

	pSurf->Rect3d(rect,
		pSurf->ColourMatch(0xc0c0c0),
		pSurf->ColourMatch(0x000000), 1);
	rect.InflateRect(-1, -1);
	pSurf->Rect3d(rect,
		pSurf->ColourMatch(0xffffff),
		pSurf->ColourMatch(0x808080), 1);
	rect.InflateRect(-1, -1);
	rect.top += CAPTION_HEIGHT;
	pSurf->Rect3d(rect,
		pSurf->ColourMatch(0x808080),
		pSurf->ColourMatch(0xffffff), 1);
	rect.InflateRect(-1, -1);
	pSurf->Rect3d(rect,
		pSurf->ColourMatch(0x000000),
		pSurf->ColourMatch(0xc0c0c0), 1);
	rect.InflateRect(1, 1);
	rect.top -= CAPTION_HEIGHT;
	pSurf->FillRect(
		rectangle_t(rect.left, rect.top, rect.right, rect.top + CAPTION_HEIGHT),
		pSurf->ColourMatch(0x000080));

	pFont->GetTextExtent(m_title, &size);
	y = (CAPTION_HEIGHT - size.y) / 2;
	pFont->DrawText(pSurf, rect.left + y, rect.top + y, m_title,
		pSurf->ColourMatch(0xffffff));
}

#else

/*
 * Trendy BeOS-style active tab title bar.
 * Cool.
 */

void CWindowFrame::AdjustForFrame(rectangle_t* rect)
{
	point_t pt;
	rect->InflateRect(-BORDER_SIZE, -BORDER_SIZE);
	m_font->GetTextExtent(m_title, &pt);
	rect->top += 4 + pt.y;
}

void CWindowFrame::OnPaint(ISurface* pSurf, const rectangle_t* rectPaint)
{
	rectangle_t rc = *this;
	CWindow* pChild;
	point_t pt;

	m_font->GetTextExtent(m_title, &pt);
	
	rc.top += 4 + pt.y;
	pSurf->Rect(rc, pSurf->ColourMatch(0x808080), 1);
	rc.InflateRect(-1, -1);
	pSurf->Rect(rc, pSurf->ColourMatch(0xc0c0c0), 1);
	rc.InflateRect(-1, -1);
	pSurf->Rect3d(rc, pSurf->ColourMatch(0x808080), 
		pSurf->ColourMatch(0xffffff), 1);
	rc.InflateRect(-1, -1);
	pSurf->Rect(rc, pSurf->ColourMatch(0xc0c0c0), 1);

	rc = *this;
	rc.bottom = rc.top + pt.y + 5;

	for (pChild = m_first; pChild; pChild = pChild->m_next)
	{
		m_font->GetTextExtent(pChild->m_title, &pt);
		rc.right = rc.left + pt.x + 6;
		pSurf->FillRect(rc, pSurf->ColourMatch(0xffff00));
		pSurf->Rect(rc, pSurf->ColourMatch(0x808000), 1);
		m_font->DrawText(pSurf, rc.left + 3, rc.top + 2, pChild->m_title, 0);
		rc.left = rc.right + 1;
	}
}

#endif
