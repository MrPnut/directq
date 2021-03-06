
/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
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
#include "d3d_model.h"
#include "location.h"


char *locDefault = "unknown";
char *locNowhere = "unknown";

location_t	*locations = NULL;
int			numlocations = 0;


void LOC_LoadLocations (void)
{
	locations = NULL;
	numlocations = 0;

	char locname[MAX_PATH];
	COM_StripExtension (cl.worldmodel->name, locname);

	// this assumes that cl.worldmodel begins with "maps/"
	memcpy (locname, "locs", 4);
	/*
	locname[0] = 'l';
	locname[1] = 'o';
	locname[2] = 'c';
	locname[3] = 's';
	*/
	COM_DefaultExtension (locname, ".loc");

	char *locdataload = (char *) COM_LoadFile (locname);

	if (!locdataload)
	{
		Con_DPrintf ("Failed to load %s\n", locname);
		return;
	}

	char *locdata = locdataload;
	Con_DPrintf ("Loading %s\n", locname);

	// these are dynamically sized so they go on the hunk
	locations = (location_t *) scratchbuf;
	location_t *l = locations;
	memset (l, 0, sizeof (location_t));

	int maxlocations = SCRATCHBUF_SIZE / sizeof (location_t);

	while (true)
	{
		if (numlocations >= maxlocations) break;

		// parse a line from the LOC string
		if (!(locdata = COM_Parse (locdata, COM_PARSE_LINE))) break;

		// scan it in to a temp location
		if (sscanf (com_token, "%f, %f, %f, %f, %f, %f, ", &l->a[0], &l->a[1], &l->a[2], &l->b[0], &l->b[1], &l->b[2]) == 6)
		{
			l->sd = 0;	// JPG 1.05

			for (int i = 0; i < 3; i++)
			{
				if (l->a[i] > l->b[i])
				{
					float temp = l->a[i];
					l->a[i] = l->b[i];
					l->b[i] = temp;
				}

				l->sd += l->b[i] - l->a[i];  // JPG 1.05
			}

			l->a[2] -= 32.0;
			l->b[2] += 32.0;

			// now get the name - this is potentially evil stuff...
			// scan to first quote and remove it
			for (int i = 0;; i++)
			{
				if (!com_token[i])
				{
					// there may not be a first quote...
					Q_strncpy (l->name, com_token, 31);
					break;
				}

				if (com_token[i] == '\"')
				{
					// the valid first character is after the quote
					Q_strncpy (l->name, &com_token[i + 1], 31);
					break;
				}
			}

			// scan to last quote and NULL term it there
			for (int i = 0;; i++)
			{
				if (!l->name[i]) break;

				if (l->name[i] == '\"')
				{
					l->name[i] = 0;
					break;
				}
			}

			Con_DPrintf ("Read location %s\n", l->name);

			// set up a new empty location (this may not be used...)
			l++;
			memset (l, 0, sizeof (location_t));
			numlocations++;
		}
	}

	if (numlocations > 0)
	{
		l = (location_t *) ClientZone->Alloc (numlocations * sizeof (location_t));
		memcpy (l, locations, numlocations * sizeof (location_t));
		locations = l;
	}
	else locations = NULL;

	Zone_Free (locdataload);
	Con_DPrintf ("Read %i locations\n", numlocations);
}


char *LOC_GetLocation (vec3_t p)
{
	// no locations available
	if (!locations || !numlocations) return locNowhere;

	location_t *l;
	location_t *bestloc;
	float dist, bestdist;

	bestloc = NULL;
	bestdist = 999999;

	for (l = locations; l < locations + numlocations; l++)
	{
		dist =	fabs (l->a[0] - p[0]) + fabs (l->b[0] - p[0]) +
				fabs (l->a[1] - p[1]) + fabs (l->b[1] - p[1]) +
				fabs (l->a[2] - p[2]) + fabs (l->b[2] - p[2]) - l->sd;

		if (dist < .01) return l->name;

		if (dist < bestdist)
		{
			bestdist = dist;
			bestloc = l;
		}
	}

	if (bestloc)
		return bestloc->name;

	return locDefault;
}
