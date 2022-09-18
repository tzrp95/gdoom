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

#include "sys/platform.h"
#include "renderer/RenderWorld.h"

#include "gamesys/SysCvar.h"
#include "gamesys/SaveGame.h"
#include "GameBase.h"
#include "Player.h"
#include "Weapon.h"

#include "PlayerView.h"

static int MakePowerOfTwo( int num ) {
	int pot;
	for ( pot = 1 ; pot < num ; pot <<= 1 ) { }
	return pot;
}

const int IMPULSE_DELAY = 150;

/*
===============================================================================

	Player View

===============================================================================
*/

/*
==============
idPlayerView::idPlayerView
==============
*/
idPlayerView::idPlayerView() : m_postProcessManager() {
	memset( screenBlobs, 0, sizeof( screenBlobs ) );
	memset( &view, 0, sizeof( view ) );
	player = NULL;

	blackMaterial			= declManager->FindMaterial( "_black" );
	whiteMaterial			= declManager->FindMaterial( "_white" );
	currentRenderMaterial	= declManager->FindMaterial( "_currentRender" );
	scratchMaterial			= declManager->FindMaterial( "_scratch" );

	armorMaterial			= declManager->FindMaterial( "armorViewEffect" );
	tunnelMaterial			= declManager->FindMaterial( "textures/decals/tunnel" );
	berserkMaterial			= declManager->FindMaterial( "textures/decals/berserk" );
	irGogglesMaterial		= declManager->FindMaterial( "textures/decals/irblend" );
	bloodSprayMaterial		= declManager->FindMaterial( "textures/decals/bloodspray" );
	bfgMaterial				= declManager->FindMaterial( "textures/decals/bfgvision" );

	casMaterial				= declManager->FindMaterial( "postProcess/cas" );
	edgeAAMaterial			= declManager->FindMaterial( "postProcess/edgeAA" );

	lagoMaterial = declManager->FindMaterial( LAGO_MATERIAL, false );
	bfgVision = false;
	dvFinishTime = 0;
	kickFinishTime = 0;
	kickAngles.Zero();
	lastDamageTime = 0.0f;
	fadeTime = 0;
	fadeRate = 0.0;
	fadeFromColor.Zero();
	fadeToColor.Zero();
	fadeColor.Zero();
	shakeAng.Zero();
	fxManager = NULL;

	if ( fxManager == NULL ) {
		fxManager = new FullscreenFXManager;
		fxManager->Initialize( this );
	}

	ClearEffects();
}

/*
==============
idPlayerView::~idPlayerView
==============
*/
idPlayerView::~idPlayerView() {
	delete fxManager;
}

/*
==============
idPlayerView::Save
==============
*/
void idPlayerView::Save( idSaveGame *savefile ) const {
	int i;
	const screenBlob_t *blob;

	blob = &screenBlobs[ 0 ];
	for ( i = 0; i < MAX_SCREEN_BLOBS; i++, blob++ ) {
		savefile->WriteMaterial( blob->material );
		savefile->WriteFloat( blob->x );
		savefile->WriteFloat( blob->y );
		savefile->WriteFloat( blob->w );
		savefile->WriteFloat( blob->h );
		savefile->WriteFloat( blob->s1 );
		savefile->WriteFloat( blob->t1 );
		savefile->WriteFloat( blob->s2 );
		savefile->WriteFloat( blob->t2 );
		savefile->WriteInt( blob->finishTime );
		savefile->WriteInt( blob->startFadeTime );
		savefile->WriteFloat( blob->driftAmount );
	}

	savefile->WriteInt( dvFinishTime );
	savefile->WriteMaterial( scratchMaterial );
	savefile->WriteInt( kickFinishTime );
	savefile->WriteAngles( kickAngles );
	savefile->WriteBool( bfgVision );

	savefile->WriteMaterial( armorMaterial );
	savefile->WriteMaterial( tunnelMaterial );
	savefile->WriteMaterial( berserkMaterial );
	savefile->WriteMaterial( irGogglesMaterial );
	savefile->WriteMaterial( bloodSprayMaterial );
	savefile->WriteMaterial( bfgMaterial );
	savefile->WriteFloat( lastDamageTime );

	savefile->WriteVec4( fadeColor );
	savefile->WriteVec4( fadeToColor );
	savefile->WriteVec4( fadeFromColor );
	savefile->WriteFloat( fadeRate );
	savefile->WriteInt( fadeTime );

	savefile->WriteAngles( shakeAng );

	savefile->WriteObject( player );
	savefile->WriteRenderView( view );

	if ( fxManager ) {
		fxManager->Save( savefile );
	}
}

/*
==============
idPlayerView::Restore
==============
*/
void idPlayerView::Restore( idRestoreGame *savefile ) {
	int i;
	screenBlob_t *blob;

	blob = &screenBlobs[ 0 ];
	for ( i = 0; i < MAX_SCREEN_BLOBS; i++, blob++ ) {
		savefile->ReadMaterial( blob->material );
		savefile->ReadFloat( blob->x );
		savefile->ReadFloat( blob->y );
		savefile->ReadFloat( blob->w );
		savefile->ReadFloat( blob->h );
		savefile->ReadFloat( blob->s1 );
		savefile->ReadFloat( blob->t1 );
		savefile->ReadFloat( blob->s2 );
		savefile->ReadFloat( blob->t2 );
		savefile->ReadInt( blob->finishTime );
		savefile->ReadInt( blob->startFadeTime );
		savefile->ReadFloat( blob->driftAmount );
	}

	savefile->ReadInt( dvFinishTime );
	savefile->ReadMaterial( scratchMaterial );
	savefile->ReadInt( kickFinishTime );
	savefile->ReadAngles( kickAngles );
	savefile->ReadBool( bfgVision );

	savefile->ReadMaterial( armorMaterial );
	savefile->ReadMaterial( tunnelMaterial );
	savefile->ReadMaterial( berserkMaterial );
	savefile->ReadMaterial( irGogglesMaterial );
	savefile->ReadMaterial( bloodSprayMaterial );
	savefile->ReadMaterial( bfgMaterial );
	savefile->ReadFloat( lastDamageTime );

	savefile->ReadVec4( fadeColor );
	savefile->ReadVec4( fadeToColor );
	savefile->ReadVec4( fadeFromColor );
	savefile->ReadFloat( fadeRate );
	savefile->ReadInt( fadeTime );

	savefile->ReadAngles( shakeAng );

	savefile->ReadObject( reinterpret_cast<idClass*&>( player ) );
	savefile->ReadRenderView( view );

	if ( fxManager ) {
		fxManager->Restore( savefile );
	}

	// Re-Initialize the PostProcess Manager
	this->m_postProcessManager.Initialize();
}

/*
==============
idPlayerView::SetPlayerEntity
==============
*/
void idPlayerView::SetPlayerEntity( idPlayer *playerEnt ) {
	player = playerEnt;
}

/*
==============
idPlayerView::ClearEffects
==============
*/
void idPlayerView::ClearEffects() {
	lastDamageTime = MS2SEC( gameLocal.slow.time - 99999 );
	dvFinishTime = ( gameLocal.fast.time - 99999 );
	kickFinishTime = ( gameLocal.slow.time - 99999 );

	for ( int i = 0 ; i < MAX_SCREEN_BLOBS ; i++ ) {
		screenBlobs[i].finishTime = gameLocal.slow.time;
	}

	fadeTime = 0;
	bfgVision = false;
}

/*
==============
idPlayerView::GetScreenBlob
==============
*/
screenBlob_t *idPlayerView::GetScreenBlob() {
	screenBlob_t *oldest = &screenBlobs[0];

	for ( int i = 1 ; i < MAX_SCREEN_BLOBS ; i++ ) {
		if ( screenBlobs[i].finishTime < oldest->finishTime ) {
			oldest = &screenBlobs[i];
		}
	}
	return oldest;
}

/*
==============
idPlayerView::DamageImpulse

LocalKickDir is the direction of force in the player's coordinate system,
which will determine the head kick direction
==============
*/
void idPlayerView::DamageImpulse( idVec3 localKickDir, const idDict *damageDef ) {
	//
	// double vision effect
	//
	if ( lastDamageTime > 0.0f && SEC2MS( lastDamageTime ) + IMPULSE_DELAY > gameLocal.slow.time ) {
		// keep shotgun from obliterating the view
		return;
	}

	float dvTime = damageDef->GetFloat( "dv_time" );
	if ( dvTime ) {
		if ( dvFinishTime < gameLocal.fast.time ) {
			dvFinishTime = gameLocal.fast.time;
		}
		dvFinishTime += g_dvTime.GetFloat() * dvTime;
		// don't let it add up too much in god mode
		if ( dvFinishTime > gameLocal.fast.time + 5000 ) {
			dvFinishTime = gameLocal.fast.time + 5000;
		}
	}

	//
	// head angle kick
	//
	float kickTime = damageDef->GetFloat( "kick_time" );
	if ( kickTime ) {
		kickFinishTime = gameLocal.slow.time + g_kickTime.GetFloat() * kickTime;
		// forward / back kick will pitch view
		kickAngles[0] = localKickDir[0];

		// side kick will yaw view
		kickAngles[1] = localKickDir[1] * 0.5f;

		// up / down kick will pitch view
		kickAngles[0] += localKickDir[2];

		// roll will come from side
		kickAngles[2] = localKickDir[1];

		float kickAmplitude = damageDef->GetFloat( "kick_amplitude" );
		if ( kickAmplitude ) {
			kickAngles *= kickAmplitude;
		}
	}

	//
	// screen blob
	//
	float blobTime = damageDef->GetFloat( "blob_time" );
	if ( blobTime ) {
		screenBlob_t *blob = GetScreenBlob();
		blob->startFadeTime = gameLocal.slow.time;
		blob->finishTime = gameLocal.slow.time + blobTime * g_blobTime.GetFloat() * ( ( float )gameLocal.msec / USERCMD_MSEC );

		const char *materialName = damageDef->GetString( "mtr_blob" );
		blob->material = declManager->FindMaterial( materialName );
		blob->x = damageDef->GetFloat( "blob_x" );
		blob->x += ( gameLocal.random.RandomInt() & 63 ) - 32;
		blob->y = damageDef->GetFloat( "blob_y" );
		blob->y += ( gameLocal.random.RandomInt() & 63 ) - 32;

		float scale = ( 256 + ( ( gameLocal.random.RandomInt() & 63 ) - 32 ) ) / 256.0f;
		blob->w = damageDef->GetFloat( "blob_width" ) * g_blobSize.GetFloat() * scale;
		blob->h = damageDef->GetFloat( "blob_height" ) * g_blobSize.GetFloat() * scale;
		blob->s1 = 0;
		blob->t1 = 0;
		blob->s2 = 1;
		blob->t2 = 1;	
	}

	//
	// save lastDamageTime for tunnel vision accentuation
	//
	lastDamageTime = MS2SEC( gameLocal.slow.time );
}

/*
==================
idPlayerView::AddBloodSpray

If we need a more generic way to add blobs then we can do that
but having it localized here lets the material be pre-looked up etc.
==================
*/
void idPlayerView::AddBloodSpray( float duration ) { }

/*
==================
idPlayerView::WeaponFireFeedback

Called when a weapon fires, generates head twitches, etc
==================
*/
void idPlayerView::WeaponFireFeedback( const idDict *weaponDef ) {
	int recoilTime = weaponDef->GetInt( "recoilTime" );

	// don't shorten a damage kick in progress
	if ( recoilTime && kickFinishTime < gameLocal.slow.time ) {
		idAngles angles;
		weaponDef->GetAngles( "recoilAngles", "5 0 0", angles );
		kickAngles = angles;
		int	finish = gameLocal.slow.time + g_kickTime.GetFloat() * recoilTime;
		kickFinishTime = finish;
	}
}

/*
===================
idPlayerView::CalculateShake
===================
*/
void idPlayerView::CalculateShake() {
	/*
	 * shakeVolume should somehow be molded into an angle here
	 * it should be thought of as being in the range 0.0 -> 1.0, although
	 * since CurrentShakeAmplitudeForPosition() returns all the shake sounds
	 * the player can hear, it can go over 1.0 too.
	 */

	float shakeVolume = gameSoundWorld->CurrentShakeAmplitudeForPosition( gameLocal.slow.time, player->firstPersonViewOrigin );
	shakeAng[0] = gameLocal.random.CRandomFloat() * shakeVolume;
	shakeAng[1] = gameLocal.random.CRandomFloat() * shakeVolume;
	shakeAng[2] = gameLocal.random.CRandomFloat() * shakeVolume;
}

/*
===================
idPlayerView::ShakeAxis
===================
*/
idMat3 idPlayerView::ShakeAxis() const {
	return shakeAng.ToMat3();
}

/*
===================
idPlayerView::AngleOffset

kickVector, a world space direction that the attack should
===================
*/
idAngles idPlayerView::AngleOffset() const {
	idAngles ang( 0.0f, 0.0f, 0.0f );

	if ( gameLocal.slow.time < kickFinishTime ) {
		float offset = kickFinishTime - gameLocal.slow.time;
		ang = kickAngles * offset * offset * g_kickAmplitude.GetFloat();

		for ( int i = 0 ; i < 3 ; i++ ) {
			if ( ang[i] > 70.0f ) {
				ang[i] = 70.0f;
			} else if ( ang[i] < -70.0f ) {
				ang[i] = -70.0f;
			}
		}
	}
	return ang;
}

/*
==================
idPlayerView::SingleView
==================
*/
void idPlayerView::SingleView( idUserInterface *hud, const renderView_t *view ) {

	// normal rendering
	if ( !view ) {
		return;
	}

	// place the sound origin for the player
	gameSoundWorld->PlaceListener( view->vieworg, view->viewaxis, player->entityNumber + 1, gameLocal.slow.time, hud ? hud->State().GetString( "location" ) : "Undefined" );

	// if the objective system is up, don't do normal drawing
	if ( player->objectiveSystemOpen ) {
		player->objectiveSystem->Redraw( gameLocal.fast.time );
		return;
	}

	// Sikkpin: PostProccess Scaling Fix --->
	if ( screenHeight != renderSystem->GetScreenHeight() || screenWidth != renderSystem->GetScreenWidth() ) {
		renderSystem->GetGLSettings( screenWidth, screenHeight );
		float f = MakePowerOfTwo( screenWidth );
		shiftScale.x = ( float )screenWidth / f;
		f = MakePowerOfTwo( screenHeight );
		shiftScale.y = ( float )screenHeight / f;
	} // <---

	// hack the shake in at the very last moment, so it can't cause any consistency problems
	hackedView = *view;
	hackedView.viewaxis = hackedView.viewaxis * ShakeAxis();

	if ( gameLocal.portalSkyEnt.GetEntity() && gameLocal.IsPortalSkyAcive() && g_enablePortalSky.GetBool() ) {
		renderView_t portalView = hackedView;
		portalView.vieworg = gameLocal.portalSkyEnt.GetEntity()->GetPhysics()->GetOrigin();

		// setup global fixup projection vars
		hackedView.shaderParms[4] = shiftScale.x;
		hackedView.shaderParms[5] = shiftScale.y;

		gameRenderWorld->RenderScene( &portalView );
		renderSystem->CaptureRenderToImage( "_currentRender" );

		hackedView.forceUpdate = true;	// FIX: for smoke particles not drawing when portalSky present
	}
}

/*
===================
idPlayerView::SreenBlobs
===================
*/
void idPlayerView::ScreenBlobs( void ) {
	for ( int i = 0 ; i < MAX_SCREEN_BLOBS ; i++ ) {
		screenBlob_t	*blob = &screenBlobs[i];
		if ( blob->finishTime <= gameLocal.slow.time ) {
			continue;
		}
		blob->y += blob->driftAmount;

		float fade = ( float )( blob->finishTime - gameLocal.slow.time ) / ( blob->finishTime - blob->startFadeTime );
		if ( fade > 1.0f ) {
			fade = 1.0f;
		}
		if ( fade ) {
			renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, fade );
			renderSystem->DrawStretchPic( blob->x, blob->y, blob->w, blob->h,blob->s1, blob->t1, blob->s2, blob->t2, blob->material );
		}
	}
}

/*
===================
idPlayerView::ArmorPulse

Armor impulse feedback
===================
*/
void idPlayerView::ArmorPulse( void ) {
	float armorPulse = ( gameLocal.fast.time - player->lastArmorPulse ) / 250.0f;

	if ( armorPulse > 0.0f && armorPulse < 1.0f ) {
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f - armorPulse );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, armorMaterial );
	}
}

/*
===================
idPlayerView::TunnelVision
===================
*/
void idPlayerView::TunnelVision( void ) {
	float health = 0.0f;	
	if ( g_testHealthVision.GetFloat() != 0.0f ) {
		health = g_testHealthVision.GetFloat();
	} else {
		health = player->health;
	}
		
	float alpha = health / 100.0f;
	if ( alpha < 0.0f ) {
		alpha = 0.0f;
	}
	if ( alpha > 1.0f ) {
		alpha = 1.0f;
	}

	if ( alpha < 1.0f ) {
		renderSystem->SetColor4( ( player->health <= 0.0f ) ? MS2SEC( gameLocal.slow.time ) : lastDamageTime, 1.0f, 1.0f, ( player->health <= 0.0f ) ? 0.0f : alpha );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, tunnelMaterial );
	}
}

/*
===================
idPlayerView::BerserkVision
===================
*/
void idPlayerView::BerserkVision() {
	int berserkTime = player->inventory.powerupEndTime[ BERSERK ] - gameLocal.time;

	if ( berserkTime > 0 ) {
		// start fading if within 10 seconds of going away
		float alpha = ( berserkTime < 10000 ) ? ( float )berserkTime / 10000 : 1.0f;
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, alpha );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, berserkMaterial );
	}

	int	nWidth = renderSystem->GetScreenWidth() / 2;
	int	nHeight = renderSystem->GetScreenHeight() / 2;

	renderSystem->CaptureRenderToImage( "_currentRender" );

	// if double vision, render to a texture
	renderSystem->CropRenderSize( nWidth, nHeight, true, true );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, shiftScale.x, 0.0f, currentRenderMaterial );
	renderSystem->CaptureRenderToImage( "_scratch" );
	renderSystem->UnCrop();

	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, 1.0f, 0.0f, scratchMaterial );
}

/*
===================
idPlayerView::BFGVision
===================
*/
void idPlayerView::BFGVision( void ) {
	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, bfgMaterial );
}

/*
=================
idPlayerView::Flash

Flashes the player view with the given color
=================
*/
void idPlayerView::Flash( idVec4 color, int time ) {
	Fade( idVec4( 0.0f, 0.0f, 0.0f, 0.0f ), time );
	fadeFromColor = colorWhite;
}

/*
=================
idPlayerView::Fade

Used for level transition fades
Assumes: color.w is 0 or 1
=================
*/
void idPlayerView::Fade( idVec4 color, int time ) {
	SetTimeState ts( player->timeGroup );

	if ( !fadeTime ) {
		fadeFromColor.Set( 0.0f, 0.0f, 0.0f, 1.0f - color[ 3 ] );
	} else {
		fadeFromColor = fadeColor;
	}
	fadeToColor = color;

	if ( time <= 0 ) {
		fadeRate = 0;
		time = 0;
		fadeColor = fadeToColor;
	} else {
		fadeRate = 1.0f / ( float )time;
	}

	if ( gameLocal.realClientTime == 0 && time == 0 ) {
		fadeTime = 1;
	} else {
		fadeTime = gameLocal.realClientTime + time;
	}
}

/*
=================
idPlayerView::ScreenFade
=================
*/
void idPlayerView::ScreenFade() {
	int		msec;
	float	t;

	SetTimeState ts( player->timeGroup );

	if ( !fadeTime ) {
		return;
	}

	msec = fadeTime - gameLocal.realClientTime;
	if ( msec <= 0 ) {
		fadeColor = fadeToColor;
		if ( fadeColor[ 3 ] == 0.0f ) {
			fadeTime = 0;
		}
	} else {
		t = ( float )msec * fadeRate;
		fadeColor = fadeFromColor * t + fadeToColor * ( 1.0f - t );
	}

	if ( fadeColor[ 3 ] != 0.0f ) {
		renderSystem->SetColor4( fadeColor[ 0 ], fadeColor[ 1 ], fadeColor[ 2 ], fadeColor[ 3 ] );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, whiteMaterial );
	}
}

/*
===================
idPlayerView::RenderPlayerView
===================
*/
void idPlayerView::RenderPlayerView( idUserInterface *hud ) {
	const renderView_t *view = player->GetRenderView();

	SingleView( hud, view );

	// process the frame
	if ( !player->objectiveSystemOpen ) {
		fxManager->Process( &hackedView );
	}

	if ( !g_skipViewEffects.GetBool() && !player->objectiveSystemOpen ) {
		PlayerViewManager();
	}

	// if the objective system is up, don't draw hud
	if ( hud && !player->objectiveSystemOpen ) {
		player->DrawHUD( hud );
	}

	ScreenFade();

	if ( net_clientLagOMeter.GetBool() && lagoMaterial && gameLocal.isClient ) {
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 10.0f, 380.0f, 64.0f, 64.0f, 0.0f, 0.0f, 1.0f, 1.0f, lagoMaterial );
	}
}

/*
===================
idPlayerView::PlayerViewManager
===================
*/
void idPlayerView::PlayerViewManager( void ) {

	ScreenBlobs();

	ArmorPulse();

	if ( !gameLocal.inCinematic ) {
		TunnelVision();
	}

	if ( bfgVision ) {
		BFGVision();
	}

	// Berserk now uses vanilla D3 view effects when picked up as a item,
	// and the FullScreenFX when launched from weapon. All works alright, except
	// it is still dvIng the view when launched from gun.
	idWeapon *weapon = player->weapon.GetEntity();
	if ( !weapon->PowerupFromWeapon() && player->PowerUpActive( BERSERK ) ) {
		BerserkVision();
	}

	// HDR, (Denton's Method)
	if ( r_useHDR.GetBool() ) {
		cvarSystem->SetCVarBool( "r_testARBProgram", true );
		this->m_postProcessManager.Update();
	} else {
		cvarSystem->SetCVarBool( "r_testARBProgram", false );
	}

	// Contrast Adaptive Sharpening
	if ( r_useCAS.GetBool() ) {
		PostProcess_CAS();
	}

	// Edge Anti-Aliasing
	int postAA = r_useEdgeAA.GetInteger();
	if ( postAA == 1 || postAA == 2 ) {
		PostProcess_EdgeAA();
	}

	// test a single material drawn over everything
	if ( g_testPostProcess.GetString()[0] ) {
		const idMaterial *mtr = declManager->FindMaterial( g_testPostProcess.GetString(), false );
		if ( !mtr ) {
			common->Printf( "Material not found.\n" );
			g_testPostProcess.SetString( "" );
		} else {
			renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
			renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, mtr );
		}
	}
}

/*
===============================================================================

	FullScreenFX

===============================================================================
*/

/*
===================
idPlayerView::AddWarp
===================
*/
int idPlayerView::AddWarp( idVec3 worldOrigin, float centerx, float centery, float initialRadius, float durationMsec ) {
	FullscreenFX_Warp *fx = ( FullscreenFX_Warp* )( fxManager->FindFX( "warp" ) );

	if ( fx ) {
		fx->EnableGrabber( true );
		return 1;
	}

	return 1;
}

/*
===================
idPlayerView::FreeWarp
===================
*/
void idPlayerView::FreeWarp( int id ) {
	FullscreenFX_Warp *fx = ( FullscreenFX_Warp* )( fxManager->FindFX( "warp" ) );

	if ( fx ) {
		fx->EnableGrabber( false );
		return;
	}
}

/*
==================
FxFader::FxFader
==================
*/
FxFader::FxFader() {
	time = 0;
	state = FX_STATE_OFF;
	alpha = 0.0f;
	msec = 1000;
}

/*
==================
FxFader::SetTriggerState
==================
*/
bool FxFader::SetTriggerState( bool active ) {

	// handle on/off states
	if ( active && state == FX_STATE_OFF ) {
		state = FX_STATE_RAMPUP;
		time = gameLocal.slow.time + msec;
	} else if ( !active && state == FX_STATE_ON ) {
		state = FX_STATE_RAMPDOWN;
		time = gameLocal.slow.time + msec;
	}

	// handle rampup/rampdown states
	if ( state == FX_STATE_RAMPUP ) {
		if ( gameLocal.slow.time >= time ) {
			state = FX_STATE_ON;
		}
	} else if ( state == FX_STATE_RAMPDOWN ) {
		if ( gameLocal.slow.time >= time ) {
			state = FX_STATE_OFF;
		}
	}

	// compute alpha
	switch ( state ) {
		case FX_STATE_ON:		alpha = 1.0f; break;
		case FX_STATE_OFF:		alpha = 0.0f; break;
		case FX_STATE_RAMPUP:	alpha = 1.0f - ( float )( time - gameLocal.slow.time ) / msec; break;
		case FX_STATE_RAMPDOWN:	alpha = ( float )( time - gameLocal.slow.time ) / msec; break;
	}

	if ( alpha > 0.0f ) {
		return true;
	} else {
		return false;
	}
}

/*
==================
FxFader::Save
==================
*/
void FxFader::Save( idSaveGame *savefile ) {
	savefile->WriteInt( time );
	savefile->WriteInt( state );
	savefile->WriteFloat( alpha );
	savefile->WriteInt( msec );
}

/*
==================
FxFader::Restore
==================
*/
void FxFader::Restore( idRestoreGame *savefile ) {
	savefile->ReadInt( time );
	savefile->ReadInt( state );
	savefile->ReadFloat( alpha );
	savefile->ReadInt( msec );
}

/*
==================
FullscreenFX_Helltime::Save
==================
*/
void FullscreenFX::Save( idSaveGame *savefile ) {
	fader.Save( savefile );
}

/*
==================
FullscreenFX_Helltime::Restore
==================
*/
void FullscreenFX::Restore( idRestoreGame *savefile ) {
	fader.Restore( savefile );
}

/*
==================
FullscreenFX_Helltime::Initialize
==================
*/
void FullscreenFX_Helltime::Initialize() {
	acInitMaterials[0] = declManager->FindMaterial( "textures/smf/bloodorb1/ac_init" );
	acInitMaterials[1] = declManager->FindMaterial( "textures/smf/bloodorb2/ac_init" );
	acInitMaterials[2] = declManager->FindMaterial( "textures/smf/bloodorb3/ac_init" );

	acCaptureMaterials[0] = declManager->FindMaterial( "textures/smf/bloodorb1/ac_capture" );
	acCaptureMaterials[1] = declManager->FindMaterial( "textures/smf/bloodorb2/ac_capture" );
	acCaptureMaterials[2] = declManager->FindMaterial( "textures/smf/bloodorb3/ac_capture" );

	acDrawMaterials[0] = declManager->FindMaterial( "textures/smf/bloodorb1/ac_draw" );
	acDrawMaterials[1] = declManager->FindMaterial( "textures/smf/bloodorb2/ac_draw" );
	acDrawMaterials[2] = declManager->FindMaterial( "textures/smf/bloodorb3/ac_draw" );

	crCaptureMaterials[0] = declManager->FindMaterial( "textures/smf/bloodorb1/cr_capture" );
	crCaptureMaterials[1] = declManager->FindMaterial( "textures/smf/bloodorb2/cr_capture" );
	crCaptureMaterials[2] = declManager->FindMaterial( "textures/smf/bloodorb3/cr_capture" );

	crDrawMaterials[0] = declManager->FindMaterial( "textures/smf/bloodorb1/cr_draw" );
	crDrawMaterials[1] = declManager->FindMaterial( "textures/smf/bloodorb2/cr_draw" );
	crDrawMaterials[2] = declManager->FindMaterial( "textures/smf/bloodorb3/cr_draw" );

	clearAccumBuffer = true;
}

/*
==================
FullscreenFX_Helltime::DetermineLevel
==================
*/
int FullscreenFX_Helltime::DetermineLevel() {
	int testfx = g_testHelltimeFX.GetInteger();
	idPlayer *player = fxman->GetPlayer();
	idWeapon *weapon = player->weapon.GetEntity();

	// for testing purposes
	if ( testfx >= 0 && testfx < 3 ) {
		return testfx;
	}

	if ( player != NULL && player->PowerUpActive( INVULNERABILITY ) ) {
		return 2;
	} else

	if ( player != NULL && weapon->PowerupFromWeapon() && player->PowerUpActive( BERSERK ) ) {
		return 1;
	} else
	
	if ( player !=NULL && player->PowerUpActive( HELLTIME ) ) {
		return 0;
	}

	return -1;
}

/*
==================
FullscreenFX_Helltime::Active
==================
*/
bool FullscreenFX_Helltime::Active() {
	if ( gameLocal.inCinematic || gameLocal.isMultiplayer ) {
		return false;
	}

	if ( DetermineLevel() >= 0 ) {
		return true;
	} else {
		if ( fader.GetAlpha() == 0 ) {	// latch the clear flag
			clearAccumBuffer = true;
		}
	}
	
	return false;
}

/*
==================
FullscreenFX_Helltime::AccumPass
==================
*/
void FullscreenFX_Helltime::AccumPass( const renderView_t *view ) {
	int level = DetermineLevel();
	idVec2 shiftScale = fxman->GetShiftScale();

	// for testing
	if ( level < 0 || level > 2 ) {
		level = 0;
	}

	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );

	// capture pass
	if ( clearAccumBuffer ) {
		clearAccumBuffer = false;
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, 1.0f, 0.0f, acInitMaterials[level] );
	} else {
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, 1.0f, 0.0f, acCaptureMaterials[level] );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, shiftScale.x, 0.0f, crCaptureMaterials[level] );
	}

	renderSystem->CaptureRenderToImage( "_accum" );
}

/*
==================
FullscreenFX_Helltime::HighQuality
==================
*/
void FullscreenFX_Helltime::HighQuality() {
	int level = DetermineLevel();
	idVec2 shiftScale = fxman->GetShiftScale();

	// for testing
	if ( level < 0 || level > 2 ) {
		level = 0;
	}

	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );

	// draw pass
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, 1.0f, 0.0f, acDrawMaterials[level] );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, shiftScale.x, 0.0f, crDrawMaterials[level] );
}

/*
==================
FullscreenFX_Helltime::Restore
==================
*/
void FullscreenFX_Helltime::Restore( idRestoreGame *savefile ) {
	FullscreenFX::Restore( savefile );

	// latch the clear flag
	clearAccumBuffer = true;
}

/*
==================
FullscreenFX_Multiplayer::Initialize
==================
*/
void FullscreenFX_Multiplayer::Initialize() {
	acInitMaterials		= declManager->FindMaterial( "textures/smf/multiplayer1/ac_init" );
	acCaptureMaterials	= declManager->FindMaterial( "textures/smf/multiplayer1/ac_capture" );
	acDrawMaterials		= declManager->FindMaterial( "textures/smf/multiplayer1/ac_draw" );
	crCaptureMaterials	= declManager->FindMaterial( "textures/smf/multiplayer1/cr_capture" );
	crDrawMaterials		= declManager->FindMaterial( "textures/smf/multiplayer1/cr_draw" );
	clearAccumBuffer	= true;
}

/*
==================
FullscreenFX_Multiplayer::DetermineLevel
==================
*/
int FullscreenFX_Multiplayer::DetermineLevel() {
	int testfx = g_testMultiplayerFX.GetInteger();
	idPlayer *player = fxman->GetPlayer();
	idWeapon *weapon = gameLocal.GetLocalPlayer()->weapon.GetEntity();

	// for testing purposes
	if ( testfx >= 0 && testfx < 3 ) {
		return testfx;
	}
	if ( player != NULL && player->PowerUpActive( INVULNERABILITY ) ) {
		return 2;
	} else
	
	if ( player != NULL && weapon->PowerupFromWeapon() && player->PowerUpActive( BERSERK ) ) {
		return 0;
	}

	return -1;
}

/*
==================
FullscreenFX_Multiplayer::Active
==================
*/
bool FullscreenFX_Multiplayer::Active() {
	if ( !gameLocal.isMultiplayer && g_testMultiplayerFX.GetInteger() == -1 ) {
		return false;
	}

	if ( DetermineLevel() >= 0 ) {
		return true;
	} else {
		// latch the clear flag
		if ( fader.GetAlpha() == 0 ) {
			clearAccumBuffer = true;
		}
	}

	return false;
}

/*
==================
FullscreenFX_Multiplayer::AccumPass
==================
*/
void FullscreenFX_Multiplayer::AccumPass( const renderView_t *view ) {
	int level = DetermineLevel();
	idVec2 shiftScale = fxman->GetShiftScale();

	// for testing
	if ( level < 0 || level > 2 ) {
		level = 0;
	}
	
	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );

	// capture pass
	if ( clearAccumBuffer ) {
		clearAccumBuffer = false;
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, 1.0f, 0.0f, acInitMaterials );
	} else {
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, 1.0f, 0.0f, acCaptureMaterials );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, shiftScale.x, 0.0f, crCaptureMaterials );
	}

	renderSystem->CaptureRenderToImage( "_accum" );
}

/*
==================
FullscreenFX_Multiplayer::HighQuality
==================
*/
void FullscreenFX_Multiplayer::HighQuality() {
	int level = DetermineLevel();
	idVec2 shiftScale = fxman->GetShiftScale();

	// for testing
	if ( level < 0 || level > 2 ) {
		level = 0;
	}

	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );

	// draw pass
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, 1.0f, 0.0f, acDrawMaterials );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, shiftScale.x, 0.0f, crDrawMaterials );
}

/*
==================
FullscreenFX_Multiplayer::Restore
==================
*/
void FullscreenFX_Multiplayer::Restore( idRestoreGame *savefile ) {
	FullscreenFX::Restore( savefile );

	// latch the clear flag
	clearAccumBuffer = true;
}

/*
==================
FullscreenFX_Warp::Initialize
==================
*/
void FullscreenFX_Warp::Initialize() {
	material = declManager->FindMaterial( "textures/smf/warp" );
	grabberEnabled = false;
	startWarpTime = 0;
}

/*
==================
FullscreenFX_Warp::Active
==================
*/
bool FullscreenFX_Warp::Active() {
	if ( grabberEnabled ) {
		return true;
	}

	return false;
}

/*
==================
FullscreenFX_Warp::Save
==================
*/
void FullscreenFX_Warp::Save( idSaveGame *savefile ) {
	FullscreenFX::Save( savefile );

	savefile->WriteBool( grabberEnabled );
	savefile->WriteInt( startWarpTime );
}

/*
==================
FullscreenFX_Warp::Restore
==================
*/
void FullscreenFX_Warp::Restore( idRestoreGame *savefile ) {
	FullscreenFX::Restore( savefile );

	savefile->ReadBool( grabberEnabled );
	savefile->ReadInt( startWarpTime );
}

/*
==================
FullscreenFX_Warp::DrawWarp
==================
*/
void FullscreenFX_Warp::DrawWarp( WarpPolygon_t wp, float interp ) {
	idVec4 mid1_uv, mid2_uv;
	idVec4 mid1, mid2;
	idVec2 drawPts[6], shiftScale;
	WarpPolygon_t trans;

	trans = wp;
	shiftScale = fxman->GetShiftScale();

	// compute mid points
	mid1 = trans.outer1 * ( interp ) + trans.center * ( 1.0f - interp );
	mid2 = trans.outer2 * ( interp ) + trans.center * ( 1.0f - interp );
	mid1_uv = trans.outer1 * 0.5f + trans.center * 0.5f;
	mid2_uv = trans.outer2 * 0.5f + trans.center * 0.5f;

	// draw [outer1, mid2, mid1]
	drawPts[0].Set( trans.outer1.x, trans.outer1.y );
	drawPts[1].Set( mid2.x, mid2.y );
	drawPts[2].Set( mid1.x, mid1.y );
	drawPts[3].Set( trans.outer1.z, trans.outer1.w );
	drawPts[4].Set( mid2_uv.z, mid2_uv.w );
	drawPts[5].Set( mid1_uv.z, mid1_uv.w );
	for ( int j = 0; j < 3; j++ ) {
		drawPts[ j + 3 ].x *= shiftScale.x;
		drawPts[ j + 3 ].y *= shiftScale.y;
	}
	renderSystem->DrawStretchTri( drawPts[0], drawPts[1], drawPts[2], drawPts[3], drawPts[4], drawPts[5], material );

	// draw [outer1, outer2, mid2]
	drawPts[0].Set( trans.outer1.x, trans.outer1.y );
	drawPts[1].Set( trans.outer2.x, trans.outer2.y );
	drawPts[2].Set( mid2.x, mid2.y );
	drawPts[3].Set( trans.outer1.z, trans.outer1.w );
	drawPts[4].Set( trans.outer2.z, trans.outer2.w );
	drawPts[5].Set( mid2_uv.z, mid2_uv.w );
	for ( int j = 0; j < 3; j++ ) {
		drawPts[ j + 3 ].x *= shiftScale.x;
		drawPts[ j + 3 ].y *= shiftScale.y;
	}
	renderSystem->DrawStretchTri( drawPts[0], drawPts[1], drawPts[2], drawPts[3], drawPts[4], drawPts[5], material );

	// draw [mid1, mid2, center]
	drawPts[0].Set( mid1.x, mid1.y );
	drawPts[1].Set( mid2.x, mid2.y );
	drawPts[2].Set( trans.center.x, trans.center.y );
	drawPts[3].Set( mid1_uv.z, mid1_uv.w );
	drawPts[4].Set( mid2_uv.z, mid2_uv.w );
	drawPts[5].Set( trans.center.z, trans.center.w );
	for ( int j = 0; j < 3; j++ ) {
		drawPts[ j + 3 ].x *= shiftScale.x;
		drawPts[ j + 3 ].y *= shiftScale.y;
	}
	renderSystem->DrawStretchTri( drawPts[0], drawPts[1], drawPts[2], drawPts[3], drawPts[4], drawPts[5], material );
}

/*
==================
FullscreenFX_Warp::HighQuality
==================
*/
void FullscreenFX_Warp::HighQuality() {
	float x1, y1, x2, y2, radius, interp;
	idVec2 center;
	int STEP = 9;
	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );

	interp = ( idMath::Sin( ( float )( gameLocal.slow.time - startWarpTime ) / 1000 ) + 1 ) / 2.0f;
	interp = 0.7f * ( 1 - interp ) + 0.3f * ( interp );

	// draw the warps
	center.x = 320;
	center.y = 240;
	radius = 200;

	for ( float i = 0; i < 360; i += STEP ) {
		// compute the values
		x1 = idMath::Sin( DEG2RAD( i ) );
		y1 = idMath::Cos( DEG2RAD( i ) );

		x2 = idMath::Sin( DEG2RAD( i + STEP ) );
		y2 = idMath::Cos( DEG2RAD( i + STEP ) );

		// add warp polygon
		WarpPolygon_t p;

		p.outer1.x = center.x + x1 * radius;
		p.outer1.y = center.y + y1 * radius;
		p.outer1.z = p.outer1.x / ( float )SCREEN_WIDTH;
		p.outer1.w = 1 - ( p.outer1.y / ( float )SCREEN_HEIGHT );

		p.outer2.x = center.x + x2 * radius;
		p.outer2.y = center.y + y2 * radius;
		p.outer2.z = p.outer2.x / ( float )SCREEN_WIDTH;
		p.outer2.w = 1 - ( p.outer2.y / ( float )SCREEN_HEIGHT );

		p.center.x = center.x;
		p.center.y = center.y;
		p.center.z = p.center.x / ( float )SCREEN_WIDTH;
		p.center.w = 1.0f - ( p.center.y / ( float )SCREEN_HEIGHT );

		// draw it
		DrawWarp( p, interp );
	}
}

/*
==================
FullscreenFX_EnviroSuit::Initialize
==================
*/
void FullscreenFX_EnviroSuit::Initialize() {
	material = declManager->FindMaterial( "textures/smf/enviro_suit" );
}

/*
==================
FullscreenFX_EnviroSuit::Active
==================
*/
bool FullscreenFX_EnviroSuit::Active() {
	idPlayer *player = fxman->GetPlayer();

	if ( player != NULL && player->PowerUpActive( ENVIROSUIT ) ) {
		return true;
	}

	return false;
}

/*
==================
FullscreenFX_EnviroSuit::HighQuality
==================
*/
void FullscreenFX_EnviroSuit::HighQuality() {
	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, material );
}

/*
==================
FullscreenFX_DoubleVision::Initialize
==================
*/
void FullscreenFX_DoubleVision::Initialize() {
	material = declManager->FindMaterial( "textures/smf/doubleVision" );
}

/*
==================
FullscreenFX_DoubleVision::Active
==================
*/
bool FullscreenFX_DoubleVision::Active() {
	if ( gameLocal.fast.time < fxman->GetPlayerView()->dvFinishTime ) {
		return true;
	}

	return false;
}

/*
==================
FullscreenFX_DoubleVision::HighQuality
==================
*/
void FullscreenFX_DoubleVision::HighQuality() {
	int offset = fxman->GetPlayerView()->dvFinishTime - gameLocal.fast.time;
	float scale = offset * g_dvAmplitude.GetFloat();
	idPlayer *player = fxman->GetPlayer();
	idVec2 shiftScale = fxman->GetShiftScale();

	// for testing purposes
	if ( !Active() ) {
		static int test = 0;
		if ( test > 312 ) {
			test = 0;
		}

		offset = test++;
		scale = offset * g_dvAmplitude.GetFloat();
	}

	if ( player == NULL ) {
		return;
	}

	offset *= 2;	// crutch up for higher res

	// set the scale and shift
	if ( scale > 0.5f ) {
		scale = 0.5f;
	}
	float shift = scale * sin( sqrtf( ( float )offset ) * g_dvFrequency.GetFloat() );
	shift = fabs( shift );

	// carry red tint if in berserk mode
	idVec4 color( 1.0f, 1.0f, 1.0f, 1.0f );
	if ( gameLocal.fast.time < player->inventory.powerupEndTime[ BERSERK ] ) {
		color.y = 0.0f;
		color.z = 0.0f;
	}

	if ( !gameLocal.isMultiplayer && ( gameLocal.fast.time < player->inventory.powerupEndTime[ HELLTIME ] || gameLocal.fast.time < player->inventory.powerupEndTime[ INVULNERABILITY ] ) ) {
		color.y = 0.0f;
		color.z = 0.0f;
	}

	renderSystem->SetColor4( color.x, color.y, color.z, 1.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, shift, shiftScale.y, shiftScale.x, 0.0f, material );
	renderSystem->SetColor4( color.x, color.y, color.z, 0.5f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, ( 1 - shift ) * shiftScale.x, 0.0f, material );
}

/*
==================
FullscreenFX_InfluenceVision::Initialize
==================
*/
void FullscreenFX_InfluenceVision::Initialize() {
}

/*
==================
FullscreenFX_InfluenceVision::Active
==================
*/
bool FullscreenFX_InfluenceVision::Active() {
	idPlayer *player = fxman->GetPlayer();

	if ( player != NULL && ( player->GetInfluenceMaterial() || player->GetInfluenceEntity() ) ) {
		return true;
	}

	return false;
}

/*
==================
FullscreenFX_InfluenceVision::HighQuality
==================
*/
void FullscreenFX_InfluenceVision::HighQuality() {
	float distance = 0.0f;
	float pct = 1.0f;
	idVec2 shiftScale = fxman->GetShiftScale();
	idPlayer *player = fxman->GetPlayer();

	if ( player == NULL ) {
		return;
	}

	if ( player->GetInfluenceEntity() ) {
		distance = ( player->GetInfluenceEntity()->GetPhysics()->GetOrigin() - player->GetPhysics()->GetOrigin() ).Length();
		if ( player->GetInfluenceRadius() != 0.0f && distance < player->GetInfluenceRadius() ) {
			pct = distance / player->GetInfluenceRadius();
			pct = 1.0f - idMath::ClampFloat( 0.0f, 1.0f, pct );
		}
	}

	if ( player->GetInfluenceMaterial() ) {
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, pct );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, player->GetInfluenceMaterial() );
	} else if ( player->GetInfluenceEntity() == NULL ) {
		return;
	}
}

/*
==================
FullscreenFX_Bloom::Initialize
==================
*/
void FullscreenFX_Bloom::Initialize() {
	drawMaterial		= declManager->FindMaterial( "textures/smf/bloom2/draw" );
	initMaterial		= declManager->FindMaterial( "textures/smf/bloom2/init" );
	currentIntensity	= 0.0f;
	targetIntensity		= 0.0f;
}

/*
==================
FullscreenFX_Bloom::Active
==================
*/
bool FullscreenFX_Bloom::Active() {
	idPlayer *player = fxman->GetPlayer();

	if ( player != NULL && player->bloomEnabled ) {
		return true;
	}

	return false;
}

/*
==================
FullscreenFX_Bloom::HighQuality
==================
*/
void FullscreenFX_Bloom::HighQuality() {
	float shift = 1.0f;
	idPlayer *player = fxman->GetPlayer();
	idVec2 shiftScale = fxman->GetShiftScale();
	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );

	// if intensity value is different, start the blend
	targetIntensity = g_testBloomIntensity.GetFloat();

	if ( player && player->bloomEnabled ) {
		targetIntensity = player->bloomIntensity;
	}

	float delta = targetIntensity - currentIntensity;
	float step = 0.001f;

	if ( step < fabs( delta ) ) {
		if ( delta < 0.0f ) {
			step = -step;
		}
		currentIntensity += step;
	}

	// draw the blends
	int num = g_testBloomNumPasses.GetInteger();
	for ( int i = 0; i < num; i++ ) {
		float s1 = 0.0f, t1 = 0.0f, s2 = 1.0f, t2 = 1.0f;
		float alpha;

		// do the center scale
		s1 -= 0.5f;
		s1 *= shift;
		s1 += 0.5f;
		s1 *= shiftScale.x;

		t1 -= 0.5f;
		t1 *= shift;
		t1 += 0.5f;
		t1 *= shiftScale.y;

		s2 -= 0.5f;
		s2 *= shift;
		s2 += 0.5f;
		s2 *= shiftScale.x;

		t2 -= 0.5f;
		t2 *= shift;
		t2 += 0.5f;
		t2 *= shiftScale.y;

		// draw it
		if ( num == 1.0f ) {
			alpha = 1.0f;
		} else {
			alpha = 1.0f - ( float )i / ( num - 1.0f );
		}

		renderSystem->SetColor4( alpha, alpha, alpha, 1 );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, s1, t2, s2, t1, drawMaterial );
		shift += currentIntensity;
	}
}

/*
==================
FullscreenFX_Bloom::Save
==================
*/
void FullscreenFX_Bloom::Save( idSaveGame *savefile ) {
	FullscreenFX::Save( savefile );
	savefile->WriteFloat( currentIntensity );
	savefile->WriteFloat( targetIntensity );
}

/*
==================
FullscreenFX_Bloom::Restore
==================
*/
void FullscreenFX_Bloom::Restore( idRestoreGame *savefile ) {
	FullscreenFX::Restore( savefile );
	savefile->ReadFloat( currentIntensity );
	savefile->ReadFloat( targetIntensity );
}

/*
==================
FullscreenFXManager::FullscreenFXManager
==================
*/
FullscreenFXManager::FullscreenFXManager() {
	playerView = NULL;
	blendBackMaterial = NULL;
	shiftScale.Set( 0.0f, 0.0f );
}

/*
==================
FullscreenFXManager::~FullscreenFXManager
==================
*/
FullscreenFXManager::~FullscreenFXManager() {
}

/*
==================
FullscreenFXManager::FindFX
==================
*/
FullscreenFX* FullscreenFXManager::FindFX( idStr name ) {
	for ( int i = 0; i < fx.Num(); i++ ) {
		if ( fx[i]->GetName() == name ) {
			return fx[i];
		}
	}
	return NULL;
}

/*
==================
FullscreenFXManager::CreateFX
==================
*/
void FullscreenFXManager::CreateFX( idStr name, idStr fxtype, int fade ) {
	FullscreenFX *pfx = NULL;

	if ( fxtype == "helltime" ) {
		pfx = new FullscreenFX_Helltime;
	}
	else if ( fxtype == "warp" ) {
		pfx = new FullscreenFX_Warp;
	}
	else if ( fxtype == "envirosuit" ) {
		pfx = new FullscreenFX_EnviroSuit;
	}
	else if ( fxtype == "doublevision" ) {
		pfx = new FullscreenFX_DoubleVision;
	}
	else if ( fxtype == "multiplayer" ) {
		pfx = new FullscreenFX_Multiplayer;
	}
	else if ( fxtype == "influencevision" ) {
		pfx = new FullscreenFX_InfluenceVision;
	}
	else if ( fxtype == "bloom" ) {
		pfx = new FullscreenFX_Bloom;
	}
	else {
		assert( 0 );
	}

	if ( pfx ) {
		pfx->Initialize();
		pfx->SetFXManager( this );
		pfx->SetName( name );
		pfx->SetFadeSpeed( fade );
		fx.Append( pfx );
	}
}

/*
==================
FullscreenFXManager::Initialize
==================
*/
void FullscreenFXManager::Initialize( idPlayerView *pv ) {
	// set the playerview
	playerView = pv;
	blendBackMaterial = declManager->FindMaterial( "textures/smf/blendBack" );

	// allocate the fx
	CreateFX( "helltime", "helltime", 1000 );
	CreateFX( "warp", "warp", 0 );
	CreateFX( "envirosuit", "envirosuit", 500 );
	CreateFX( "doublevision", "doublevision", 0 );
	CreateFX( "multiplayer", "multiplayer", 1000 );
	CreateFX( "influencevision", "influencevision", 1000 );
	CreateFX( "bloom", "bloom", 0 );

	// pre-cache the texture grab so we dont hitch
	renderSystem->CropRenderSize( 512, 512, true );
	renderSystem->CaptureRenderToImage( "_accum" );
	renderSystem->UnCrop();

	renderSystem->CropRenderSize( 512, 256, true );
	renderSystem->CaptureRenderToImage( "_scratch" );
	renderSystem->UnCrop();

	renderSystem->CaptureRenderToImage( "_currentRender" );
}

/*
==================
FullscreenFXManager::Blendback
==================
*/
void FullscreenFXManager::Blendback( float alpha ) {
	// alpha fade
	if ( alpha < 1.0f ) {
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f - alpha );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, shiftScale.x, 0.0f, blendBackMaterial );
	}
}

/*
==================
FullscreenFXManager::Save
==================
*/
void FullscreenFXManager::Save( idSaveGame *savefile ) {
	savefile->WriteVec2( shiftScale );

	for ( int i = 0; i < fx.Num(); i++ ) {
		FullscreenFX *pfx = fx[i];
		pfx->Save( savefile );
	}
}

/*
==================
FullscreenFXManager::Restore
==================
*/
void FullscreenFXManager::Restore( idRestoreGame *savefile ) {
	savefile->ReadVec2( shiftScale );

	for ( int i = 0; i < fx.Num(); i++ ) {
		FullscreenFX *pfx = fx[i];
		pfx->Restore( savefile );
	}
}

/*
==================
FullscreenFXManager::Process
==================
*/
void FullscreenFXManager::Process( const renderView_t *view ) {
	bool allpass = false;
	if ( g_testFullscreenFX.GetInteger() == -2 ) {
		allpass = true;
	}

	// compute the shift scale
	int vidWidth, vidHeight;
	renderSystem->GetGLSettings( vidWidth, vidHeight );

	float pot;
	int	 w = vidWidth;
	pot = MakePowerOfTwo( w );
	shiftScale.x = ( float )w / pot;

	int	 h = vidHeight;
	pot = MakePowerOfTwo( h );
	shiftScale.y = ( float )h / pot;
	

	// do the first render
	gameRenderWorld->RenderScene( view );

	// do the process
	for ( int i = 0; i < fx.Num(); i++ ) {
		FullscreenFX *pfx = fx[i];
		bool drawIt = false;

		// determine if we need to draw
		if ( pfx->Active() || g_testFullscreenFX.GetInteger() == i || allpass ) {
			drawIt = pfx->SetTriggerState( true );
		} else {
			drawIt = pfx->SetTriggerState( false );
		}

		// do the actual drawing
		if ( drawIt ) {
			// we need to dump to _currentRender
			renderSystem->CaptureRenderToImage( "_currentRender" );

			// handle the accum pass if we have one
			if ( pfx->HasAccum() ) {
				// we need to crop the accum pass
				renderSystem->CropRenderSize( 512, 512, true );
				pfx->AccumPass( view );
				renderSystem->CaptureRenderToImage( "_accum" );
				renderSystem->UnCrop();	
			}

			// do the high quality pass
			pfx->HighQuality();

			// do the blendback
			Blendback( pfx->GetFadeAlpha() );
		}
	}
}

/*
===============================================================================

	PostProcess Rendering Features

 * Everything under should be moved to the renderer.

===============================================================================
*/

/*
===================
dnPostProcessManager::dnPostProcessManager
===================
*/
idPlayerView::dnPostProcessManager::dnPostProcessManager():
	m_imageCurrentRender(				"_currentRender" ),
	m_imageCurrentRender8x8DownScaled(	"_RTtoTextureScaled64x" ),
	m_imageLuminance64x64(				"_luminanceTexture64x64" ),
	m_imageluminance4x4(				"_luminanceTexture4x4" ),
	m_imageAdaptedLuminance1x1(			"_adaptedLuminance" ),
	m_imageBloom(						"_bloomImage" ),
	m_imageHalo(						"_haloImage" ),
	m_imageCookedMath(					"_cookedMath" ),

	m_matCookMath_pass1(				declManager->FindMaterial( "postProcess/cookMath_pass1" ) ),
	m_matCookMath_pass2(				declManager->FindMaterial( "postProcess/cookMath_pass2" ) ),
	m_matCookMath_pass3(				declManager->FindMaterial( "postProcess/cookMath_pass3" ) ),
	m_matAvgLuminance64x(				declManager->FindMaterial( "postProcess/averageLum64" )	),
	m_matAvgLumSample4x4(				declManager->FindMaterial( "postProcess/averageLum4" ) ),
	m_matAdaptLuminance(				declManager->FindMaterial( "postProcess/adaptLum" ) ),
	m_matBrightPass(					declManager->FindMaterial( "postProcess/brightPassOptimized" ) ),
	m_matGaussBlurX(					declManager->FindMaterial( "postProcess/blurx" ) ),
	m_matGaussBlurY(					declManager->FindMaterial( "postProcess/blury" ) ),
	m_matHalo(							declManager->FindMaterial( "postProcess/halo" ) ),
	m_matGaussBlurXHalo(				declManager->FindMaterial( "postProcess/blurx_halo" ) ),
	m_matGaussBlurYHalo(				declManager->FindMaterial( "postProcess/blury_halo" ) ),
	m_matFinalScenePass(				declManager->FindMaterial( "postProcess/finalScenePassOptimized" ) ),
	m_matCookVignette(					declManager->FindMaterial( "postProcess/cookVignette" ) ),

	// Materials for debugging intermediate textures
	m_matDecodedLumTexture64x64(		declManager->FindMaterial( "postProcess/decode_luminanceTexture64x64" ) ), 
	m_matDecodedLumTexture4x4(			declManager->FindMaterial( "postProcess/decode_luminanceTexture4x4" ) ),
	m_matDecodedAdaptLuminance(			declManager->FindMaterial( "postProcess/decode_adaptedLuminance" ) )
{
	m_iScreenHeight = m_iScreenWidth = 0;
	m_iScreenHeightPow2 = m_iScreenWidthPow2 = 0;
	m_fShiftScale_x = m_fShiftScale_y = 0;
	m_bForceUpdateOnCookedData = false;
	m_nFramesSinceLumUpdate = 0;
	m_nFramesToUpdateCookedData = 0;

	// Initialize once this object is created.	
	this->Initialize();

	if ( !common->SetCallback( idCommon::CB_ReloadImages, ( idCommon::FunctionPointer )ReloadImagesCallback, this ) ) {
		gameLocal.Warning( "Couldn't set ReloadImages Callback! This could lead to errors on vid_restart and similar!\n" );
	}
}

/*
===================
dnPostProcessManager::~dnPostProcessManager
===================
*/
idPlayerView::dnPostProcessManager::~dnPostProcessManager() {
	// remove callback because this object is destroyed (and this was passed as userArg)
	common->SetCallback( idCommon::CB_ReloadImages, NULL, NULL );
}

/*
===================
idPlayerView::~dnPostProcessManager::ReloadImagesCallback

Called by the engine whenever vid_restart or reloadImages is executed.
Replaces old sourcehook hack.
===================
*/
void idPlayerView::dnPostProcessManager::ReloadImagesCallback( void *arg, const idCmdArgs &cmdArgs) {
	idPlayerView::dnPostProcessManager *self = ( idPlayerView::dnPostProcessManager* )arg;
	self->m_nFramesToUpdateCookedData = 1;

	if ( r_useHDR.GetBool() ) {
		gameLocal.Printf( "Cooked Data will be updated after %d frames...\n", self->m_nFramesToUpdateCookedData );
	}
}

/*
===================
dnPostProcessManager::Initialize
===================
*/
void idPlayerView::dnPostProcessManager::Initialize() {
	m_bForceUpdateOnCookedData = true;

	// Make sure that we always measure luminance at first frame. 
	m_nFramesSinceLumUpdate	= r_hdrLumUpdateRate.GetInteger();
	r_useHDR.SetModified();
}

/*
===================
dnPostProcessManager::Update
===================
*/
void idPlayerView::dnPostProcessManager::Update( void ) {
	static const float fBloomImageDownScale = 4.0f;
	static const float fHaloImageDownScale = 8.0f;
	static const float fBackbufferLumDownScale = 8.0f;

	this->UpdateBackBufferParameters();

	renderSystem->CaptureRenderToImage( m_imageCurrentRender );
	this->UpdateCookedData();

	// Delayed luminance measurement and adaptation for performance improvement.
	if ( r_hdrEyeAdaptationBias.GetFloat() > 0.0f ) {

		if ( m_nFramesSinceLumUpdate >= r_hdrLumUpdateRate.GetInteger() ) {
			//-------------------------------------------------
			// Downscale 
			//-------------------------------------------------
			renderSystem->CropRenderSize( m_iScreenWidthPow2 / fBackbufferLumDownScale, m_iScreenHeightPow2 / fBackbufferLumDownScale, true );
			renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
			renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, m_fShiftScale_y, m_fShiftScale_x, 0, m_imageCurrentRender );
			renderSystem->CaptureRenderToImage( m_imageCurrentRender8x8DownScaled );
			renderSystem->UnCrop();
			//-------------------------------------------------
			// Measure Luminance from Downscaled Image
			//-------------------------------------------------
			renderSystem->CropRenderSize( 64, 64, true );
			renderSystem->SetColor4( 1.0f / Min( 192.0f, m_iScreenWidthPow2 / fBackbufferLumDownScale), 1.0f, 1.0f, 1.0f );			 
			renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matAvgLuminance64x );
			renderSystem->CaptureRenderToImage( m_imageLuminance64x64 );
			renderSystem->UnCrop();
			//-------------------------------------------------
			// Average out the luminance of the scene to a 4x4 Texture
			//-------------------------------------------------
			renderSystem->CropRenderSize( 4, 4, true );
			renderSystem->SetColor4( 1.0f / 16.0f, 1.0f, 1.0f, 1.0f );			 
			renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matAvgLumSample4x4 );
			renderSystem->CaptureRenderToImage( m_imageluminance4x4 );
			renderSystem->UnCrop();

			// Reset vars
			m_nFramesSinceLumUpdate	= 1;
		} else {
			m_nFramesSinceLumUpdate ++;
		}

		//-------------------------------------------------
		// Adapt to the newly calculated Luminance from previous Luminance.
		//-------------------------------------------------
		renderSystem->CropRenderSize( 1, 1, true );
		renderSystem->SetColor4( ( gameLocal.time - gameLocal.previousTime) / ( 1000.0f * r_hdrEyeAdaptationRate.GetFloat() ), r_hdrMaxLuminance.GetFloat(), r_hdrMinLuminance.GetFloat(), 1.0f );
		renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matAdaptLuminance );
		renderSystem->CaptureRenderToImage( m_imageAdaptedLuminance1x1 );
		renderSystem->UnCrop();
		//---------------------
	}

	const float fHDRBloomIntensity = r_hdrBloomIntensity.GetFloat();
	const float fHDRHaloIntensity = fHDRBloomIntensity > 0.0f ? r_hdrHaloIntensity.GetFloat() : 0.0f;

	if ( fHDRBloomIntensity > 0.0f ) {
		//-------------------------------------------------
		// Apply the bright-pass filter to acquire bloom image
		//-------------------------------------------------
		renderSystem->CropRenderSize( m_iScreenWidthPow2 / fBloomImageDownScale, m_iScreenHeightPow2 / fBloomImageDownScale, true );
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matBrightPass );
		renderSystem->CaptureRenderToImage( m_imageBloom );

		//-------------------------------------------------
		// Apply Gaussian Smoothing to create bloom
		//-------------------------------------------------
		renderSystem->SetColor4( fBloomImageDownScale / m_iScreenWidthPow2, 1.0f, 1.0f, 1.0f );			 
		renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matGaussBlurX );
		renderSystem->CaptureRenderToImage( m_imageBloom );
		renderSystem->SetColor4( fBloomImageDownScale / m_iScreenHeightPow2, 1.0f, 1.0f, 1.0f );		 
		renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matGaussBlurY );
		renderSystem->CaptureRenderToImage( m_imageBloom );
		renderSystem->UnCrop();

		if ( fHDRHaloIntensity > 0.0f ) {
			//-------------------------------------------------
			// Downscale bloom image and blur again to obtain Halo Image
			//-------------------------------------------------
			renderSystem->CropRenderSize( m_iScreenWidthPow2 / fHaloImageDownScale, m_iScreenHeightPow2 / fHaloImageDownScale, true );
			renderSystem->SetColor4( fHaloImageDownScale / m_iScreenWidthPow2, 1.0f, 1.0f, 1.0f );			 
			renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matGaussBlurXHalo );
			renderSystem->CaptureRenderToImage( m_imageHalo );
			renderSystem->SetColor4( fHaloImageDownScale / m_iScreenHeightPow2, 1.0f, 1.0f, 1.0f );		 
			renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matGaussBlurYHalo );
			renderSystem->CaptureRenderToImage( m_imageHalo );
			renderSystem->UnCrop();
		}
	}

	//-------------------------------------------------
	// Calculate and Render Final Image
	//-------------------------------------------------
	renderSystem->SetColor4( fHDRBloomIntensity, fHDRHaloIntensity, .5f, 1.0f );
	renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, m_fShiftScale_y, m_fShiftScale_x, 0, m_matFinalScenePass );

	this->RenderDebugTextures();
}

/*
===================
dnPostProcessManager::UpdateBackBufferParameters
===================
*/
void idPlayerView::dnPostProcessManager::UpdateBackBufferParameters() {

	// This condition makes sure that, the 2 loops inside run once only when resolution changes or map starts.
	if ( m_iScreenHeight != renderSystem->GetScreenHeight() || m_iScreenWidth !=renderSystem->GetScreenWidth() ) {

		m_iScreenHeightPow2 = 256, m_iScreenWidthPow2 = 256;

		// This should probably fix the ATI issue...
		renderSystem->GetGLSettings( m_iScreenWidth, m_iScreenHeight );

		while ( m_iScreenWidthPow2 < m_iScreenWidth ) {
			m_iScreenWidthPow2 <<= 1;
		}
		while ( m_iScreenHeightPow2 < m_iScreenHeight ) {
			m_iScreenHeightPow2 <<= 1;
		}
		m_fShiftScale_x = m_iScreenWidth / ( float )m_iScreenWidthPow2;
		m_fShiftScale_y = m_iScreenHeight / ( float )m_iScreenHeightPow2;
	}
}

/*
===================
dnPostProcessManager::UpdateCookedData
===================
*/
void idPlayerView::dnPostProcessManager::UpdateCookedData( void ) {

	if ( m_nFramesToUpdateCookedData > 0 ) {
		m_nFramesToUpdateCookedData--;
		m_bForceUpdateOnCookedData = true;
		return;
	}

	if	(	m_bForceUpdateOnCookedData					|| 
			r_hdrSceneExposure.IsModified()				||
			r_hdrMiddleGray.IsModified()				||
			r_hdrColorCurveBias.IsModified()			||
			r_hdrMaxColorIntensity.IsModified()			|| 
			r_hdrGammaCorrection.IsModified()			||
			r_hdrBrightPassOffset.IsModified()			|| 
			r_hdrBrightPassThreshold.IsModified()		||
			r_hdrEyeAdaptationBias.IsModified()			||
			r_hdrEyeAdaptationBloomBias.IsModified()	||
			r_hdrVignetteBias.IsModified()
		)
	{

		if ( m_bForceUpdateOnCookedData ) {
			gameLocal.Printf( "Forcing an update on cooked math data.\n" );
		}
		gameLocal.Printf( "Cooking math data please wait...\n" );

		//------------------------------------------------------------------------
		// Crop backbuffer image to the size of our cooked math image
		//------------------------------------------------------------------------
		renderSystem->CropRenderSize( 1024, 256, true );
		//------------------------------------------------------------------------

		// Changed from max to Max for cross platform compiler compatibility.
		const float fMaxColorIntensity = Max( r_hdrMaxColorIntensity.GetFloat(), 0.00001f );

		//------------------------------------------------------------------------
		// Cook math Pass 1 
		//------------------------------------------------------------------------
		renderSystem->SetColor4( r_hdrMiddleGray.GetFloat(), 1.0f / fMaxColorIntensity, r_hdrSceneExposure.GetFloat(), r_hdrEyeAdaptationBias.GetFloat() );
		renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matCookMath_pass1 );
		renderSystem->CaptureRenderToImage( m_imageCookedMath );

		//------------------------------------------------------------------------
		// Cook math Pass 2 
		//------------------------------------------------------------------------
 		renderSystem->SetColor4( r_hdrMiddleGray.GetFloat(), r_hdrBrightPassThreshold.GetFloat(), r_hdrBrightPassOffset.GetFloat(), r_hdrEyeAdaptationBloomBias.GetFloat() );
 		renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matCookMath_pass2 );
 		renderSystem->CaptureRenderToImage( m_imageCookedMath );

		//------------------------------------------------------------------------
		// Cook math Pass 3 
		//------------------------------------------------------------------------
		renderSystem->SetColor4( r_hdrColorCurveBias.GetFloat(), r_hdrGammaCorrection.GetFloat(), 0.0f, 0.0f );
		renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matCookMath_pass3 );
		renderSystem->CaptureRenderToImage( m_imageCookedMath );

		//------------------------------------------------------------------------
		// Cooke Vignette image 
		//------------------------------------------------------------------------
		renderSystem->SetColor4( r_hdrVignetteBias.GetFloat(), 1.0f / m_fShiftScale_x, 1.0f / m_fShiftScale_y, 0.0f );
		renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, 1, 0, m_matCookVignette );
		renderSystem->CaptureRenderToImage( m_imageCookedMath );

		//------------------------------------------------------------------------
		renderSystem->UnCrop();
		//------------------------------------------------------------------------

		r_hdrSceneExposure.ClearModified();
		r_hdrMiddleGray.ClearModified();
		r_hdrColorCurveBias.ClearModified(); 
		r_hdrMaxColorIntensity.ClearModified();
		r_hdrGammaCorrection.ClearModified();
		r_hdrEyeAdaptationBias.ClearModified();
		r_hdrEyeAdaptationBloomBias.ClearModified();
		r_hdrBrightPassOffset.ClearModified();
		r_hdrBrightPassThreshold.ClearModified();
		r_hdrVignetteBias.ClearModified();

		m_bForceUpdateOnCookedData = false;

		gameLocal.Printf( "Cooking complete.\n" );
	}
}

/*
===================
dnPostProcessManager::RenderDebugTextures
===================
*/
void idPlayerView::dnPostProcessManager::RenderDebugTextures() {

	const int iDebugMode = r_hdrDebugMode.GetInteger();
	const int iDebugTexture = r_hdrDebugTextureIndex.GetInteger();

	if ( 1 == iDebugMode ) {
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 0,				0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, 1, 1, 0, m_imageCurrentRender8x8DownScaled );
		renderSystem->DrawStretchPic( SCREEN_WIDTH/5,	0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, 1, 1, 0, m_imageLuminance64x64 );
		renderSystem->DrawStretchPic( SCREEN_WIDTH*2/5, 0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, 1, 1, 0, m_imageluminance4x4 );
		renderSystem->DrawStretchPic( SCREEN_WIDTH*3/5, 0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, 1, 1, 0, m_imageAdaptedLuminance1x1 );
		renderSystem->DrawStretchPic( SCREEN_WIDTH*4/5, 0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, m_fShiftScale_y, m_fShiftScale_x, 0, m_imageBloom );

	}
	else if ( 2 == iDebugMode ) {
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 0,				0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, 1, 1, 0, m_imageCurrentRender8x8DownScaled );
		renderSystem->DrawStretchPic( SCREEN_WIDTH/5,	0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, 1, 1, 0, m_matDecodedLumTexture64x64 );
		renderSystem->DrawStretchPic( SCREEN_WIDTH*2/5, 0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, 1, 1, 0, m_matDecodedLumTexture4x4 );
		renderSystem->DrawStretchPic( SCREEN_WIDTH*3/5, 0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, 1, 1, 0, m_matDecodedAdaptLuminance );
		renderSystem->DrawStretchPic( SCREEN_WIDTH*4/5, 0, SCREEN_WIDTH/6,	SCREEN_HEIGHT/6, 0, m_fShiftScale_y, m_fShiftScale_x, 0, m_imageBloom );
	}

	if ( 0!= iDebugMode && 0 < iDebugTexture && 5 > iDebugTexture ) {
		struct {
			dnImageWrapper *m_pImage;
			float m_fShiftScaleX, m_fShiftScaleY;
		} 
		const arrStretchedImages[4] = { 
				{ &m_imageCurrentRender8x8DownScaled, 1, 1},
				{ &m_imageBloom,		m_fShiftScale_x, m_fShiftScale_y },
				{ &m_imageHalo,			m_fShiftScale_x, m_fShiftScale_y },
				{ &m_imageCookedMath,	1, 1 },
		};
		
		int i = iDebugTexture - 1;
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
 		renderSystem->DrawStretchPic( 0, SCREEN_HEIGHT * .2f, SCREEN_WIDTH * 0.6f, SCREEN_HEIGHT * 0.6f, 0, 
					arrStretchedImages[i].m_fShiftScaleY, arrStretchedImages[i].m_fShiftScaleX, 0, 
					*arrStretchedImages[i].m_pImage );
	}
}

/*
===================
dnPostProcessManager::UpdateInteractionShader
===================
*/
void idPlayerView::dnPostProcessManager::UpdateInteractionShader() {
	// Check the CVARs.
	if ( r_useHDR.GetBool() ) {
		cvarSystem->SetCVarInteger( "r_testARBProgram", 1 );
		gameLocal.Printf( "HDR enabled.\n" );
	} else {
		cvarSystem->SetCVarInteger( "r_testARBProgram", 0 );
		gameLocal.Printf( "HDR disabled.\n" );
	}
}

/*
===================
idPlayerView::PostProcess_CAS
===================
*/
void idPlayerView::PostProcess_CAS( void ) {
	renderSystem->CaptureRenderToImage( "_currentRender" );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, shiftScale.x, 0.0f, casMaterial );
}

/*
===================
idPlayerView::PostProcess_EdgeAA
===================
*/
void idPlayerView::PostProcess_EdgeAA( void ) {
	renderSystem->CaptureRenderToImage( "_currentRender" );
	renderSystem->SetColor4( r_edgeAASampleScale.GetFloat(), r_edgeAAFilterScale.GetFloat(), 1.0f, r_useEdgeAA.GetFloat() );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, edgeAAMaterial );
}