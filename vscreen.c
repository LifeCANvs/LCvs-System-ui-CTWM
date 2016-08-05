/*
 *  [ ctwm ]
 *
 *  Copyright 1992 Claude Lecommandeur.
 *
 * Permission to use, copy, modify  and distribute this software  [ctwm] and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above  copyright notice appear  in all copies and that both that
 * copyright notice and this permission notice appear in supporting documen-
 * tation, and that the name of  Claude Lecommandeur not be used in adverti-
 * sing or  publicity  pertaining to  distribution of  the software  without
 * specific, written prior permission. Claude Lecommandeur make no represen-
 * tations  about the suitability  of this software  for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * Claude Lecommandeur DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL  IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL  Claude Lecommandeur  BE LIABLE FOR ANY SPECIAL,  INDIRECT OR
 * CONSEQUENTIAL  DAMAGES OR ANY  DAMAGES WHATSOEVER  RESULTING FROM LOSS OF
 * USE, DATA  OR PROFITS,  WHETHER IN AN ACTION  OF CONTRACT,  NEGLIGENCE OR
 * OTHER  TORTIOUS ACTION,  ARISING OUT OF OR IN  CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Claude Lecommandeur [ lecom@sic.epfl.ch ][ April 1992 ]
 */

#include "ctwm.h"

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xatom.h>

#include "ctwm_atoms.h"
#include "cursor.h"
#include "icons.h"
#include "otp.h"
#include "screen.h"


static void DisplayWinUnchecked(VirtualScreen *vs, TwmWindow *tmp_win);


void InitVirtualScreens(ScreenInfo *scr)
{
	Cursor cursor;
	unsigned long valuemask, attrmask;
	XSetWindowAttributes attributes;
	name_list *nptr;
	bool userealroot = true;
	VirtualScreen *vs00 = NULL;

	NewFontCursor(&cursor, "X_cursor");

	if(scr->VirtualScreens == NULL) {
		if(userealroot) {
			VirtualScreen *vs = malloc(sizeof(VirtualScreen));

			vs->x      = 0;
			vs->y      = 0;
			vs->w      = scr->rootw;
			vs->h      = scr->rooth;
			vs->window = scr->Root;
			vs->next   = NULL;
			vs->wsw    = 0;
			scr->vScreenList = vs;
			scr->currentvs   = vs;
			scr->numVscreens = 1;
			return;
		}
		else {
			scr->VirtualScreens = malloc(sizeof(name_list));
			scr->VirtualScreens->next = NULL;
			asprintf(&scr->VirtualScreens->name, "%dx%d+0+0",
			         scr->rootw, scr->rooth);
		}
	}
	scr->numVscreens = 0;

	attrmask  = ColormapChangeMask | EnterWindowMask | PropertyChangeMask |
	            SubstructureRedirectMask | KeyPressMask | ButtonPressMask |
	            ButtonReleaseMask;

	valuemask = CWBackingStore | CWSaveUnder | CWBackPixel | CWOverrideRedirect |
	            CWEventMask | CWCursor;
	attributes.backing_store     = NotUseful;
	attributes.save_under        = False;
	attributes.override_redirect = True;
	attributes.event_mask        = attrmask;
	attributes.cursor            = cursor;
	attributes.background_pixel  = Scr->Black;

	scr->vScreenList = NULL;
	for(nptr = Scr->VirtualScreens; nptr != NULL; nptr = nptr->next) {
		VirtualScreen *vs;
		char *geometry = (char *) nptr->name;
		int x = 0, y = 0;
		unsigned int w = 0, h = 0;

		XParseGeometry(geometry, &x, &y, &w, &h);

		if((x < 0) || (y < 0) || (w > scr->rootw) || (h > scr->rooth)) {
			fprintf(stderr, "InitVirtualScreens : invalid geometry : %s\n", geometry);
			continue;
		}
		vs = malloc(sizeof(VirtualScreen));
		vs->x = x;
		vs->y = y;
		vs->w = w;
		vs->h = h;
		vs->window = XCreateWindow(dpy, Scr->Root, x, y, w, h,
		                           0, CopyFromParent, CopyFromParent,
		                           CopyFromParent, valuemask, &attributes);
		vs->wsw = 0;

		XSync(dpy, 0);
		XMapWindow(dpy, vs->window);
		XChangeProperty(dpy, vs->window, XA_WM_VIRTUALROOT, XA_STRING, 8,
		                PropModeReplace, (unsigned char *) "Yes", 4);

		vs->next = scr->vScreenList;
		scr->vScreenList = vs;
		Scr->numVscreens++;

		/*
		 * Remember which virtual screen is at (0,0).
		 */
		if(x == 0 && y == 0) {
			vs00 = vs;
		}
	}

	if(scr->vScreenList == NULL) {
		fprintf(stderr, "no valid VirtualScreens found, exiting...\n");
		exit(1);
	}
	/* Setup scr->{currentvs,Root{,x,y,w,h}} as if the
	 * _correct_ virtual screen is entered with the mouse.
	 * See HandleEnterNotify().
	 */
	if(vs00 == NULL) {
		vs00 = scr->vScreenList;
	}

	Scr->Root  = vs00->window;
	Scr->rootx = Scr->crootx + vs00->x;
	Scr->rooty = Scr->crooty + vs00->y;
	Scr->rootw = vs00->w;
	Scr->rooth = vs00->h;
	Scr->currentvs = vs00;
}

VirtualScreen *findIfVScreenOf(int x, int y)
{
	VirtualScreen *vs;
	for(vs = Scr->vScreenList; vs != NULL; vs = vs->next) {

		if((x >= vs->x) && ((x - vs->x) < vs->w) &&
		                (y >= vs->y) && ((y - vs->y) < vs->h)) {
			return vs;
		}
	}
	return NULL;
}

VirtualScreen *getVScreenOf(int x, int y)
{
	VirtualScreen *vs;
	if((vs = findIfVScreenOf(x, y))) {
		return vs;
	}
	return Scr->vScreenList;
}

/*
 * Returns the order that virtual screens are displayed for the vscreen
 * list.  This is stored this way so everything ends up in the right place
 * on a ctwm restart.
 */
bool
CtwmGetVScreenMap(Display *display, Window rootw,
                  char *outbuf, int *outbuf_len)
{
	unsigned char       *prop;
	unsigned long       bytesafter;
	unsigned long       len;
	Atom                actual_type;
	int                 actual_format;

	if(XA_WM_CTWM_VSCREENMAP == None) {
		return false;
	}
	if(XGetWindowProperty(display, rootw, XA_WM_CTWM_VSCREENMAP, 0L, 512,
	                      False, XA_STRING, &actual_type, &actual_format, &len,
	                      &bytesafter, &prop) != Success) {
		return false;
	}
	if(len == 0) {
		return false;
	}
	*outbuf_len = (len >= *outbuf_len) ? *outbuf_len - 1 : len;
	memcpy(outbuf, prop, *outbuf_len);
	outbuf[*outbuf_len] = '\0';
	XFree(prop);
	return true;
}

bool
CtwmSetVScreenMap(Display *display, Window rootw,
                  struct VirtualScreen *firstvs)
{
	char                        buf[1024];
	int                         tally = 0;
	struct VirtualScreen        *vs;

	if(XA_WM_CTWM_VSCREENMAP == None) {
		return false;
	}

	memset(buf, 0, sizeof(buf));
	for(vs = firstvs; vs; vs = vs->next) {
		if(tally) {
			strcat(buf, ",");
		}
		if(vs->wsw && vs->wsw->currentwspc && vs->wsw->currentwspc->name) {
			strcat(buf, vs->wsw->currentwspc->name);
			tally++;
		}
	}

	if(! tally) {
		return false;
	}

	XChangeProperty(display, rootw, XA_WM_CTWM_VSCREENMAP, XA_STRING, 8,
	                PropModeReplace, (unsigned char *)buf, strlen(buf));
	return true;
}


/*
 * Display a window in a given virtual screen.
 */
void
DisplayWin(VirtualScreen *vs, TwmWindow *tmp_win)
{
	OtpCheckConsistency();
	DisplayWinUnchecked(vs, tmp_win);
	OtpCheckConsistency();
}

static void
DisplayWinUnchecked(VirtualScreen *vs, TwmWindow *tmp_win)
{
	/*
	 * A window cannot be shown in multiple virtual screens, even if
	 * it occupies both corresponding workspaces.
	 */
	if(vs && tmp_win->vs) {
		return;
	}

	/* This is where we're moving it */
	tmp_win->vs = vs;


	/* If it's unmapped, RFAI() moves the necessary bits here */
	if(!tmp_win->mapped) {
		ReparentFrameAndIcon(tmp_win);

		/* If it's got an icon that should be up, make it up here */
		if(tmp_win->isicon) {
			if(tmp_win->icon_on) {
				if(tmp_win->icon && tmp_win->icon->w) {

					IconUp(tmp_win);
					XMapWindow(dpy, tmp_win->icon->w);
				}
			}
		}

		/* All there is to do with unmapped wins */
		return;
	}


	/* If we make it this far, the window is mapped */

	if(tmp_win->UnmapByMovingFarAway) {
		/*
		 * XXX I don't believe the handling of UnmapByMovingFarAway is
		 * quite correct.
		 */
		if(vs) {
			XReparentWindow(dpy, tmp_win->frame, vs->window,
			                tmp_win->frame_x, tmp_win->frame_y);
		}
		else {
			XMoveWindow(dpy, tmp_win->frame, tmp_win->frame_x, tmp_win->frame_y);
		}
	}
	else {
		/* Map and move it here */
		if(!tmp_win->squeezed) {
			XWindowAttributes   winattrs;
			unsigned long       eventMask;

			XGetWindowAttributes(dpy, tmp_win->w, &winattrs);
			eventMask = winattrs.your_event_mask;
			XSelectInput(dpy, tmp_win->w, eventMask & ~StructureNotifyMask);
			XMapWindow(dpy, tmp_win->w);
			XSelectInput(dpy, tmp_win->w, eventMask);
		}

		ReparentFrameAndIcon(tmp_win);

		XMapWindow(dpy, tmp_win->frame);
		SetMapStateProp(tmp_win, NormalState);
	}
}


/*
 * Move a window's frame and icon to a new VS.  This mostly happens as a
 * backend bit of the DisplayWin() process, but it does get called
 * directly for the Occupy window.  XXX Should it?
 */
void
ReparentFrameAndIcon(TwmWindow *tmp_win)
{
	VirtualScreen *vs = tmp_win->vs; /* which virtual screen we want it in */

	/* parent_vs is the current real parent of the window */
	if(vs != tmp_win->parent_vs) {
		struct Icon *icon = tmp_win->icon;

		tmp_win->parent_vs = vs;

		if(icon && icon->w) {
			ReparentWindowAndIcon(dpy, tmp_win, vs->window,
			                      tmp_win->frame_x, tmp_win->frame_y,
			                      icon->w_x, icon->w_y);
		}
		else {
			ReparentWindow(dpy, tmp_win,  WinWin, vs->window,
			               tmp_win->frame_x, tmp_win->frame_y);
		}
	}
}
