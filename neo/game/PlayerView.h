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

#ifndef __GAME_PLAYERVIEW_H__
#define __GAME_PLAYERVIEW_H__

#include "idlib/math/Vector.h"
#include "idlib/Dict.h"
#include "renderer/Material.h"
#include "renderer/RenderWorld.h"

class idSaveGame;
class idRestoreGame;

/*
===============================================================================

	Player view / FullScreenFX

===============================================================================
*/

// screenBlob_t are for the on-screen damage claw marks, etc
typedef struct {
	const idMaterial	*material;
	float				x, y, w, h;
	float				s1, t1, s2, t2;
	int					finishTime;
	int					startFadeTime;
	float				driftAmount;
} screenBlob_t;

#define	MAX_SCREEN_BLOBS	8

class WarpPolygon_t {
public:
	idVec4					outer1;
	idVec4					outer2;
	idVec4					center;
};

class Warp_t {
public:
	int						id;
	bool					active;

	int						startTime;
	float					initialRadius;

	idVec3					worldOrigin;
	idVec2					screenOrigin;

	int						durationMsec;

	idList<WarpPolygon_t>	polys;
};

class idPlayerView;
class FullscreenFXManager;

/*
==================
idPlayerView
==================
*/
class idPlayerView {
public:
							idPlayerView();
							~idPlayerView();

	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );

	void					SetPlayerEntity( class idPlayer *playerEnt );
	void					ClearEffects( void );
	void					DamageImpulse( idVec3 localKickDir, const idDict *damageDef );
	void					WeaponFireFeedback( const idDict *weaponDef );
	idAngles				AngleOffset( void ) const;			// returns the current kick angle
	idMat3					ShakeAxis( void ) const;			// returns the current shake angle
	void					CalculateShake( void );

	// this may involve rendering to a texture and displaying
	// that with a warp model or in double vision mode
	void					RenderPlayerView( idUserInterface *hud );

	void					Fade( idVec4 color, int time );
	void					Flash( idVec4 color, int time );
	void					AddBloodSpray( float duration );

	// temp for view testing
	void					EnableBFGVision( bool b ) { bfgVision = b; };

	void					ScreenBlobs( void );	
	void					ArmorPulse( void );
	void					TunnelVision( void );
	void					BFGVision( void );
	void					BerserkVision( void );
	int						AddWarp( idVec3 worldOrigin, float centerx, float centery, float initialRadius, float durationMsec );
	void					FreeWarp( int id );

	void					PlayerViewManager( void );
	void					PostProcess_CAS( void );

private:
	void					SingleView( idUserInterface *hud, const renderView_t *view );
	void					ScreenFade();
	screenBlob_t			*GetScreenBlob();
	screenBlob_t			screenBlobs[MAX_SCREEN_BLOBS];

public:			
	const idMaterial		*tunnelMaterial;		// health tunnel vision
	const idMaterial		*armorMaterial;			// armor damage view effect
	const idMaterial		*berserkMaterial;		// berserk effect
	const idMaterial		*irGogglesMaterial;		// ir effect
	const idMaterial		*bloodSprayMaterial;	 // blood spray
	const idMaterial		*bfgMaterial;			// when targeted with BFG
	const idMaterial		*lagoMaterial;			// lagometer drawing

	const idMaterial		*blackMaterial;			// Black material (for general use) 
	const idMaterial		*whiteMaterial;			// White material (for general use) 
	const idMaterial		*currentRenderMaterial;	// Current Render material (for general use) 
	const idMaterial		*scratchMaterial;		// material to take the double vision screen shot

	const idMaterial		*casMaterial;			// Explosion FX material

	int						dvFinishTime;		// double vision will be stopped at this time
	int						kickFinishTime;		// view kick will be stopped at this time
	idAngles				kickAngles;
	bool					bfgVision;
	idVec4					fadeColor;			// fade color
	idVec4					fadeToColor;		// color to fade to
	idVec4					fadeFromColor;		// color to fade from
	float					fadeRate;			// fade rate
	int						fadeTime;			// fade time
	float					lastDamageTime;		// accentuate the tunnel effect for a while
	idAngles				shakeAng;			// from the sound sources

	idPlayer				*player;
	renderView_t			view;
	FullscreenFXManager		*fxManager;
	renderView_t			hackedView;

	// PostProccess Scaling Fix (Sikkpin)
	int						screenHeight;
	int						screenWidth;
	idVec2					shiftScale;

	class dnImageWrapper {
private:	
	idStr					m_strImage;
	const idMaterial		*m_matImage;

public:
	dnImageWrapper( const char *a_strImage ) : m_strImage( a_strImage ), m_matImage( declManager->FindMaterial( a_strImage ) ) { }

	ID_INLINE operator const char *() const { return m_strImage.c_str(); }
	ID_INLINE operator const idMaterial *() const { return m_matImage; }
};

class dnPostProcessManager {
private:
	int						m_iScreenHeight;
	int						m_iScreenWidth;
	int						m_iScreenHeightPow2;
	int						m_iScreenWidthPow2;
	float					m_fShiftScale_x;
	float					m_fShiftScale_y;

	int						m_nFramesToUpdateCookedData;	// After these number of frames Cooked data will be updated. 0 means no update.

	unsigned char			m_nFramesSinceLumUpdate;						
		
	bool					m_bForceUpdateOnCookedData;

	dnImageWrapper			m_imageCurrentRender;
	dnImageWrapper			m_imageCurrentRender8x8DownScaled;
	dnImageWrapper			m_imageLuminance64x64;
	dnImageWrapper			m_imageluminance4x4;
	dnImageWrapper			m_imageAdaptedLuminance1x1;
	dnImageWrapper			m_imageBloom;
	dnImageWrapper			m_imageHalo;
	
	// Every channel of this image will have a cooked mathematical data. 
	dnImageWrapper			m_imageCookedMath;
	const idMaterial		*m_matCookMath_pass1;
	const idMaterial		*m_matCookMath_pass2;
	const idMaterial		*m_matCookMath_pass3;

	const idMaterial		*m_matAvgLuminance64x;
	const idMaterial		*m_matAvgLumSample4x4;
	const idMaterial		*m_matAdaptLuminance;
	const idMaterial		*m_matBrightPass;
	const idMaterial		*m_matGaussBlurX;
	const idMaterial		*m_matGaussBlurY;
	const idMaterial		*m_matHalo;
	const idMaterial		*m_matGaussBlurXHalo;
	const idMaterial		*m_matGaussBlurYHalo;
	const idMaterial		*m_matFinalScenePass;
	const idMaterial		*m_matCookVignette;

	// For debug renders
	const idMaterial		*m_matDecodedLumTexture64x64;
	const idMaterial		*m_matDecodedLumTexture4x4;
	const idMaterial		*m_matDecodedAdaptLuminance;

public:
							dnPostProcessManager();
							~dnPostProcessManager();

	// will be called whenever vid_restart or reloadImages is executed
	// (set via common->SetCallback())
	static void				ReloadImagesCallback( void *arg, const idCmdArgs &cmdArgs );

	void					Initialize();	// This method should be invoked when idPlayerView::Restore is called.
	void					Update();		// Called Every Frame. 

private:
	// Following methods should not be called by any other object, but itself.
	void					UpdateBackBufferParameters();
	void					UpdateCookedData();
	void					RenderDebugTextures();		
	void					UpdateInteractionShader();
};
	dnPostProcessManager	m_postProcessManager;
};

/*
==================
FxFader
==================
*/
class FxFader {
	enum {
		FX_STATE_OFF,
		FX_STATE_RAMPUP,
		FX_STATE_RAMPDOWN,
		FX_STATE_ON
	};

	int						time;
	int						state;
	float					alpha;
	int						msec;

public:
							FxFader();

	// primary functions
	bool					SetTriggerState( bool active );

	virtual void			Save( idSaveGame *savefile );
	virtual void			Restore( idRestoreGame *savefile );

	// fader functions
	void					SetFadeTime( int t )	{ msec = t; };
	int						GetFadeTime()			{ return msec; };

	// misc functions
	float					GetAlpha()				{ return alpha; };
};

/*
==================
FullscreenFX
==================
*/
class FullscreenFX {
protected:
	idStr					name;
	FxFader					fader;
	FullscreenFXManager		*fxman;

public:
							FullscreenFX()							{ fxman = NULL; };
	virtual					~FullscreenFX()							{ };

	virtual void			Initialize()							= 0;
	virtual bool			Active()								= 0;
	virtual void			HighQuality()							= 0;
	virtual void			LowQuality()							{ };
	virtual void			AccumPass( const renderView_t *view )	{ };
	virtual bool			HasAccum()								{ return false; };

	void					SetName( idStr n )						{ name = n; };
	idStr					GetName()								{ return name; };

	void					SetFXManager( FullscreenFXManager *fx )	{ fxman = fx; };

	bool					SetTriggerState( bool state )			{ return fader.SetTriggerState( state ); };
	void					SetFadeSpeed( int msec )				{ fader.SetFadeTime( msec ); };
	float					GetFadeAlpha()							{ return fader.GetAlpha(); };

	virtual void			Save( idSaveGame *savefile );
	virtual void			Restore( idRestoreGame *savefile );
};

/*
==================
FullscreenFX_Helltime
==================
*/
class FullscreenFX_Helltime : public FullscreenFX {
	const idMaterial		*acInitMaterials[3];
	const idMaterial		*acCaptureMaterials[3];
	const idMaterial		*acDrawMaterials[3];
	const idMaterial		*crCaptureMaterials[3];
	const idMaterial		*crDrawMaterials[3];
	bool					clearAccumBuffer;

	int						DetermineLevel();

public:
	virtual void			Initialize();
	virtual bool			Active();
	virtual void			HighQuality();
	virtual void			AccumPass( const renderView_t *view );
	virtual bool			HasAccum() { return true; };

	virtual void			Restore( idRestoreGame *savefile );
};

/*
==================
FullscreenFX_Multiplayer
==================
*/
class FullscreenFX_Multiplayer : public FullscreenFX {
	const idMaterial		*acInitMaterials;
	const idMaterial		*acCaptureMaterials;
	const idMaterial		*acDrawMaterials;
	const idMaterial		*crCaptureMaterials;
	const idMaterial		*crDrawMaterials;
	bool					clearAccumBuffer;

	int						DetermineLevel();

public:
	virtual void			Initialize();
	virtual bool			Active();
	virtual void			HighQuality();
	virtual void			AccumPass( const renderView_t *view );
	virtual bool			HasAccum()		{ return true; };

	virtual void			Restore( idRestoreGame *savefile );
};

/*
==================
FullscreenFX_Warp
==================
*/
class FullscreenFX_Warp : public FullscreenFX {
	const idMaterial		*material;
	bool					grabberEnabled;
	int						startWarpTime;

	void					DrawWarp( WarpPolygon_t wp, float interp );

public:
	virtual void			Initialize();
	virtual bool			Active();
	virtual void			HighQuality();

	void					EnableGrabber( bool active )	{ grabberEnabled = active; startWarpTime = gameLocal.slow.time; };

	virtual void			Save( idSaveGame *savefile );
	virtual void			Restore( idRestoreGame *savefile );
};

/*
==================
FullscreenFX_EnviroSuit
==================
*/
class FullscreenFX_EnviroSuit : public FullscreenFX {
	const idMaterial		*material;

public:
	virtual void			Initialize();
	virtual bool			Active();
	virtual void			HighQuality();
};

/*
==================
FullscreenFX_DoubleVision
==================
*/
class FullscreenFX_DoubleVision : public FullscreenFX {
	const idMaterial		*material;

public:
	virtual void			Initialize();
	virtual bool			Active();
	virtual void			HighQuality();
};

/*
==================
FullscreenFX_InfluenceVision
==================
*/
class FullscreenFX_InfluenceVision : public FullscreenFX {
public:
	virtual void			Initialize();
	virtual bool			Active();
	virtual void			HighQuality();
};

/*
==================
FullscreenFX_Bloom
==================
*/
class FullscreenFX_Bloom : public FullscreenFX {
	const idMaterial		*drawMaterial;
	const idMaterial		*initMaterial;

	float					currentIntensity;
	float					targetIntensity;

public:
	virtual void			Initialize();
	virtual bool			Active();
	virtual void			HighQuality();

	virtual void			Save( idSaveGame *savefile );
	virtual void			Restore( idRestoreGame *savefile );
};

/*
==================
FullscreenFXManager
==================
*/
class FullscreenFXManager {
	idList<FullscreenFX*>	fx;
	idVec2					shiftScale;

	idPlayerView			*playerView;
	const idMaterial		*blendBackMaterial;

	void					CreateFX( idStr name, idStr fxtype, int fade );

public:
							FullscreenFXManager();
	virtual					~FullscreenFXManager();

	void					Initialize( idPlayerView *pv );

	void					Process( const renderView_t *view );
	void					Blendback( float alpha );

	idVec2					GetShiftScale()			{ return shiftScale; };
	idPlayerView			*GetPlayerView()		{ return playerView; };
	idPlayer				*GetPlayer()			{ return gameLocal.GetLocalPlayer(); };

	int						GetNum()				{ return fx.Num(); };
	FullscreenFX			*GetFX( int index )		{ return fx[index]; };
	FullscreenFX			*FindFX( idStr name );

	void					Save( idSaveGame *savefile );
	void					Restore( idRestoreGame *savefile );
};

#endif /* !__GAME_PLAYERVIEW_H__ */
