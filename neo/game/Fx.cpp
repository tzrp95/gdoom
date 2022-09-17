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
#include "renderer/ModelManager.h"
#include "framework/DeclEntityDef.h"

#include "gamesys/SysCvar.h"
#include "Player.h"
#include "Projectile.h"
#include "WorldSpawn.h"
#include "SmokeParticles.h"

#include "Fx.h"

/*
===============================================================================

	idEntityFx

 * class modified by Ivan to support new class idDamagingFX

===============================================================================
*/

const idEventDef EV_Fx_FadeFx( "fadeFx" );
const idEventDef EV_Fx_KillFx( "_killfx" );
const idEventDef EV_Fx_Action( "_fxAction", "e" );		// implemented by subclasses

CLASS_DECLARATION( idEntity, idEntityFx )
	EVENT( EV_Activate,		idEntityFx::Event_Trigger )
	EVENT( EV_Fx_KillFx,	idEntityFx::Event_ClearFx )
	EVENT( EV_Fx_FadeFx,	idEntityFx::Event_FadeFx )
END_CLASS


/*
================
idEntityFx::Save
================
*/
void idEntityFx::Save( idSaveGame *savefile ) const {
	int i;

	victim.Save( savefile );
	savefile->WriteBool( manualRemove );
	savefile->WriteBool( manualFadingOut );
	savefile->WriteBool( hasEndlessSounds );

	savefile->WriteInt( started );
	savefile->WriteInt( nextTriggerTime );
	savefile->WriteFX( fxEffect );
	savefile->WriteString( systemName );

	savefile->WriteInt( actions.Num() );

	for ( i = 0; i < actions.Num(); i++ ) {

		if ( actions[i].lightDefHandle >= 0 ) {
			savefile->WriteBool( true );
			savefile->WriteRenderLight( actions[i].renderLight );
		} else {
			savefile->WriteBool( false );
		}

		if ( actions[i].modelDefHandle >= 0 ) {
			savefile->WriteBool( true );
			savefile->WriteRenderEntity( actions[i].renderEntity );
		} else {
			savefile->WriteBool( false );
		}

		savefile->WriteFloat( actions[i].delay );
		savefile->WriteInt( actions[i].start );
		savefile->WriteBool( actions[i].soundStarted );
		savefile->WriteBool( actions[i].shakeStarted );
		savefile->WriteBool( actions[i].decalDropped );
		savefile->WriteBool( actions[i].launched );
		savefile->WriteParticle( actions[i].smoke );
		savefile->WriteInt( actions[i].smokeTime );
	}
}

/*
================
idEntityFx::Restore
================
*/
void idEntityFx::Restore( idRestoreGame *savefile ) {
	int i;
	int num;
	bool hasObject;

	victim.Restore( savefile );
	savefile->ReadBool( manualRemove );
	savefile->ReadBool( manualFadingOut );
	savefile->ReadBool( hasEndlessSounds );

	savefile->ReadInt( started );
	savefile->ReadInt( nextTriggerTime );
	savefile->ReadFX( fxEffect );
	savefile->ReadString( systemName );

	savefile->ReadInt( num );

	actions.SetNum( num );
	for ( i = 0; i < num; i++ ) {

		savefile->ReadBool( hasObject );
		if ( hasObject ) {
			savefile->ReadRenderLight( actions[i].renderLight );
			actions[i].lightDefHandle = gameRenderWorld->AddLightDef( &actions[i].renderLight );
		} else {
			memset( &actions[i].renderLight, 0, sizeof( renderLight_t ) );
			actions[i].lightDefHandle = -1;
		}

		savefile->ReadBool( hasObject );
		if ( hasObject ) {
			savefile->ReadRenderEntity( actions[i].renderEntity );
			actions[i].modelDefHandle = gameRenderWorld->AddEntityDef( &actions[i].renderEntity );
		} else {
			memset( &actions[i].renderEntity, 0, sizeof( renderEntity_t ) );
			actions[i].modelDefHandle = -1;
		}

		savefile->ReadFloat( actions[i].delay );
		savefile->ReadInt( actions[i].start );
		savefile->ReadBool( actions[i].soundStarted );
		savefile->ReadBool( actions[i].shakeStarted );
		savefile->ReadBool( actions[i].decalDropped );
		savefile->ReadBool( actions[i].launched );
		savefile->ReadParticle( actions[i].smoke );
		savefile->ReadInt( actions[i].smokeTime );
	}
}

/*
================
idEntityFx::Setup
================
*/
void idEntityFx::Setup( const char *fx ) {

	// already started
	if ( started >= 0 ) {
		return;
	}

	// early during MP Spawn() with no information. wait till we ReadFromSnapshot for more
	if ( gameLocal.isClient && ( !fx || fx[0] == '\0' ) ) {
		return;
	}

	systemName = fx;
	started = 0;
	hasEndlessSounds = false;

	fxEffect = static_cast<const idDeclFX*>( declManager->FindType( DECL_FX, systemName.c_str() ) );

	if ( fxEffect ) {
		idFXLocalAction localAction;

		memset( &localAction, 0, sizeof( idFXLocalAction ) );

		actions.AssureSize( fxEffect->events.Num(), localAction );

		for ( int i = 0; i<fxEffect->events.Num(); i++ ) {
			const idFXSingleAction &fxaction = fxEffect->events[i];

			idFXLocalAction &laction = actions[i];
			if ( fxaction.random1 || fxaction.random2 ) {
				laction.delay = fxaction.random1 + gameLocal.random.RandomFloat() * ( fxaction.random2 - fxaction.random1 );
			} else {
				laction.delay = fxaction.delay;
			}
			laction.start = -1;
			laction.lightDefHandle = -1;
			laction.modelDefHandle = -1;
			laction.shakeStarted = false;
			laction.decalDropped = false;
			laction.launched = false;
			laction.smokeTime = -1;
			if ( fxaction.data.Find( "_smoke.prt" ) >= 0 ) {
				laction.smoke = static_cast<const idDeclParticle*>( declManager->FindType( DECL_PARTICLE, fxaction.data ) );
			}
			if ( !hasEndlessSounds && fxaction.type == FX_SOUND && fxaction.duration <= 0 ) {
				hasEndlessSounds = true;
			}

		}
	}
}

/*
================
idEntityFx::EffectName
================
*/
const char *idEntityFx::EffectName( void ) {
	return fxEffect ? fxEffect->GetName() : NULL;
}

/*
================
idEntityFx::Joint
================
*/
const char *idEntityFx::Joint( void ) {
	return fxEffect ? fxEffect->joint.c_str() : NULL;
}

// Ivan --->
/*
================
idEntityFx::ResetShaderParms
================
*/
void idEntityFx::ResetShaderParms( void ) {
	if ( !fxEffect ) {
		return;
	}
	for ( int i = 0; i < fxEffect->events.Num(); i++ ) {
		const idFXSingleAction &fxaction = fxEffect->events[i];
		idFXLocalAction &laction = actions[i];

		if ( laction.lightDefHandle != -1 && fxaction.sibling == -1 && fxaction.type != FX_ATTACHLIGHT ) {
			laction.renderLight.shaderParms[ SHADERPARM_RED ]	= fxaction.lightColor.x;
			laction.renderLight.shaderParms[ SHADERPARM_GREEN ]	= fxaction.lightColor.y;
			laction.renderLight.shaderParms[ SHADERPARM_BLUE ]	= fxaction.lightColor.z;
			laction.renderLight.shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( gameLocal.time );
		}
		if ( laction.modelDefHandle != -1 && fxaction.sibling == -1 && fxaction.type != FX_ATTACHENTITY ) {
			laction.renderEntity.shaderParms[ SHADERPARM_RED ]		= 1.0f;
			laction.renderEntity.shaderParms[ SHADERPARM_GREEN ]	= 1.0f;
			laction.renderEntity.shaderParms[ SHADERPARM_BLUE ]		= 1.0f;
			if ( manualFadingOut ) { // no need to reset it if we are not fading - this would make the particle to change position instantly
				laction.renderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( gameLocal.time );
			}
			laction.renderEntity.shaderParms[ SHADERPARM_PARTICLE_STOPTIME ] = 0; // make particles to appear
		}
	}
}
// <---

/*
================
idEntityFx::CleanUp
================
*/
void idEntityFx::CleanUp( void ) {
	if ( !fxEffect ) {
		return;
	}
	for ( int i = 0; i < fxEffect->events.Num(); i++ ) {
		const idFXSingleAction &fxaction = fxEffect->events[i];
		idFXLocalAction &laction = actions[i];
		CleanUpSingleAction( fxaction, laction );
	}
}

/*
================
idEntityFx::CleanUpSingleAction
================
*/
void idEntityFx::CleanUpSingleAction( const idFXSingleAction &fxaction, idFXLocalAction &laction ) {
	if ( laction.lightDefHandle != -1 && fxaction.sibling == -1 && fxaction.type != FX_ATTACHLIGHT ) {
		gameRenderWorld->FreeLightDef( laction.lightDefHandle );
		laction.lightDefHandle = -1;
	}
	if ( laction.modelDefHandle != -1 && fxaction.sibling == -1 && fxaction.type != FX_ATTACHENTITY ) {
		gameRenderWorld->FreeEntityDef( laction.modelDefHandle );
		laction.modelDefHandle = -1;
	}
	laction.start = -1;
}

/*
================
idEntityFx::Start
================
*/
void idEntityFx::Start( int time ) {
	if ( !fxEffect ) {
		return;
	}

	// already started
	bool restartSounds = true;
	if ( started >= 0 ) {
		if ( manualFadingOut && hasEndlessSounds ) { // if fading and endless we need to stop them
			StopSound( SND_CHANNEL_ANY, false );
		} else {
			restartSounds = false;
		}
	}

	manualFadingOut = false;
	started = time;

	for ( int i = 0; i < fxEffect->events.Num(); i++ ) {
		idFXLocalAction &laction = actions[i];

		if ( laction.delay < 0 ) { // will be started manually
			laction.start = -1; // start disabled
		} else {
			laction.start = time;
		}
		if ( restartSounds ) {
			laction.soundStarted = false;
		} // else don't change it!

		laction.smokeTime = time;
		laction.shakeStarted = false;
		laction.decalDropped = false;
		laction.launched = false;
	}
}

/*
================
idEntityFx::Stop
================
*/
void idEntityFx::Stop( void ) {
	CleanUp();
	started = -1;
}

// Ivan --->
/*
================
idEntityFx::FadeDuration

It returns the max 'duration' of the fadeOut stage with "delay -1" (manually started)
================
*/
const int idEntityFx::FadeDuration( void ) {
	int max = 0;

	if ( !fxEffect ) {
		return max;
	}

	for( int i = 0; i < fxEffect->events.Num(); i++ ) {
		const idFXSingleAction &fxaction = fxEffect->events[i];

		if ( fxaction.delay > 0 || fxaction.fadeOutTime <= 0 ) { // than it's not part of the manual fade
			continue;
		}

		int d = ( fxaction.duration ) * 1000.0f;
		if ( d > max ) {
			max = d;
		}
	}

	return max;
}
// <---

/*
================
idEntityFx::Duration
================
*/
const int idEntityFx::Duration( void ) {
	int max = 0;

	if ( !fxEffect ) {
		return max;
	}

	for ( int i = 0; i < fxEffect->events.Num(); i++ ) {
		const idFXSingleAction &fxaction = fxEffect->events[i];
		int d = ( fxaction.delay + fxaction.duration ) * 1000.0f;
		if ( d > max ) {
			max = d;
		}
	}

	return max;
}

/*
================
idEntityFx::Done
================
*/
const bool idEntityFx::Done( void ) {
	if ( manualRemove ) { // it is never "done"... unless it has been removed :)
		return false;
	}

	if ( started > 0 && gameLocal.time > started + Duration() ) {
		return true;
	}
	return false;
}

/*
================
idEntityFx::ApplyFade
================
*/
void idEntityFx::ApplyFade( const idFXSingleAction &fxaction, idFXLocalAction &laction, const int time, const int actualStart ) {
	if ( fxaction.fadeInTime || fxaction.fadeOutTime ) {
		float fadePct = ( float )( time - actualStart ) / ( 1000.0f * ( ( fxaction.fadeInTime != 0 ) ? fxaction.fadeInTime : fxaction.fadeOutTime ) );
		if ( fadePct > 1.0 ) {
			fadePct = 1.0;
		}
		if ( laction.modelDefHandle != -1 ) {
			laction.renderEntity.shaderParms[SHADERPARM_RED] = ( fxaction.fadeInTime ) ? fadePct : 1.0f - fadePct;
			laction.renderEntity.shaderParms[SHADERPARM_GREEN] = ( fxaction.fadeInTime ) ? fadePct : 1.0f - fadePct;
			laction.renderEntity.shaderParms[SHADERPARM_BLUE] = ( fxaction.fadeInTime ) ? fadePct : 1.0f - fadePct;

			gameRenderWorld->UpdateEntityDef( laction.modelDefHandle, &laction.renderEntity );
		}
		if ( laction.lightDefHandle != -1 ) {
			laction.renderLight.shaderParms[SHADERPARM_RED] = fxaction.lightColor.x * ( ( fxaction.fadeInTime ) ? fadePct : 1.0f - fadePct );
			laction.renderLight.shaderParms[SHADERPARM_GREEN] = fxaction.lightColor.y * ( ( fxaction.fadeInTime ) ? fadePct : 1.0f - fadePct );
			laction.renderLight.shaderParms[SHADERPARM_BLUE] = fxaction.lightColor.z * ( ( fxaction.fadeInTime ) ? fadePct : 1.0f - fadePct );

			gameRenderWorld->UpdateLightDef( laction.lightDefHandle, &laction.renderLight );
		}
	}
}

/*
================
idEntityFx::Run
================
*/
void idEntityFx::Run( int time ) {
	int ieff, j;
	idEntity *ent = NULL;
	const idDict *projectileDef = NULL;
	idProjectile *projectile = NULL;

	if ( !fxEffect ) {
		return;
	}

	for ( ieff = 0; ieff < fxEffect->events.Num(); ieff++ ) {
		const idFXSingleAction &fxaction = fxEffect->events[ieff];
		idFXLocalAction &laction = actions[ieff];

		//
		// if we're currently done with this one
		//
		if ( laction.start == -1 ) {
			continue;
		}

		//
		// see if it's delayed
		//
		if ( laction.delay > 0 ) {
			if ( laction.start + (time - laction.start) < laction.start + (laction.delay * 1000) ) {
				continue;
			}
		}
		// negative delay: they are meant to be started only manually
		else if ( laction.delay < 0 ) {
			if ( !manualFadingOut ) {
				continue;
			}
		}

		//
		// each event can have it's own delay and restart
		//
        int actualStart = ( laction.delay > 0 )? laction.start + ( int )( laction.delay * 1000 ) : laction.start;
        if ( fxaction.duration > 0 ) { // duration 0 means endless
            float pct = ( float )( time - actualStart ) / (1000 * fxaction.duration );
            if ( pct >= 1.0f ) {
                laction.start = -1;
                float totalDelay = 0.0f;
                if ( fxaction.restart ) {
                    if ( fxaction.random1 || fxaction.random2 ) {
                        totalDelay = fxaction.random1 + gameLocal.random.RandomFloat() * (fxaction.random2 - fxaction.random1);
                    } else {
                        totalDelay = fxaction.delay;
                    }
                    laction.delay = totalDelay;
                    laction.start = time;
                }
                continue;
            }
        }

		if ( fxaction.fire.Length() ) {
			for ( j = 0; j < fxEffect->events.Num(); j++ ) {
				if ( fxEffect->events[j].name.Icmp( fxaction.fire ) == 0 ) {
					actions[j].delay = 0;
				}
			}
		}

		idFXLocalAction *useAction;
		if ( fxaction.sibling == -1 ) {
			useAction = &laction;
		} else {
			useAction = &actions[fxaction.sibling];
		}
		assert( useAction );

		switch( fxaction.type ) {
			case FX_ATTACHLIGHT:
			case FX_LIGHT: {
				if ( useAction->lightDefHandle == -1 ) {
					if ( fxaction.type == FX_LIGHT ) {
						memset( &useAction->renderLight, 0, sizeof( renderLight_t ) );
						useAction->renderLight.origin = GetPhysics()->GetOrigin() + fxaction.offset;
						useAction->renderLight.axis = GetPhysics()->GetAxis();
						useAction->renderLight.lightRadius[0] = fxaction.lightRadius;
						useAction->renderLight.lightRadius[1] = fxaction.lightRadius;
						useAction->renderLight.lightRadius[2] = fxaction.lightRadius;
						useAction->renderLight.shader = declManager->FindMaterial( fxaction.data, false );
						useAction->renderLight.shaderParms[ SHADERPARM_RED ]	= fxaction.lightColor.x;
						useAction->renderLight.shaderParms[ SHADERPARM_GREEN ]	= fxaction.lightColor.y;
						useAction->renderLight.shaderParms[ SHADERPARM_BLUE ]	= fxaction.lightColor.z;
						useAction->renderLight.shaderParms[ SHADERPARM_TIMESCALE ]	= 1.0f;
						useAction->renderLight.shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( time );
						useAction->renderLight.referenceSound = refSound.referenceSound;
						useAction->renderLight.pointLight = true;
						if ( fxaction.noshadows ) {
							useAction->renderLight.noShadows = true;
						}
						useAction->lightDefHandle = gameRenderWorld->AddLightDef( &useAction->renderLight );
					}
					if ( fxaction.noshadows ) {
						for ( j = 0; j < fxEffect->events.Num(); j++ ) {
							idFXLocalAction &laction2 = actions[j];
							if ( laction2.modelDefHandle != -1 ) {
								laction2.renderEntity.noShadow = true;
							}
						}
					}
				}

				else if ( fxaction.trackOrigin ) {
					// todo: add code for explicitAxis also here?
					useAction->renderLight.origin = GetPhysics()->GetOrigin() + fxaction.offset;
					useAction->renderLight.axis = GetPhysics()->GetAxis();
					// track origin fix
					gameRenderWorld->UpdateLightDef( useAction->lightDefHandle, &useAction->renderLight );

				}

				ApplyFade( fxaction, *useAction, time, actualStart );
				break;
			}
			case FX_SOUND: {
				if ( !useAction->soundStarted ) {
					useAction->soundStarted = true;
					const idSoundShader *shader = declManager->FindSound( fxaction.data );
					StartSoundShader( shader, SND_CHANNEL_ANY, 0, false, NULL );
					for ( j = 0; j < fxEffect->events.Num(); j++ ) {
						idFXLocalAction &laction2 = actions[j];
						if ( laction2.lightDefHandle != -1 ) {
							laction2.renderLight.referenceSound = refSound.referenceSound;
							gameRenderWorld->UpdateLightDef( laction2.lightDefHandle, &laction2.renderLight );
						}
					}
				}
				break;
			}
			case FX_DECAL: {
				if ( !useAction->decalDropped ) {
					useAction->decalDropped = true;

					// foresthale: orient the decal toward the collision surface, not gravity
					// note that this must be negative (into the surface) for the decal depth to work right
					idVec3 dir = decalDir * -1.0f;
					if ( dir.LengthSqr() < 0.0001f ) {  // if no dir, we use gravity
						dir = GetPhysics()->GetGravity();
						dir.Normalize();
					}

					gameLocal.ProjectDecal( GetPhysics()->GetOrigin(), dir, 8.0f, true, fxaction.size, fxaction.data );
				}
				break;
			}
			case FX_SHAKE: {
				if ( !useAction->shakeStarted ) {
					idDict args;
					args.Clear();
					args.SetFloat( "kick_time", fxaction.shakeTime );
					args.SetFloat( "kick_amplitude", fxaction.shakeAmplitude );
					for ( j = 0; j < gameLocal.numClients; j++ ) {
						idPlayer *player = gameLocal.GetClientByNum( j );
						if ( player && ( player->GetPhysics()->GetOrigin() - GetPhysics()->GetOrigin() ).LengthSqr() < Square( fxaction.shakeDistance ) ) {
							if ( !gameLocal.isMultiplayer || !fxaction.shakeIgnoreMaster || GetBindMaster() != player ) {
								player->playerView.DamageImpulse( fxaction.offset, &args );
							}
						}
					}
					if ( fxaction.shakeImpulse != 0.0f && fxaction.shakeDistance != 0.0f ) {
						idEntity *ignore_ent = NULL;
						if ( gameLocal.isMultiplayer ) {
							ignore_ent = this;
							if ( fxaction.shakeIgnoreMaster ) {
								ignore_ent = GetBindMaster();
							}
						}
						// lookup the ent we are bound to?
						gameLocal.RadiusPush( GetPhysics()->GetOrigin(), fxaction.shakeDistance, fxaction.shakeImpulse, this, ignore_ent, 1.0f, true );
					}
					useAction->shakeStarted = true;
				}
				break;
			}
			case FX_ATTACHENTITY:
			case FX_PARTICLE:
			case FX_MODEL: {
				if ( laction.smoke ) {
					if ( !gameLocal.smokeParticles->EmitSmoke( laction.smoke, laction.smokeTime, gameLocal.random.CRandomFloat(),
						GetPhysics()->GetOrigin() + fxaction.offset * ( fxaction.explicitAxis ? fxaction.axis : GetPhysics()->GetAxis() ),
						( fxaction.explicitAxis ? fxaction.axis : GetPhysics()->GetAxis() ), timeGroup ) ) {
						laction.smokeTime = gameLocal.time; // restart
					}
				} else {
					if ( useAction->modelDefHandle == -1 ) {
						memset( &useAction->renderEntity, 0, sizeof( renderEntity_t ) );
						useAction->renderEntity.origin = GetPhysics()->GetOrigin() + fxaction.offset;
						useAction->renderEntity.axis = ( fxaction.explicitAxis ) ? fxaction.axis : GetPhysics()->GetAxis();
						useAction->renderEntity.hModel = renderModelManager->FindModel( fxaction.data );
						useAction->renderEntity.shaderParms[ SHADERPARM_RED ]		= 1.0f;
						useAction->renderEntity.shaderParms[ SHADERPARM_GREEN ]		= 1.0f;
						useAction->renderEntity.shaderParms[ SHADERPARM_BLUE ]		= 1.0f;
						useAction->renderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( time );
						useAction->renderEntity.shaderParms[3] = 1.0f;
						useAction->renderEntity.shaderParms[5] = 0.0f;
						if ( useAction->renderEntity.hModel ) {
							useAction->renderEntity.bounds = useAction->renderEntity.hModel->Bounds( &useAction->renderEntity );
						}
						useAction->modelDefHandle = gameRenderWorld->AddEntityDef( &useAction->renderEntity );
					} else if ( fxaction.trackOrigin ) {
						// fix offset: must be relative to our axis!
						useAction->renderEntity.axis = fxaction.explicitAxis ? fxaction.axis : GetPhysics()->GetAxis();
						useAction->renderEntity.origin = GetPhysics()->GetOrigin() + fxaction.offset * useAction->renderEntity.axis;
						//track origin fix
						gameRenderWorld->UpdateEntityDef( useAction->modelDefHandle, &useAction->renderEntity );
					}
					ApplyFade( fxaction, *useAction, time, actualStart );
				}
				break;
			}
			case FX_LAUNCH: {
				if ( gameLocal.isClient ) {
					// client never spawns entities outside of ClientReadSnapshot
					useAction->launched = true;
					break;
				}
				if ( !useAction->launched ) {
					useAction->launched = true;
					projectile = NULL;
					// FIXME: may need to cache this if it is slow
					projectileDef = gameLocal.FindEntityDefDict( fxaction.data, false );
					if ( !projectileDef ) {
						gameLocal.Warning( "projectile \'%s\' not found", fxaction.data.c_str() );
					} else {
						gameLocal.SpawnEntityDef( *projectileDef, &ent, false );
						if ( ent && ent->IsType( idProjectile::Type ) ) {
							projectile = ( idProjectile* )ent;
							projectile->Create( this, GetPhysics()->GetOrigin(), GetPhysics()->GetAxis()[0] );
							projectile->Launch( GetPhysics()->GetOrigin(), GetPhysics()->GetAxis()[0], vec3_origin );
						}
					}
				}
				break;
			}
			case FX_SHOCKWAVE: {
				if ( gameLocal.isClient ) {
					useAction->shakeStarted = true;
					break;
				}
				if ( !useAction->shakeStarted ) {
					idStr	shockDefName;
					useAction->shakeStarted = true;

					shockDefName = fxaction.data;
					if ( !shockDefName.Length() ) {
						shockDefName = "func_shockwave";
					}

					projectileDef = gameLocal.FindEntityDefDict( shockDefName, false );
					if ( !projectileDef ) {
						gameLocal.Warning( "shockwave \'%s\' not found", shockDefName.c_str() );
					} else {
						gameLocal.SpawnEntityDef( *projectileDef, &ent );
						ent->SetOrigin( GetPhysics()->GetOrigin() + fxaction.offset );
//						ent->PostEventMS( &EV_Activate, 0, victim.GetEntity() ); // set owner for shockwaves
						ent->PostEventMS( &EV_Remove, ent->spawnArgs.GetInt( "duration" ) );
					}
				}
				break;
			}
		}
	}
}

/*
================
idEntityFx::idEntityFx
================
*/
idEntityFx::idEntityFx() {
	fxEffect = NULL;
	started = -1;
	nextTriggerTime = -1;
	fl.networkSync = true;

	manualRemove		= false;
	manualFadingOut		= false;
	hasEndlessSounds	= false;
	victim				= NULL;
}

/*
================
idEntityFx::~idEntityFx
================
*/
idEntityFx::~idEntityFx() {
	CleanUp();
	fxEffect = NULL;

	if ( hasEndlessSounds ) {
		StopSound( SND_CHANNEL_ANY, false );
	}
}

/*
================
idEntityFx::Spawn
================
*/
void idEntityFx::Spawn( void ) {
	if ( g_skipFX.GetBool() ) {
		return;
	}

	const char *fx;
	nextTriggerTime = 0;
	fxEffect = NULL;
	if ( spawnArgs.GetString( "fx", "", &fx ) ) {
		systemName = fx;
	}
	if ( !spawnArgs.GetBool( "triggered" ) ) {
		Setup( fx );
		if ( spawnArgs.GetBool( "test" ) || spawnArgs.GetBool( "start" ) || spawnArgs.GetFloat ( "restart" ) ) {
			PostEventMS( &EV_Activate, 0, this );
		}
	}

	manualRemove = spawnArgs.GetBool( "manualRemove", "0" ); // don't change default behaviour

}

/*
================
idEntityFx::Think

Clears any visual fx started when {item,mob,player} was spawned
================
*/
void idEntityFx::Think( void ) {
	if ( g_skipFX.GetBool() ) {
		return;
	}

	if ( thinkFlags & TH_THINK ) {
		Run( gameLocal.time );
	}

	RunPhysics();
	Present();
}

// Ivan --->
/*
================
idEntityFx::SetupFade
================
*/
void idEntityFx::SetupFade( void ) {
	int ieff;

	if ( !fxEffect ) {
		return;
	}

	for ( ieff = 0; ieff < fxEffect->events.Num(); ieff++ ) {
		const idFXSingleAction &fxaction = fxEffect->events[ieff];
		idFXLocalAction &laction = actions[ieff];

		// turn on the waiting ones
		if ( laction.delay < 0 ) {
			laction.start = gameLocal.time;
		}
		// turn off the ones which are not endless
		else if ( fxaction.duration != 0 ) { // duration = 0 means endless. Turn off -1 ones too!
			laction.start = -1;
		}

		// turn off particles
		if ( laction.modelDefHandle != -1 && fxaction.sibling == -1 && fxaction.type != FX_ATTACHENTITY ) {
			laction.renderEntity.shaderParms[ SHADERPARM_PARTICLE_STOPTIME ] = MS2SEC( gameLocal.time );
		}
	}
}


/*
================
idEntityFx::FadeOutFx
================
*/
void idEntityFx::FadeOutFx( void ) {
	if ( started < 0 || manualFadingOut ) { // if not active or already fading
		return;
	}

	manualFadingOut = true;
	SetupFade();

	if ( hasEndlessSounds ) {
		FadeSound( SND_CHANNEL_ANY, -60, 1 ); // fade out sounds
	}

	if ( manualRemove ) {
		CancelEvents( &EV_Activate ); // make sure it's not going to re-activate itself
		CancelEvents( &EV_Fx_KillFx ); // make sure it's not going to kill or re-activate itself too soon
		PostEventMS( &EV_Fx_KillFx, FadeDuration() );
	}
}

/*
================
idEntityFx::Event_StopFx

 Clears any visual fx started when item(mob) was spawned
================
*/
void idEntityFx::Event_FadeFx( void ) {
	FadeOutFx();
}
// <---


/*
================
idEntityFx::Event_ClearFx

Clears any visual fx started when item(mob) was spawned
================
*/
void idEntityFx::Event_ClearFx( void ) {
	if ( g_skipFX.GetBool() ) {
		return;
	}

	Stop();
	CleanUp();
	BecomeInactive( TH_THINK );

	if ( spawnArgs.GetBool( "test" ) ) {
		PostEventMS( &EV_Activate, 0, this );
	} else {
		if ( spawnArgs.GetFloat( "restart" ) || !spawnArgs.GetBool( "triggered" ) ) {
			float rest = spawnArgs.GetFloat( "restart", "0" );
			if ( rest == 0.0f ) {
				PostEventSec( &EV_Remove, 0.1f );
			} else {
				rest *= gameLocal.random.RandomFloat();
				PostEventSec( &EV_Activate, rest, this );
			}
		}
	}
}

/*
================
idEntityFx::Event_Trigger
================
*/
void idEntityFx::Event_Trigger( idEntity *activator ) {
	if ( g_skipFX.GetBool() ) {
		return;
	}

	if ( gameLocal.time < nextTriggerTime ) {
		return;
	}

	float		fxActionDelay;
	const char	*fx;

	if ( spawnArgs.GetString( "fx", "", &fx ) ) {
		if ( manualRemove ) { // new case
			if ( started >= 0 && !manualFadingOut && spawnArgs.GetBool( "toggle", "0") ) { // if it is active && toggle is set
				FadeOutFx();
			} else {
				Setup( fx );
				Start( gameLocal.time ); // don't autokill
			}
		} else { // old case
			Setup( fx );
			Start( gameLocal.time );
			PostEventMS( &EV_Fx_KillFx, Duration() );
		}
		BecomeActive( TH_THINK );
	}

	fxActionDelay = spawnArgs.GetFloat( "fxActionDelay" );
	if ( fxActionDelay != 0.0f ) {
		nextTriggerTime = gameLocal.time + SEC2MS( fxActionDelay );
	} else {
		// prevent multiple triggers on same frame
		nextTriggerTime = gameLocal.time + 1;
	}
	PostEventSec( &EV_Fx_Action, fxActionDelay, activator );
}

/*
================
idEntityFx::StartFxUtility

 * added by Ivan
================
*/
void idEntityFx::StartFxUtility( idEntityFx *nfx, const idVec3 *useOrigin, const idMat3 *useAxis, idEntity *ent, bool bind, bool orientated, jointHandle_t jointnum, const idVec3 *useDecalDir ) {
	nfx->victim = ent;

	if ( nfx->Joint() && *nfx->Joint() ) { // "bindto" keyword
		nfx->BindToJoint( ent, nfx->Joint(), orientated ); // orientated
		nfx->SetOrigin( vec3_origin );
	}
	else if ( bind && jointnum != INVALID_JOINT ) {
		nfx->SetAxis( (useAxis) ? *useAxis : ent->GetPhysics()->GetAxis() );
		nfx->BindToJoint( ent, jointnum, orientated );
		nfx->SetOrigin( vec3_origin );
	} else {
		nfx->SetOrigin( (useOrigin) ? *useOrigin : ent->GetPhysics()->GetOrigin() );
		nfx->SetAxis( ( useAxis ) ? *useAxis : ent->GetPhysics()->GetAxis() );

		// foresthale: added decalDir so that StartFx can pass it to FX_DECAL
		if ( useDecalDir ) {
			nfx->decalDir = *useDecalDir;
		} else {
			nfx->decalDir = idVec3( 0.0f, 0.0f, 0.0f ); // no decalDir specified, we check for this in FX_DECAL
		}

		if ( bind ) {
			// never bind to world spawn
			if ( ent != gameLocal.world ) {
				nfx->Bind( ent, orientated );
			}
		}
	}
	nfx->Show();
}

/*
================
idEntityFx::StartFx
================
*/
idEntityFx *idEntityFx::StartFx( const char *fx, const idVec3 *useOrigin, const idMat3 *useAxis, idEntity *ent, bool bind, bool orientated, jointHandle_t jointnum, const idVec3 *useDecalDir ) {
	if ( g_skipFX.GetBool() || !fx || !*fx ) {
		return NULL;
	}

	idDict args;
	args.SetBool( "start", true );
	args.Set( "fx", fx );
	idEntityFx *nfx = static_cast<idEntityFx*>( gameLocal.SpawnEntityType( idEntityFx::Type, &args ) );

	if ( nfx ) {
		StartFxUtility( nfx, useOrigin, useAxis, ent, bind, orientated, jointnum, useDecalDir );
	}

	return nfx;
}

/*
=================
idEntityFx::WriteToSnapshot
=================
*/
void idEntityFx::WriteToSnapshot( idBitMsgDelta &msg ) const {
	GetPhysics()->WriteToSnapshot( msg );
	WriteBindToSnapshot( msg );
	msg.WriteInt( ( fxEffect != NULL ) ? gameLocal.ServerRemapDecl( -1, DECL_FX, fxEffect->Index() ) : -1 );
	msg.WriteInt( started );
}

/*
=================
idEntityFx::ReadFromSnapshot
=================
*/
void idEntityFx::ReadFromSnapshot( const idBitMsgDelta &msg ) {
	int fx_index, start_time, max_lapse;

	GetPhysics()->ReadFromSnapshot( msg );
	ReadBindFromSnapshot( msg );
	fx_index = gameLocal.ClientRemapDecl( DECL_FX, msg.ReadInt() );
	start_time = msg.ReadInt();

	if ( fx_index != -1 && start_time > 0 && !fxEffect && started < 0 ) {
		spawnArgs.GetInt( "effect_lapse", "1000", max_lapse );
		if ( gameLocal.time - start_time > max_lapse ) {
			// too late, skip the effect completely
			started = 0;
			return;
		}
		const idDeclFX *fx = static_cast<const idDeclFX*>( declManager->DeclByIndex( DECL_FX, fx_index ) );
		if ( !fx ) {
			gameLocal.Error( "FX at index %d not found", fx_index );
		}
		fxEffect = fx;
		Setup( fx->GetName() );
		Start( start_time );
	}
}

/*
=================
idEntityFx::ClientPredictionThink
=================
*/
void idEntityFx::ClientPredictionThink( void ) {
	if ( gameLocal.isNewFrame ) {
		Run( gameLocal.time );
	}
	RunPhysics();
	Present();
}

/*
===============================================================================

	idTeleporter

===============================================================================
*/

CLASS_DECLARATION( idEntityFx, idTeleporter )
	EVENT( EV_Fx_Action,	idTeleporter::Event_DoAction )
END_CLASS

/*
================
idTeleporter::Event_DoAction
================
*/
void idTeleporter::Event_DoAction( idEntity *activator ) {
	idAngles a( 0, spawnArgs.GetFloat( "angle" ), 0 );
	activator->Teleport( GetPhysics()->GetOrigin(), a, NULL );
}

/*
===============================================================================

  idDamagingFx

  * new class by Ivan

===============================================================================
*/

CLASS_DECLARATION( idEntityFx, idDamagingFx )
END_CLASS

/*
================
idDamagingFx::idDamagingFx
================
*/
idDamagingFx::idDamagingFx() {
	nextDamageTime		= 0;
	endDamageTime		= 0;
	damageRate			= 0;
	attacker			= NULL;
	damageDefString		= "";
	prtFadeinTime		= 0;
	prtTotalTime		= 0;
	aliveTime			= 0;
	deadTime			= 0;
	victimShellHandle	= -1;
	victimShellTime		= 0;
	victimShellCompensationTime = 0;
	victimShellCompensationValue = 0;
	victimShellSkin		= NULL;
}

/*
================
idDamagingFx::~idDamagingFx
================
*/
idDamagingFx::~idDamagingFx() {
	// remove the shell
	if ( victimShellHandle != -1 ) {
		gameRenderWorld->FreeEntityDef( victimShellHandle );
	}
}

/*
================
idDamagingFx::Spawn
================
*/
void idDamagingFx::Spawn( void ) {

	damageDefString = spawnArgs.GetString( "def_damage", "damage_damagingFx" );

	// damage def must be valid
	const idDict *damageDef = gameLocal.FindEntityDefDict( damageDefString.c_str() );
	if ( !damageDef ) {
		gameLocal.Error( "Unknown damageDef '%s' for '%s'", damageDefString.c_str(), name.c_str() );
	}

	// damageRate
	damageRate = SEC2MS( spawnArgs.GetFloat( "damageRate", "1" ) );
	if ( damageRate <= 0 ) {
		damageRate = 1000; // 1sec default
	}

	// time settings
	prtFadeinTime = SEC2MS( spawnArgs.GetFloat( "prtFadeinTime" ) );
	prtTotalTime = SEC2MS( spawnArgs.GetFloat( "prtTotalTime" ) );
	aliveTime = SEC2MS( spawnArgs.GetFloat( "aliveTime" ) );
	deadTime = SEC2MS( spawnArgs.GetFloat( "deadTime", "-1" ) ); // -1  = no effect on fx lifetime

	if ( deadTime > prtTotalTime ) {
		gameLocal.Error( "deadTime > prtTotalTime for '%s'", name.c_str() );
	}
	if ( aliveTime > prtTotalTime ) {
		gameLocal.Error( "aliveTime > prtTotalTime for '%s'", name.c_str() );
	}
}

/*
================
idDamagingFx::SetAttacker
================
*/
void idDamagingFx::SetAttacker( idEntity *ent ) {
	attacker = ent;
}

/*
================
idDamagingFx::VictimKilled
================
*/
void idDamagingFx::VictimKilled( void ) {
	if ( deadTime > 0 ) {
		Restart();
	} else if ( deadTime == 0 ) {
		FadeOutFx();
	}
}

/*
================
idDamagingFx::Restart
================
*/
void idDamagingFx::Restart( void ) {
	if ( victimShellTime + prtFadeinTime > gameLocal.time ) {
		return; // ignore restart if the fx is just fading in
	}

	CancelEvents( &EV_Activate );
	CancelEvents( &EV_Fx_KillFx );

	ResetShaderParms();

	nextTriggerTime = 0; // make sure the event is accepted
	PostEventMS( &EV_Activate, 0, this );
}

/*
================
idDamagingFx::Start
================
*/
void idDamagingFx::Start( int time ) { // time is usually the current time

	// aligment offset. We don't want the prt to 'jump'
	int alignmentOffset = 0;

	// victim must be valid
	if ( !victim.GetEntity() ) {
		gameLocal.Warning( "No 'victim' for entity '%s'", name.c_str() );
		return;
	}

	// different time for dead victims if killedTime is set
	if ( deadTime >= 0 && victim.GetEntity()->health <= 0 ) {
		// skin smoke time
		int prtExcessTime = prtTotalTime - deadTime;
		if ( endDamageTime == 0 || manualFadingOut ) { // new or already fading out
			victimShellTime = time;
			victimShellCompensationTime = time + prtFadeinTime; // wait for fadein to complete
			victimShellCompensationValue = -prtExcessTime; // jump fw so the prt ends early
		} else { // restart
			alignmentOffset = ( time - victimShellTime ) % prtFadeinTime;
//			victimShellTime = ...; // don't jump and don't overwrite old value
			victimShellCompensationTime = victimShellTime + deadTime; // wait for the last moment
			victimShellCompensationValue = time - victimShellTime - alignmentOffset - prtExcessTime; // jump back correct # of periods + delta
		}

		// end time
		endDamageTime = time + deadTime - alignmentOffset; // remove the alignmentOffset so lifetime is aligned to the skin prt
	}
	else {

		// skin smoke time
		int prtExcessTime = prtTotalTime - aliveTime;
		if ( endDamageTime == 0 || manualFadingOut ) { // new or already fading out
			victimShellTime = time;
			victimShellCompensationTime = time + aliveTime; // wait for the last moment
			victimShellCompensationValue = -prtExcessTime;  // jump fw so the prt ends early
		} else { // restart

			/*
			|---| = prtFadeinTime, i.e: 1 period (example: 1p, period = 3t, time unit)

			Example: particle (3t * 4periods) long

			|<-- start time = 0t
			|   |<-- period 1 (fadein) completes at 3t
			|       |<-- period 2 completes at 6t
			|           | <-- period 3 completes at 9t
			|               | <-- period 4 completes at 12t

			|  /|abc|def|ghi|\
			| / |   |   |   | \
			|/  |   |   |   |  \
			|

			Restart requested at 7t: alignmentOffset = 1t

			v2 -  delay jumps in time as much as possible so that multiple restarts may result in only 1 final jump which is aligned to the existing period
			|         |<-- restart at 7t. don't change particle time
			|               |<-- JUMP back in time at 12t! this when the original particle was meant to end. Jump back # periods to add((restartTime-oldTime)/prtFadeinTime) = (7t-0t)/3t = 2 periods = 6t = (restartTime-oldTime-alignmentOffset) = 7-0-1
			|                so we avoid the fadein and the total prt lifetime is = the correct lenght (actually, is 'alignmentOffset' shorter)
			|
			|               (j)
			|   |   | ef|ghi|def|ghi|\
			|   |   |   |   |   |   | \
			|   |   |   |   |   |   |  \
			|
			|            |<-- restart2 at 10t. don't change particle time. alignmentOffset = 1t
			|               |<-- JUMP back in time at 12t! this when the original particle was meant to end. Jump back # periods to add((restartTime-oldTime)/prtFadeinTime) = (10t-0t)/3t = 3 periods = 9t = (restartTime-oldTime-alignmentOffset) = 10-0-1
			|                so we avoid the fadein and the total prt lifetime is of the correct lenght (actually, is 'alignmentOffset' shorter)
			|               (j)
			|   |   |   | hi|abc|def|ghi|\
			|   |   |   |   |   |   |   | \
			|   |   |   |   |   |   |   |  \

			*/

			alignmentOffset = ( time - victimShellTime ) % prtFadeinTime;
//			victimShellTime = ...; // don't jump and don't overwrite old value
			victimShellCompensationTime = victimShellTime + aliveTime; // wait for the last moment
			victimShellCompensationValue = time - victimShellTime - alignmentOffset - prtExcessTime; // jump back correct # of periods
		}

		// end time
		if ( aliveTime > 0 ) {
			endDamageTime = time + aliveTime - alignmentOffset; // remove the alignmentOffset so lifetime is aligned to the skin prt
		} else {
			endDamageTime = -1; // -1 is endless
		}
	}

	// damageRate
	nextDamageTime = time + damageRate;

	// start the fx
	idEntityFx::Start( time );
}

/*
================
idDamagingFx::Think
================
*/
void idDamagingFx::Think( void ) {
	idEntityFx::Think();

	if ( started > 0 ) { // is started
		idEntity *victimEnt = victim.GetEntity();
		if ( !victimEnt || victimEnt->IsHidden() ) { // hide
			// remove the shell
			if ( victimShellHandle != -1 ) {
				gameRenderWorld->FreeEntityDef( victimShellHandle );
				victimShellHandle = -1;
			}
			// fade?
			if ( !manualFadingOut ) {
				FadeOutFx();
			}
		} else {
			if ( !manualFadingOut ) { // not fading
				if ( endDamageTime > 0 && endDamageTime <= gameLocal.time ) { // timed out
					FadeOutFx();
				} else if ( nextDamageTime < gameLocal.time ) { // damage time
					nextDamageTime = damageRate + gameLocal.time;
					victimEnt->Damage( this, ( victimEnt->health > 0 ) ? attacker.GetEntity() : this, vec3_origin, damageDefString.c_str() , 1.0f, INVALID_JOINT );
				}
			}
			// update shell - todo move in present()?
			if ( victimShellSkin ) {
				if ( victimShellCompensationTime > 0 && victimShellCompensationTime < gameLocal.time ) {
					victimShellCompensationTime = 0;
					victimShellTime += victimShellCompensationValue; // compensate for skipped time
				}

				renderEntity_t shell;
				shell = *victimEnt->GetRenderEntity();
				shell.entityNum = victimEnt->entityNumber;
				shell.customSkin = victimShellSkin;
				shell.shaderParms[SHADERPARM_TIMEOFFSET] = -MS2SEC( victimShellTime );
				shell.shaderParms[SHADERPARM_DIVERSITY] = 0.0f;

				if ( victimShellHandle == -1 ) {
					victimShellHandle = gameRenderWorld->AddEntityDef( &shell );
				} else {
					gameRenderWorld->UpdateEntityDef( victimShellHandle, &shell );
				}
			}
		}
	}
}

/*
================
idDamagingFx::StartDamagingFx
================
*/
idDamagingFx *idDamagingFx::StartDamagingFx( const char *defName, idEntity *attackerEnt, idEntity *victimEnt ) {
	idEntity		*ent;
	idDamagingFx	*nfx;
	const char		*jointName;
	const char		*skinName;

	if ( g_skipFX.GetBool() || !victimEnt ) {
		return NULL;
	}

	const idDeclEntityDef *fxDef = gameLocal.FindEntityDef( defName, false );
	if ( fxDef == NULL ) {
		gameLocal.Warning( "No def '%s' found for idDamagingFx", defName );
		return NULL;
	}

	if ( gameLocal.SpawnEntityDef( fxDef->dict, &ent, false ) && ent && ent->IsType( idDamagingFx::Type ) ) {
		nfx	= static_cast<idDamagingFx*>( ent );
		nfx->attacker = attackerEnt;

		if ( victimEnt->spawnArgs.GetString( nfx->spawnArgs.GetString( "key_joint" ), NULL, &jointName ) ) {
			StartFxUtility( nfx, NULL, &mat3_identity, victimEnt, true, false, victimEnt->GetAnimator()->GetJointHandle( jointName ) );
		} else {
			const idVec3 fxPos = victimEnt->GetPhysics()->GetAbsBounds().GetCenter();
			StartFxUtility( nfx, &fxPos, &mat3_identity, victimEnt, true, false );
		}

		if ( victimEnt->spawnArgs.GetString( nfx->spawnArgs.GetString( "key_skin" ), NULL, &skinName ) ) {
			nfx->victimShellSkin = declManager->FindSkin( skinName );
		}

		return nfx;
	} else {
		gameLocal.Error( "Could not spawn idDamagingFx" );
		return NULL;
	}
}

/*
================
idDamagingFx::Save
================
*/
void idDamagingFx::Save( idSaveGame *savefile ) const {
	savefile->WriteString( damageDefString );
	attacker.Save( savefile );
	savefile->WriteInt( nextDamageTime );
	savefile->WriteInt( damageRate );
	savefile->WriteInt( endDamageTime );
	savefile->WriteInt( prtFadeinTime );
	savefile->WriteInt( prtTotalTime );
	savefile->WriteInt( aliveTime );
	savefile->WriteInt( deadTime );
	savefile->WriteInt( victimShellHandle );
	savefile->WriteInt( victimShellTime );
	savefile->WriteInt( victimShellCompensationTime );
	savefile->WriteInt( victimShellCompensationValue );
	savefile->WriteSkin( victimShellSkin );
}

/*
================
idDamagingFx::Restore
================
*/
void idDamagingFx::Restore( idRestoreGame *savefile ) {
	savefile->ReadString( damageDefString );
	attacker.Restore( savefile );
	savefile->ReadInt( nextDamageTime );
	savefile->ReadInt( damageRate );
	savefile->ReadInt( endDamageTime );
	savefile->ReadInt( prtFadeinTime );
	savefile->ReadInt( prtTotalTime );
	savefile->ReadInt( aliveTime );
	savefile->ReadInt( deadTime );
	savefile->ReadInt( victimShellHandle );
	savefile->ReadInt( victimShellTime );
	savefile->ReadInt( victimShellCompensationTime );
	savefile->ReadInt( victimShellCompensationValue );
	savefile->ReadSkin( victimShellSkin );
}
