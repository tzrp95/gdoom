/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#ifndef __GAME_ENDLEVEL_H__
#define __GAME_ENDLEVEL_H__

#include "Entity.h"

class idTarget_EndLevel : public idEntity {
public:
	CLASS_PROTOTYPE( idTarget_EndLevel );

	void				Spawn( void );
						~idTarget_EndLevel();

	// the endLevel will be responsible for drawing the entire screen
	// when it is active
	void				Draw();

	// when an endlevel is active, player buttons get sent here instead
	// of doing anything to the player, which will allow moving to
	// the next level
	void				PlayerCommand( int buttons );


	// the game will check this each frame, and return it to the
	// session when there is something to give
	const char *		ExitCommand();


private:
	idStr				exitCommand;

	idVec3				initialViewOrg;
	idVec3				initialViewAngles;
	
	idUserInterface		*gui;
	bool				noGui;

	// don't skip out until buttons are released, then pressed
	bool				buttonsReleased;

	// set when the player triggers the exit
	bool				readyToExit;

	void				Event_Trigger( idEntity *activator );
};

#endif