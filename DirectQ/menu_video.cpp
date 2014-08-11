/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


#include "quakedef.h"
#include "d3d_quake.h"

#include "winquake.h"
#include "resource.h"
#include <commctrl.h>


#include "menu_common.h"

char **menu_videomodes = NULL;
int menu_videomodenum = 0;

char **menu_anisotropicmodes = NULL;
int menu_anisonum = 0;

extern cvar_t gl_conscale;
extern cvar_t scr_fov;
extern cvar_t scr_fovcompat;
extern cvar_t d3d_mode;
extern cvar_t v_gamma;
extern cvar_t r_gamma;
extern cvar_t g_gamma;
extern cvar_t b_gamma;
extern cvar_t r_anisotropicfilter;
extern cvar_t vid_vsync;
extern cvar_t r_filterrefresh;
cvar_t dummy_vsync;

bool D3D_ModeIsCurrent (d3d_ModeDesc_t *mode);

extern d3d_ModeDesc_t *d3d_ModeList;

#define TAG_VIDMODEAPPLY	1

// if these are changed they need to be changed in menu_other.cpp as well
#define MENU_TAG_SIMPLE		666
#define MENU_TAG_FULL		1313


char **filterrefreshstrings = NULL;
int filterrefreshnum = 0;

void VID_ApplyModeChange (void)
{
	// the value here has already been forced to correct in the draw func so we just set it
	Cvar_Set (&d3d_mode, menu_videomodenum);
	Cvar_Set (&vid_vsync, dummy_vsync.integer);

	// position the selection back at the video mode option
	menu_Video.Key (K_UPARROW);
}


int Menu_VideoCustomDraw (int y)
{
	// 5 is an arbitrary upper limit here
	if (r_filterrefresh.value < 0) Cvar_Set (&r_filterrefresh, 0.0f);
	if (r_filterrefresh.value > 5) Cvar_Set (&r_filterrefresh, 5.0f);

	Cvar_Set (&r_filterrefresh, (float) filterrefreshnum / 4.0f);

	// check for "Apply" on d3d_mode change
	if (menu_videomodenum == d3d_mode.integer && vid_vsync.integer == dummy_vsync.integer)
		menu_Video.DisableOptions (TAG_VIDMODEAPPLY);
	else menu_Video.EnableOptions (TAG_VIDMODEAPPLY);

	d3d_ModeDesc_t *mode;
	int nummodes;

	// ensure videomode num is valid
	// (no longer forces an instant change)
	for (mode = d3d_ModeList, nummodes = 0; mode; mode = mode->Next, nummodes++)
	{
		if (menu_videomodenum == nummodes)
		{
			// store to d3d_mode cvar
			//d3d_mode.value = nummodes;
			//Cvar_Set (&d3d_mode, d3d_mode.value);
			goto do_aniso;
		}
	}

	// invalid mode in d3d_mode so force it to the current mode
	for (mode = d3d_ModeList, nummodes = 0; mode; mode = mode->Next, nummodes++)
	{
		if (D3D_ModeIsCurrent (mode))
		{
			menu_videomodenum = /*d3d_mode.value =*/ nummodes;
			//Cvar_Set (&d3d_mode, d3d_mode.value);
			break;
		}
	}

do_aniso:
	// no anisotropic filtering available
	if (d3d_DeviceCaps.MaxAnisotropy < 2) return y;

	// store the selected anisotropic filter into the r_aniso cvar
	for (int af = 1, i = 0; ; i++, af <<= 1)
	{
		if (i == menu_anisonum)
		{
			Con_DPrintf ("Setting r_anisotropicfilter to %i\n", af);
			Cvar_Set (&r_anisotropicfilter, af);
			break;
		}
	}

	return y;
}


void Menu_VideoCustomEnter (void)
{
	if (!filterrefreshstrings)
	{
		filterrefreshstrings = (char **) Pool_Alloc (POOL_PERMANENT, 22 * sizeof (char *));

		for (int i = 0; i < 21; i++)
		{
			filterrefreshstrings[i] = (char *) Pool_Alloc (POOL_PERMANENT, 16);
			filterrefreshstrings[i][0] = 0;
		}

		filterrefreshstrings[21] = NULL;
	}

	// 5 is an arbitrary upper limit here
	if (r_filterrefresh.value < 0) Cvar_Set (&r_filterrefresh, 0.0f);
	if (r_filterrefresh.value > 5) Cvar_Set (&r_filterrefresh, 5.0f);

	filterrefreshnum = r_filterrefresh.value * 4;

	if (filterrefreshnum < 0) filterrefreshnum = 0;
	if (filterrefreshnum > 10) filterrefreshnum = 10;

	extern D3DDISPLAYMODE d3d_DesktopMode;
	int rrate = d3d_CurrentMode.RefreshRate;

	if (!rrate) rrate = d3d_DesktopMode.RefreshRate;
	if (!rrate) rrate = 60;

	sprintf (filterrefreshstrings[0], "Off");

	for (int i = 1; ; i++)
	{
		if (!filterrefreshstrings[i]) break;

		sprintf (filterrefreshstrings[i], "%0.1f Hz", (float) rrate * (float) i / 4.0f);
	}

	// take it from the d3d_mode cvar
	menu_videomodenum = (int) d3d_mode.value;

	// store out vsync
	dummy_vsync.integer = vid_vsync.integer;

	int real_aniso;

	// no anisotropic filtering available
	if (d3d_DeviceCaps.MaxAnisotropy < 2) return;

	// get the real value from the cvar - users may enter any old crap manually!!!
	for (real_aniso = 1; real_aniso < r_anisotropicfilter.value; real_aniso <<= 1);

	// clamp it
	if (real_aniso < 1) real_aniso = 1;
	if (real_aniso > d3d_DeviceCaps.MaxAnisotropy) real_aniso = d3d_DeviceCaps.MaxAnisotropy;

	// store it back
	Cvar_Set (&r_anisotropicfilter, real_aniso);

	// now derive the menu entry from it
	for (int i = 0, af = 1; ; i++, af <<= 1)
	{
		if (af == real_aniso)
		{
			menu_anisonum = i;
			break;
		}
	}
}


void Menu_VideoBuild (void)
{
	d3d_ModeDesc_t *mode;
	int nummodes;

	// get the number of modes
	for (mode = d3d_ModeList, nummodes = 0; mode; mode = mode->Next, nummodes++);

	// add 1 for terminating NULL
	menu_videomodes = (char **) Pool_Alloc (POOL_PERMANENT, (nummodes + 1) * sizeof (char *));

	// now write them in
	for (mode = d3d_ModeList, nummodes = 0; mode; mode = mode->Next, nummodes++)
	{
		menu_videomodes[nummodes] = (char *) Pool_Alloc (POOL_PERMANENT, 128);

		_snprintf
		(
			menu_videomodes[nummodes],
			128,
			"%i x %i x %i (%s)",
			mode->d3d_Mode.Width,
			mode->d3d_Mode.Height,
			mode->BPP,
			mode->ModeDesc
		);

		// select current mode
		if (D3D_ModeIsCurrent (mode)) menu_videomodenum = nummodes;
	}

	// terminate with NULL
	menu_videomodes[nummodes] = NULL;

	menu_Video.AddOption (new CQMenuCustomDraw (Menu_VideoCustomDraw));
	menu_Video.AddOption (new CQMenuSpacer ("Select a Video Mode"));
	menu_Video.AddOption (new CQMenuSpinControl (NULL, &menu_videomodenum, menu_videomodes));
	menu_Video.AddOption (new CQMenuCvarToggle ("Vertical Sync", &dummy_vsync, 0, 1));
	menu_Video.AddOption (TAG_VIDMODEAPPLY, new CQMenuSpacer (DIVIDER_LINE));
	menu_Video.AddOption (TAG_VIDMODEAPPLY, new CQMenuCommand ("Apply Video Mode Change", VID_ApplyModeChange));

	// add the rest of the options to ensure that they;re kept in order
	menu_Video.AddOption (new CQMenuSpacer ());
	menu_Video.AddOption (new CQMenuTitle ("Configure Video Options"));
	menu_Video.AddOption (new CQMenuSpinControl ("Filter Refresh", &filterrefreshnum, &filterrefreshstrings));
	menu_Video.AddOption (new CQMenuCvarSlider ("Screen Size", &scr_viewsize, 30, 120, 10));
	menu_Video.AddOption (new CQMenuCvarSlider ("Console Size", &gl_conscale, 1, 0, 0.1));

	if (d3d_DeviceCaps.MaxAnisotropy > 1)
	{
		// count the number of modes available
		for (int i = 1, mode = 1; ; i++, mode *= 2)
		{
			if (mode == d3d_DeviceCaps.MaxAnisotropy)
			{
				menu_anisotropicmodes = (char **) Pool_Alloc (POOL_PERMANENT, (i + 1) * sizeof (char *));

				for (int m = 0, f = 1; m < i; m++, f *= 2)
				{
					menu_anisotropicmodes[m] = (char *) Pool_Alloc (POOL_PERMANENT, 32);

					if (f == 1)
						strncpy (menu_anisotropicmodes[m], "Off", 32);
					else _snprintf (menu_anisotropicmodes[m], 32, "%i x Filtering", f);
				}

				menu_anisotropicmodes[i] = NULL;
				break;
			}
		}

		menu_Video.AddOption (new CQMenuSpinControl ("Anisotropic Filter", &menu_anisonum, menu_anisotropicmodes));
	}

	menu_Video.AddOption (new CQMenuCvarSlider ("Field of View", &scr_fov, 10, 170, 5));
	menu_Video.AddOption (new CQMenuCvarToggle ("Compatible FOV", &scr_fovcompat, 0, 1));
	menu_Video.AddOption (MENU_TAG_FULL, new CQMenuTitle ("Brightness Controls"));
	menu_Video.AddOption (MENU_TAG_FULL, new CQMenuCvarSlider ("Master Gamma", &v_gamma, 1.75, 0.25, 0.05));
	menu_Video.AddOption (MENU_TAG_FULL, new CQMenuCvarSlider ("Red Gamma", &r_gamma, 1.75, 0.25, 0.05));
	menu_Video.AddOption (MENU_TAG_FULL, new CQMenuCvarSlider ("Green Gamma", &g_gamma, 1.75, 0.25, 0.05));
	menu_Video.AddOption (MENU_TAG_FULL, new CQMenuCvarSlider ("Blue Gamma", &b_gamma, 1.75, 0.25, 0.05));
}

