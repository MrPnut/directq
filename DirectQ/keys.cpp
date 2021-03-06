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

/*

key up events are sent even if in console mode

*/


#define      HISTORY_FILE_NAME   "id1/quake_history.txt"

// lets allow a nice big history
#define		MAXCMDLINE	256
#define		CMDLINES	256

int Key_ModifyKey (int key);

char	key_lines[CMDLINES][MAXCMDLINE];
int		key_linepos;
int		shift_down = false;
int		ctrl_down = false;
int		alt_down = false;
int		key_lastpress;
int		key_insert = 0;

int		edit_line = 0;
int		history_line = 0;

keydest_t	key_dest;

int		key_count;			// incremented every key event

char	*keybindings[256];
bool	consolekeys[256];	// if true, can't be rebound while in console
bool	menubound[256];	// if true, can't be rebound while in menu
int		keyshift[256];		// key to map to if shift held down in console
int		key_repeats[256];	// if > 1, it is autorepeating
bool	keydown[256];

typedef struct
{
	char	*name;
	int		keynum;
} keyname_t;

keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"POV1", K_POV1},
	{"POV2", K_POV2},
	{"POV3", K_POV3},
	{"POV4", K_POV4},

	{"PAUSE", K_PAUSE},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"SEMICOLON", ';'},	// because a raw semicolon seperates commands

	{NULL, 0}
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

void Key_HistoryLoad (void)
{
	int i, c;
	FILE *hf;

	for (i = 0; i < CMDLINES; i++)
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}

	key_linepos = 1;

	if ((hf = fopen (HISTORY_FILE_NAME, "r")))
	{
		do
		{
			i = 1;

			do
			{
				c = fgetc (hf);
				key_lines[edit_line][i++] = c;
			} while (c != '\n' && c != EOF && i < MAXCMDLINE);

			key_lines[edit_line][i - 1] = 0;
			edit_line = (edit_line + 1) & (CMDLINES - 1);
		} while (c != EOF && edit_line < CMDLINES);

		fclose (hf);

		history_line = edit_line = (edit_line - 1) & (CMDLINES - 1);
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = 0;
	}
}


void Key_HistoryFlush (void)
{
	int i;
	FILE *hf;

	if ((hf = fopen (HISTORY_FILE_NAME, "w")))
	{
		i = edit_line;

		do
		{
			i = (i + 1) & (CMDLINES - 1);
		} while (i != edit_line && !key_lines[i][1]);

		do
		{
			fprintf (hf, "%s\n", key_lines[i] + 1);
			i = (i + 1) & (CMDLINES - 1);
		} while (i != edit_line && key_lines[i][1]);

		fclose (hf);
	}
}


// protocol autocomplete list
char *protolist[] =
{
	"15",
	"Fitz",
	"RMQ",
	NULL
};

char *d3d_filtermodelist[] =
{
	"GL_NEAREST",
	"GL_LINEAR",
	"GL_NEAREST_MIPMAP_NEAREST",
	"GL_LINEAR_MIPMAP_NEAREST",
	"GL_NEAREST_MIPMAP_LINEAR",
	"GL_LINEAR_MIPMAP_LINEAR",
	NULL
};


// content autocompletion lists
extern char **spinbox_bsps;
extern char **skybox_menulist;
extern char **demolist;
extern char **saveloadlist;
extern char *gamedirs[];


typedef struct key_autotablist_s
{
	char *str;
	char **list;
} key_autotablist_t;

int Cmd_Match (char *partial, int matchcycle, bool conout);
int match_count = 0;
int match_cycle = 0;
char match_buf[256] = {0};
static int contentcycle = 0;

void Key_PrintMatch (char *cmd)
{
	strcpy (key_lines[edit_line] + 1, cmd);
	key_linepos = strlen (cmd) + 1;
	key_lines[edit_line][key_linepos] = ' ';
	key_linepos++;
	key_lines[edit_line][key_linepos] = 0;
}


void Key_PrintContentMatch (char *matchedcontent)
{
	// find the text position after the command
	// this needs to work back from the first \0 as subsequent text from a previous command may still be in the buffer and contain a space
	for (int i = strlen (key_lines[edit_line]); i >= 0; i--)
	{
		if (key_lines[edit_line][i] == ' ')
		{
			// switch the next to 0 so we can strcat it
			key_lines[edit_line][i + 1] = 0;
			break;
		}
	}

	// add the completed content and switch the cursor pos
	strcat (key_lines[edit_line], matchedcontent);
	key_linepos = strlen (key_lines[edit_line]);
}


void Key_ContentMatch (char **contentlist)
{
	static char matched[256] = {0};

	if (!contentcycle)
	{
		// grab the text after the command
		// we need to store this for subsequent commands
		// this needs to work back from the first \0 as subsequent text from a previous command may still be in the buffer and contain a space
		for (int i = strlen (key_lines[edit_line]); i > 0; i--)
		{
			//  find the first valid char
			if (key_lines[edit_line][i] == ' ')
			{
				// we can't just store the pointer as the text may have changed due to autocompletion
				strcpy (matched, &key_lines[edit_line][i + 1]);
				break;
			}
		}

		if (!matched[0])
		{
			// return the first item on the list
			Key_PrintContentMatch (contentlist[0]);
			contentcycle = 1;
			return;
		}

		// find the first matching one
		for (int i = 0;; i++)
		{
			if (!contentlist[i])
			{
				// no match (should this gracefully fail or inform the user of no match?)
				contentcycle = 0;
				// Key_PrintContentMatch ("** (No Match)");
				return;
			}

			if (!_strnicmp (contentlist[i], matched, strlen (matched)))
			{
				// do the completion
				Key_PrintContentMatch (contentlist[i]);
				contentcycle = i + 1;
				return;
			}
		}

		// never reached but i feel happier with it here...
		return;
	}

	for (int i = contentcycle, passes = 0;; i++)
	{
		// if we run out of maps we restart the cycle at 0
		if (!contentlist[i])
		{
			i = 0;

			// keep track of the number of passes through the list
			passes++;
		}

		// should never happen
		if (passes > 1) return;

		// check for a match
		if (!_strnicmp (contentlist[i], matched, strlen (matched)))
		{
			// do the completion
			Key_PrintContentMatch (contentlist[i]);
			contentcycle = i + 1;
			return;
		}
	}
}


bool Key_CheckAutoTab (char *str, char **list)
{
	if (list && list[0])
	{
		if (!_strnicmp (&key_lines[edit_line][1], str, strlen (str)))
		{
			Key_ContentMatch (list);
			return true;
		}
	}

	return false;
}


/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void Key_Console (int key)
{
	int		i;
	HANDLE	th;
	char	*clipText, *textCopied;

	if (key == K_ENTER)
	{
		Cbuf_AddText (key_lines[edit_line] + 1);	// skip the >
		Cbuf_AddText ("\n");
		Con_Printf ("%s\n", key_lines[edit_line]);

		edit_line = (edit_line + 1) & (CMDLINES - 1);
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = 0;

		key_linepos = 1;

		// force an update, because the command may take some time
		if (cls.state != ca_connected) SCR_UpdateScreen (0);

		return;
	}

	if (key == K_TAB)
	{
		// we can't declare these globally as they may change so we have to do it this way
		// oh well, at least it's still kinda cleaner (gotta watch the stack though...)
		key_autotablist_t key_autotablist[] =
		{
			{"map ", spinbox_bsps},
			{"changelevel ", spinbox_bsps},
			{"playdemo ", demolist},
			{"timedemo ", demolist},
			{"save ", saveloadlist},
			{"load ", saveloadlist},
			{"game ", gamedirs},
			{"gamedir ", gamedirs},
			{"sv_protocol ", protolist},
			{"gl_texturemode ", d3d_filtermodelist},
			{NULL, NULL}
		};

		for (i = 0;; i++)
		{
			if (!key_autotablist[i].str) break;
			if (!key_autotablist[i].list) break;
			if (Key_CheckAutoTab (key_autotablist[i].str, key_autotablist[i].list)) return;
		}

		// no match so begin a new cycle
		contentcycle = 0;

		if (!match_count)
		{
			// copy to the match buffer, null terminate (in case the cursor is in the middle of a line)
			strcpy (match_buf, key_lines[edit_line] + 1);
			match_buf[key_linepos - 1] = 0;

			// init the match cycle
			match_cycle = 0;

			// get the first match
			match_count = Cmd_Match (match_buf, match_cycle, true);
		}
		else
		{
			// get subsequent matches (no console output for these)
			Cmd_Match (match_buf, match_cycle, false);
		}

		// run the cycle
		if (++match_cycle >= match_count) match_cycle = 0;

		return;
	}
	else
	{
		contentcycle = 0;
		match_count = 0;
	}

	if (key == K_LEFTARROW)
	{
		if (key_linepos > 1) key_linepos--;

		return;
	}

	if (key == K_BACKSPACE)
	{
		if (key_linepos > 1)
		{
			strcpy (key_lines[edit_line] + key_linepos - 1, key_lines[edit_line] + key_linepos);
			key_linepos--;
		}

		return;
	}

	if (key == K_DEL)
	{
		// delete char on cursor
		if (key_linepos < strlen (key_lines[edit_line]))
			strcpy (key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + 1);

		return;
	}

	if (key == K_RIGHTARROW)
	{
		// if we're at the end, get one character from previous line, otherwise just go right one
		if (strlen (key_lines[edit_line]) == key_linepos)
		{
			// no character to get
			if (strlen (key_lines[(edit_line + (CMDLINES - 1)) & (CMDLINES - 1)]) <= key_linepos) return;

			key_lines[edit_line][key_linepos] = key_lines[(edit_line + (CMDLINES - 1)) & (CMDLINES - 1)][key_linepos];
			key_linepos++;
			key_lines[edit_line][key_linepos] = 0;
		}
		else key_linepos++;

		return;
	}

	if (key == K_INS)
	{
		// toggle insert mode
		key_insert ^= 1;
		return;
	}

	if (key == K_UPARROW)
	{
		do
		{
			history_line = (history_line - 1) & (CMDLINES - 1);
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line)
			history_line = (edit_line + 1) & (CMDLINES - 1);

		strcpy (key_lines[edit_line], key_lines[history_line]);
		key_linepos = strlen (key_lines[edit_line]);
		return;
	}

	if (key == K_DOWNARROW)
	{
		if (history_line == edit_line) return;

		do
		{
			history_line = (history_line + 1) & (CMDLINES - 1);
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line)
		{
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		}
		else
		{
			strcpy (key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen (key_lines[edit_line]);
		}

		return;
	}

	if (key == K_PGUP || key == K_MWHEELUP)
	{
		con_backscroll += 2;

		if (con_backscroll > con_totallines - (vid.consize.height >> 3) - 1)
			con_backscroll = con_totallines - (vid.consize.height >> 3) - 1;

		return;
	}

	if (key == K_PGDN || key == K_MWHEELDOWN)
	{
		con_backscroll -= 2;

		if (con_backscroll < 0)
			con_backscroll = 0;

		return;
	}

	if (key == K_HOME)
	{
		// the home key functions differently depending on whether or not we are in the first position on the line
		if (key_linepos == 1)
		{
			con_backscroll = con_totallines - (vid.consize.height >> 3) - 1;
			return;
		}
		else
		{
			// expected behaviour
			key_linepos = 1;
			return;
		}
	}

	if (key == K_END)
	{
		// the home key functions differently depending on whether or not we are in the first position on the line
		if (!key_lines[edit_line][key_linepos])
		{
			// note that position 0 is the input prompt ']'
			con_backscroll = 0;
			return;
		}
		else
		{
			// expected behaviour
			key_linepos = strlen (key_lines[edit_line]);
			return;
		}
	}

	if ((key == 'C' || key == 'c') && keydown[K_CTRL])
	{
		if (OpenClipboard (NULL))
		{
			// set up the text; note - SetClipboardData needs a handle, not a pointer, so
			// we need to go through this rigmarole to get it.
			HGLOBAL hCopyText = (HGLOBAL) GlobalAlloc (GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, 256);
			char *CopyText = (char *) GlobalLock (hCopyText);

			// because [0] is the "]" prompt
			strcpy (CopyText, &key_lines[edit_line][1]);
			GlobalUnlock (hCopyText);

			// ensure that there's nothing there before we begin
			EmptyClipboard ();

			// copy it in
			SetClipboardData (CF_TEXT, (HANDLE) hCopyText);

			// done.  note - per MSDN, the system owns the data now so we can't free it
			CloseClipboard ();
			return;
		}
	}

	if ((key == 'V' || key == 'v') && keydown[K_CTRL])
	{
		if (OpenClipboard (NULL))
		{
			th = GetClipboardData (CF_TEXT);

			if (th)
			{
				clipText = (char *) GlobalLock (th);

				if (clipText)
				{
					textCopied = (char *) Zone_Alloc (GlobalSize (th) + 1);
					strcpy (textCopied, clipText);

					// Substitutes a NULL for every token
					strtok (textCopied, "\n\r\b");

					i = strlen (textCopied);

					if (i + key_linepos >= MAXCMDLINE) i = MAXCMDLINE - key_linepos;

					if (i > 0)
					{
						textCopied[i] = 0;
						strcat (key_lines[edit_line], textCopied);
						key_linepos += i;
					}

					Zone_Free (textCopied);
				}

				GlobalUnlock (th);
			}

			CloseClipboard ();
			return;
		}
	}

	if ((key = Key_ModifyKey (key)) == 0) return;

	if (key_linepos < MAXCMDLINE - 1)
	{
		int i;

		// check insert mode
		if (key_insert)
		{
			// can't do strcpy to move string to right
			if ((i = strlen (key_lines[edit_line]) - 1) == 254) i--;

			for (; i >= key_linepos; i--) key_lines[edit_line][i + 1] = key_lines[edit_line][i];
		}

		// only null terminate if at the end
		i = key_lines[edit_line][key_linepos];
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;

		if (!i) key_lines[edit_line][key_linepos] = 0;
	}
}


//============================================================================

char chat_buffer[78];
bool team_message = false;

void Key_Message (int key)
{
	static int chat_bufferlen = 0;

	if (key == K_ENTER)
	{
		if (team_message)
			Cbuf_AddText ("say_team \"");
		else Cbuf_AddText ("say \"");

		Cbuf_AddText (chat_buffer);
		Cbuf_AddText ("\"\n");

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE)
	{
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	if (key == K_BACKSPACE)
	{
		if (chat_bufferlen)
		{
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}

		return;
	}

	if (chat_bufferlen == 77)
		return; // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================


/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum (char *str)
{
	keyname_t	*kn;

	if (!str || !str[0])
		return -1;

	if (!str[1])
		return str[0];

	for (kn = keynames; kn->name; kn++)
	{
		if (!_stricmp (str, kn->name))
			return kn->keynum;
	}

	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char *Key_KeynumToString (int keynum)
{
	keyname_t	*kn;
	static	char	tinystr[2];

	if (keynum == -1)
		return "<KEY NOT FOUND>";

	if (keynum > 32 && keynum < 127)
	{
		// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}


/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding (int keynum, char *binding)
{
	char	*newbind;
	int		l;

	if (keynum == -1)
		return;

	// free old bindings
	if (keybindings[keynum])
	{
		Zone_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}

	// allocate memory for new binding
	l = strlen (binding);
	newbind = (char *) Zone_Alloc (l + 1);
	strcpy (newbind, binding);
	newbind[l] = 0;
	keybindings[keynum] = newbind;
}


/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f (void)
{
	int		b;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (1));

	if (b == -1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv (1));
		return;
	}

	Key_SetBinding (b, "");
}

void Key_Unbindall_f (void)
{
	int		i;

	for (i = 0; i < 256; i++)
		if (keybindings[i])
			Key_SetBinding (i, "");
}


/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f (void)
{
	int			i, c, b;
	char		cmd[1024];

	c = Cmd_Argc();

	if (c != 2 && c != 3)
	{
		Con_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (1));

	if (b == -1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv (1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[b])
			Con_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv (1), keybindings[b]);
		else
			Con_Printf ("\"%s\" is not bound\n", Cmd_Argv (1));

		return;
	}

	// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string

	for (i = 2; i < c; i++)
	{
		if (i > 2)
			strcat (cmd, " ");

		strcat (cmd, Cmd_Argv (i));
	}

	Key_SetBinding (b, cmd);
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings (FILE *f)
{
	for (int i = 0; i < 256; i++)
	{
		if (keybindings[i])
		{
			if (*keybindings[i])
			{
				fprintf (f, "bind \"%s\" \"%s\"\n", Key_KeynumToString (i), keybindings[i]);
			}
		}
	}
}


int Key_GetBinding (char *cmd)
{
	for (int i = 0; i < 256; i++)
	{
		if (keybindings[i])
		{
			if (*keybindings[i])
			{
				if (!_stricmp (cmd, keybindings[i]))
				{
					return i;
				}
			}
		}
	}

	return -1;
}


/*
===================
Key_Init
===================
*/
cmd_t Key_Bind_f_Cmd ("bind", Key_Bind_f);
cmd_t Key_Unbind_f_Cmd ("unbind", Key_Unbind_f);
cmd_t Key_Unbindall_f_Cmd ("unbindall", Key_Unbindall_f);

void Key_Init (void)
{
	int		i;

	// load up the history
	Key_HistoryLoad ();

	// init ascii characters in console mode
	for (i = 32; i < 128; i++)
		consolekeys[i] = true;

	consolekeys[K_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_DEL] = true;
	consolekeys[K_INS] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (i = 0; i < 256; i++)
		keyshift[i] = i;

	for (i = 'a'; i <= 'z'; i++)
		keyshift[i] = i - 'a' + 'A';

	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;

	for (i = 0; i < 12; i++)
		menubound[K_F1+i] = true;
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Menu_ToggleMenu (void);
void M_Keydown (int key);
void HUD_ShowDemoScores (void);

void Key_Event (int key, bool down)
{
	char	*kb;
	char	cmd[1024];

	keydown[key] = down;

	if (!down) key_repeats[key] = 0;

	key_lastpress = key;
	key_count++;

	if (key_count <= 0)
	{
		// just catching keys for Con_NotifyBox
		// (this function doesn't exist anymore, but we leave the
		// check in case it has unknown consequences or assumptions
		// are made that it still exists elsewhere).
		// OK, it's also in SCR_ModalMessage
		return;
	}

	// update auto-repeat status
	if (down)
	{
		key_repeats[key]++;

		if (key != K_BACKSPACE && key != K_PAUSE && key_repeats[key] > 1 && key_dest == key_game)
		{
			// ignore most autorepeats in-game
			return;
		}

		// only display this in-game
		if (key >= 200 && !keybindings[key] && key_dest == key_game)
			Con_Printf ("%s is unbound, hit F4 to set.\n", Key_KeynumToString (key));
	}

	if (key == K_SHIFT) shift_down = down;
	if (key == K_ALT) alt_down = down;
	if (key == K_CTRL) ctrl_down = down;

	// handle escape specialy, so the user can never unbind it
	if (key == K_ESCAPE)
	{
		if (!down)
			return;

		switch (key_dest)
		{
		case key_message:
			Key_Message (key);
			break;

		case key_menu:
			M_Keydown (key);
			break;

		case key_game:
		case key_console:
			Menu_ToggleMenu ();
			break;

		default:
			Sys_Error ("Bad key_dest");
		}

		return;
	}

	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	if (!down)
	{
		kb = keybindings[key];

		if (kb && kb[0] == '+')
		{
			_snprintf (cmd, 1024, "-%s %i\n", kb + 1, key);
			Cbuf_AddText (cmd);
		}

		if (keyshift[key] != key)
		{
			kb = keybindings[keyshift[key]];

			if (kb && kb[0] == '+')
			{
				_snprintf (cmd, 1024, "-%s %i\n", kb + 1, key);
				Cbuf_AddText (cmd);
			}
		}

		return;
	}

	// during demo playback, most keys bring up the main menu
	if (cls.demoplayback && down && consolekeys[key] && key_dest == key_game)
	{
		if (key == K_TAB)
			HUD_ShowDemoScores ();
		else Menu_ToggleMenu ();

		return;
	}

	// if not a consolekey, send to the interpreter no matter what mode is
	if ((key_dest == key_menu && menubound[key]) ||
		(key_dest == key_console && !consolekeys[key]) ||
		(key_dest == key_game && (cls.maprunning || !consolekeys[key])))
	{
		kb = keybindings[key];

		if (kb)
		{
			if (kb[0] == '+')
			{
				// button commands add keynum as a parm
				_snprintf (cmd, 1024, "%s %i\n", kb, key);
				Cbuf_AddText (cmd);
			}
			else
			{
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}

		return;
	}

	if (!down)
		return;		// other systems only care about key down events

	if (shift_down)
		key = keyshift[key];

	switch (key_dest)
	{
	case key_message:
		Key_Message (key);
		break;
	case key_menu:
		M_Keydown (key);
		break;

	case key_game:
	case key_console:
		Key_Console (key);
		break;

	default:
		Sys_Error ("Bad key_dest");
	}
}


/*
===================================================================================================================================

						STUFF PORTED FROM VIDNT CODE

===================================================================================================================================
*/

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int		i;

	// send an up event for each key, to make sure the server clears them all
	for (i = 0; i < 256; i++)
	{
		// trigger the up action for all down keys
		if (keydown[i] || key_repeats[i])
			Key_Event (i, false);

		keydown[i] = false;
		key_repeats[i] = 0;
	}

	shift_down = false;
	ctrl_down = false;
	alt_down = false;

	IN_ClearMouseState ();
}


int Key_ModifyKey (int key)
{
	// non-printable
	bool passed = !((key & 127) < 32);

	// Is either printable or printable if extended by CTRL or ALT sequence
	if (ctrl_down)
	{
		if (key >= '0' && key <= '9')
		{
			key = key - '0' + 0x12;	// yellow number
			passed = true;
		}
		else
		{
			switch (key)
			{
			case '[': key = 0x10; passed = true; break;
			case ']': key = 0x11; passed = true; break;
			case 'g': key = 0x86; passed = true; break;
			case 'r': key = 0x87; passed = true; break;
			case 'y': key = 0x88; passed = true; break;
			case 'b': key = 0x89; passed = true; break;
			case '(': key = 0x80; passed = true; break;
			case '=': key = 0x81; passed = true; break;
			case ')': key = 0x82; passed = true; break;
			case 'a': key = 0x83; passed = true; break;
			case '<': key = 0x1d; passed = true; break;
			case '-': key = 0x1e; passed = true; break;
			case '>': key = 0x1f; passed = true; break;
			case ',': key = 0x1c; passed = true; break;
			case '.': key = 0x9c; passed = true; break;
			case 'B': key = 0x8b; passed = true; break;
			case 'C': key = 0x8d; passed = true; break;
			}
		}
	}

	// bronzing
	if (alt_down && passed) key |= 128;

	if (!passed)
		return 0;
	else return key;
}

