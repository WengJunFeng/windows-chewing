#include ".\imeui.h"
#include "ChewingIME.h"
#include "IMCLock.h"
#include "IMEUILock.h"

#include "CompStr.h"

#include "CompWnd.h"
#include "CandWnd.h"
#include "StatusWnd.h"
#include "XPToolbar.h"

#include <windows.h>
#include <multimon.h>

IMEUI::IMEUI(HWND hUIWnd) : hwnd(hUIWnd)
{
	fixedCompWndPos.x = fixedCompWndPos.y = 0;
}

IMEUI::~IMEUI(void)
{
}

LRESULT IMEUI::onIMENotify( HIMC hIMC, WPARAM wp , LPARAM lp )
{
    if ( hIMC==NULL )
    {
        return  0;
    }

	switch(wp)
	{
	case IMN_CLOSESTATUSWINDOW:
		closeStatusWnd();
		break;
	case IMN_OPENSTATUSWINDOW:
		openStatusWnd(hIMC);
		break;
	case IMN_OPENCANDIDATE:
		openCandWnd();
		break;
	case IMN_CHANGECANDIDATE:
		updateCandWnd();
		break;
	case IMN_CLOSECANDIDATE:
		closeCandWnd();
		break;
	case IMN_SETCANDIDATEPOS:
//		setCandWndPos(hIMC);
		break;
	case IMN_SETCONVERSIONMODE:
		statusWnd.updateIcons(hIMC);
		break;
	case IMN_SETSENTENCEMODE:
		break;
	case IMN_SETOPENSTATUS:
		break;
	case IMN_SETCOMPOSITIONFONT:
		{
			IMCLock imc(hIMC);
			if(imc.getIC())
				compWnd.setFont( (LOGFONT*)&imc.getIC()->lfFont );
		}
		break;
	case IMN_SETCOMPOSITIONWINDOW:
		{
	// The IMN_SETCOMPOSITIONWINDOW message is sent when the composition form of 
	// the Input Context is updated. When the UI window receives this message, 
	// the cfCompForm of the Input Context can be referenced to obtain the new 
	// conversion mode.
			POINT pt = getCompWndPos( IMCLock(hIMC) );
			if( !compWnd.isWindow() )
				compWnd.create(hwnd);
			compWnd.move(pt.x, pt.y);
		}
		break;
	case IMN_PRIVATE:
		{
			switch( lp )
			{
			case 0:
				{
					IMCLock imc(hIMC);
					CompStr* cs = imc.getCompStr();
					LPCTSTR msg = cs->getShowMsg();
					if( *msg )
					{
						POINT pt = getCompWndPos(imc);
						tooltip.showTip(pt.x, pt.y + 22, msg, 1500);
					}
					else if(tooltip.isVisible())
						tooltip.hideTip();
					break;
				}
			}
		}
	case IMN_GUIDELINE:
	// The IMN_GUIDELINE message is sent when an IME is about to show an error or 
	// information. When the application or UI window receives this message, either 
	// one can call ImmGetGuideLine to obtain information about the guideline.
		break;
	case IMN_SOFTKBDDESTROYED:
		break;
	}
	return 0;
}


LRESULT CALLBACK IMEUI::wndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	IMEUILock uilock(hwnd);
	IMEUI* ui = uilock.getIMEUI();
	if( ! ui )
		ui = uilock.createIMEUI();
	if( ui )
		return ui->wndProc( msg, wp, lp );
	return 0;
}

BOOL IMEUI::registerUIClasses()
{
	WNDCLASSEX wc;
	wc.cbSize			= sizeof(WNDCLASSEX);
	wc.style			= CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS| CS_IME;
	wc.lpfnWndProc		= (WNDPROC)IMEUI::wndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 2 * sizeof(LONG);
	wc.hInstance		= g_dllInst;
	wc.hCursor			= LoadCursor( NULL, IDC_ARROW );
	wc.hIcon			= NULL;
	wc.lpszMenuName		= (LPTSTR)NULL;
	wc.lpszClassName	= g_pcmanIMEClass;
	wc.hbrBackground	= NULL;
	wc.hIconSm			= NULL;
	if( !RegisterClassEx( (LPWNDCLASSEX)&wc ) 
		|| !CompWnd::registerClass()
		|| !CandWnd::registerClass()
		|| !StatusWnd::registerClass()
		|| !Tooltip::registerClass() )
		return FALSE;
	return TRUE;
}

LRESULT IMEUI::wndProc( UINT msg, WPARAM wp, LPARAM lp)
{
	HIMC hIMC = (HIMC)GetWindowLong(hwnd, IMMGWL_IMC);
	switch(msg)
	{
	case WM_IME_NOTIFY:
		if( ! hIMC )
			return 0;
		onIMENotify( hIMC,wp, lp );
		break;
	case WM_IME_STARTCOMPOSITION:
		{
			if( ! hIMC )
				return 0;
			IMCLock imc( hIMC );
			if( !imc.getIC() )
				break;
			POINT pt = getCompWndPos( imc );
			if( !compWnd.isWindow() )
				compWnd.create(hwnd);
			compWnd.move(pt.x, pt.y);
			break;
		}
	case WM_IME_COMPOSITION:
		if( ! hIMC )
			return 0;
		return onComposition( hIMC, wp, lp );
	case WM_IME_ENDCOMPOSITION:
		{
			compWnd.hide();
			break;
		}
	case WM_IME_SETCONTEXT:
		{
			if( wp )
			{
				if( hIMC )
				{
					if( (lp & ISC_SHOWUICOMPOSITIONWINDOW)
					&& ! compWnd.getDisplayedCompStr().empty() )
						compWnd.show();
					statusWnd.show();
				}
				else
				{
					compWnd.hide();
					statusWnd.hide();
				}
			}
			else
			{
				compWnd.hide();
				statusWnd.hide();
			}
			break;
		}
	case WM_IME_RELOADCONFIG:
		LoadConfig();
		if( g_hideStatusWnd && statusWnd.isVisible() )
			statusWnd.hide();
		break;
	case WM_CREATE:
		{
			break;
		}
	case WM_NCDESTROY:
		{
			HGLOBAL hIMEUI = (HGLOBAL)GetWindowLong(hwnd, IMMGWL_PRIVATE);
			IMEUI* ui = (IMEUI*)GlobalLock(hIMEUI);
			if(ui)
				ui->~IMEUI();	// delete ui
			GlobalUnlock(hIMEUI);
			GlobalFree(hIMEUI);
		}
		break;
	default:
		if( !IsImeMessage(msg) )
			return DefWindowProc(hwnd, msg, wp, lp);
	}
	return 0;
}


void IMEUI::closeStatusWnd(void)
{
//	char log[100];
//	sprintf(log, "CLOSE: hwnd=%x, himc=%x", hwnd, hIMC );
	if( statusWnd.isWindow() )
		statusWnd.hide();
}

void IMEUI::openStatusWnd(HIMC hIMC)
{
	if( g_hideStatusWnd )
		return;

	if( !statusWnd.isWindow() )
		statusWnd.create(hwnd);

	IMCLock imc(hIMC);
	INPUTCONTEXT* ic = imc.getIC();

	if( ic )
	{
		if( ic->ptStatusWndPos.x == -1 && ic->ptStatusWndPos.y == -1 )
		{
			RECT rc;
			IMEUI::getWorkingArea( &rc, ic->hWnd );
			int w, h;
			statusWnd.getSize(&w, &h);
			ic->ptStatusWndPos.x = rc.right - w;
			ic->ptStatusWndPos.y = rc.bottom - h - 32;
		}
		statusWnd.move( ic->ptStatusWndPos.x, ic->ptStatusWndPos.y );
	}
	statusWnd.show();
}

void IMEUI::openCandWnd(void)
{
	if( !candWnd.isWindow() )
		candWnd.create(hwnd);

	if( candWnd.isVisible() )
		candWnd.refresh();
	else
		candWnd.show();
}

void IMEUI::updateCandWnd(void)
{
	if( !candWnd.isWindow() )
		candWnd.create(hwnd);

	candWnd.refresh();
//	candWnd.updateSize();
	candWnd.show();
}

void IMEUI::closeCandWnd(void)
{
	candWnd.destroy();
}

bool IMEUI::getWorkingArea(RECT* rc, HWND app_wnd)
{
	HMONITOR mon = MonitorFromWindow( app_wnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	if( GetMonitorInfo(mon, &mi) )
		*rc = mi.rcWork;
	return true;
}

void IMEUI::unregisterUIClasses()
{
	UnregisterClass(g_pcmanIMEClass, g_dllInst);
	UnregisterClass(g_candWndClass, g_dllInst);
	UnregisterClass(g_compWndClass, g_dllInst);
	UnregisterClass(g_statusWndClass, g_dllInst);
	Tooltip::unregisterClass();
//	XPToolbar::unregisterClass();
}

LRESULT IMEUI::onComposition(HIMC hIMC, WPARAM wp , LPARAM lp)
{
	IMCLock imc(hIMC);
	if( !imc.getIC() )
		return 0;

	CompStr* cs = imc.getCompStr();

	if( !compWnd.isWindow() )
		compWnd.create(hwnd);

	if( lp & GCS_COMPSTR )
	{
		compWnd.refresh();
		POINT pt = getCompWndPos(imc);
		compWnd.move( pt.x, pt.y );
		int w, h;
		compWnd.getSize(&w, &h);
		SetWindowPos( compWnd.getHwnd(), NULL, 0, 0, w, h, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE );

		if( *cs->getCompStr() )
		{
			if( !compWnd.isVisible() )
				compWnd.show();
		}
		else
			compWnd.hide();

	}
	if( (lp & GCS_COMPSTR) || (lp & GCS_CURSORPOS) )
		if( compWnd.isVisible() )
			compWnd.refresh();
	return LRESULT(0);
}

POINT IMEUI::getCompWndPos(IMCLock& imc)
{
	POINT pt;
	if( g_fixCompWnd && compWnd.isWindow() )
	{
		RECT rc;
		GetWindowRect( compWnd.getHwnd(), &rc );
		pt.x = rc.left;
		pt.y = rc.top;
	}
	else
	{
		pt = imc.getIC()->cfCompForm.ptCurrentPos;
		bool absolute = false;
		if( g_fixCompWnd || 0 == imc.getIC()->cfCompForm.dwStyle )
		{
			RECT rc;
			if( g_fixCompWnd || !GetCaretPos( &pt ) )
			{
				getWorkingArea( &rc, imc.getIC()->hWnd );
				pt.x = rc.left + 10;
				pt.y = rc.bottom -= 50;
				absolute = true;
			}
		}
		imc.getIC()->cfCompForm.ptCurrentPos = pt;
		if( ! (imc.getIC()->fdwInit & INIT_COMPFORM) )
			imc.getIC()->fdwInit |= INIT_COMPFORM;
		if( !absolute )
			ClientToScreen( imc.getIC()->hWnd, &pt );
	}
	return pt;
}

/*
void IMEUI::setCompWndPos(IMCLock& imc)
{
	INPUTCONTEXT* ic = imc.getIC();
	if( !ic )
		return;
	POINT pt;
	if( ic->cfCompForm.dwStyle & CFS_FORCE_POSITION )
	{
		pt = ic->cfCompForm.ptCurrentPos;
		ClientToScreen( ic->hWnd, &pt );
	}
	else
	{
		switch( ic->cfCompForm.dwStyle )
		{
		case CFS_POINT:
		case CFS_RECT :
			pt = ic->cfCompForm.ptCurrentPos;
			ClientToScreen( ic->hWnd, &pt );
			break;
		case CFS_DEFAULT:
		default:
			if( GetCaretPos(&pt) )
				ClientToScreen( ic->hWnd, &pt );
			else
			{
				RECT rc;
				getWorkingArea(&rc, ic->hWnd);
				pt.x = rc.left + 20;
				pt.y = rc.bottom - 80;
			}
			break;
		}
	}
	if( !compWnd.isWindow() )
		compWnd.create(hwnd);
	compWnd.move(pt.x, pt.y);
}
*/

