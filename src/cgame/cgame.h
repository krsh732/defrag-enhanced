#ifndef CGAME_HEADER_GUARD__
#define CGAME_HEADER_GUARD__

#include "utils.h"
#include "assert.h"
#include "collections.h"
#include "cvar.h"
#include "extensions.h"
#include "fs.h"
#include "hook.h"
#include "log.h"
#include "cg_local.h"
#include "bg_savestate.h"
#include "qfiles.h"
#include "q_math.h"

// TODO: maybe consider moving this out to a separate header if it gets too big
#define FOR_EACH_CVAR(V)                                                       \
    V(cg_entitiesDraw, "0", CVAR_ARCHIVE,                                      \
      "Draw map entities and the connections between them.")                   \
                                                                               \
    V(cg_entitiesMaxDistance, "1000", CVAR_ARCHIVE,                            \
      "The distance at which entities drawn by cg_entitiesDraw will fade out " \
      "of view.")                                                              \
                                                                               \
    V(cg_spawnPointsDraw, "0", CVAR_ARCHIVE,                                   \
      "Draw player models at spawn points.")                                   \
                                                                               \
    V(cg_spawnPointsShader, "dfe/spawnpoint", CVAR_ARCHIVE,                    \
      "The shader used by cg_spawnPointsDraw.")                                \
                                                                               \
    V(cg_spawnPointsColor, "0.0 1.0 0.5 0.125", CVAR_ARCHIVE,                  \
      "The color of non-team spawn points.")                                   \
                                                                               \
    V(cg_spawnPointsColorRed, "1.0 0.25 0.25 0.25", CVAR_ARCHIVE,              \
      "The color of red team spawn points. Set to \"\" to use "                \
      "cg_spawnPointsColor.")                                                  \
                                                                               \
    V(cg_spawnPointsColorBlue, "0.0 0.5 1.0 0.25", CVAR_ARCHIVE,               \
      "The color of blue team spawn points. Set to \"\" to use "               \
      "cg_spawnPointsColor.")                                                  \
                                                                               \
    V(cg_overbounceDraw, "0", CVAR_ARCHIVE,                                    \
      "Highlight surfaces on which an overbounce is possible.")                \
                                                                               \
    V(cg_overbounceColorGo, "0 0.5 0.75 0.5", CVAR_ARCHIVE,                    \
      "\"Go\" overbounce highlight color.")                                    \
                                                                               \
    V(cg_overbounceColorJump, "0.5 0.5 0.1 0.5", CVAR_ARCHIVE,                 \
      "\"Jump\" overbounce highlight color.")                                  \
                                                                               \
    V(cg_overbounceFadeTime, "0.1", CVAR_ARCHIVE,                              \
      "The number of seconds it takes for overbounce highlights to fade "      \
      "fully in or out.")                                                      \
                                                                               \
    V(cg_overbounceIgnoreKillOBs, "0", CVAR_ARCHIVE,                           \
      "Show overbounces even when df_ob_KillOBs is enabled.")                  \
                                                                               \
    V(cg_overbounceIgnoreNoOB, "0", CVAR_ARCHIVE,                              \
      "Show overbounces on no-OB surfaces.")                                   \
                                                                               \
    V(cg_overbounceMaxTestsPerFrame, "50", CVAR_ARCHIVE,                       \
      "The maximum number of checks for overbounces allowed per frame.")       \
                                                                               \
    V(cg_recallRecordMode, "0", CVAR_ARCHIVE,                                  \
      "Record states to the recall history buffer.\n"                          \
      "0: Don't record\n"                                                      \
      "1: Record temporarily (history does not persist across sessions)\n"     \
      "2: Autosave/load (history persists across sessions)")

#define DECLARE_CVAR_(name, default, flags, description) extern vmCvar_t name;
FOR_EACH_CVAR(DECLARE_CVAR_)
#undef DECLARE_CVAR_

//
// cg_bsp.c
//
struct entity_s;
typedef struct entity_s entity_t;

struct entity_s {
    char* classname;
    char* target;
    char* targetname;
    entity_t** targets;
    vec3_t origin;
    vec3_t angles;
};

typedef struct {
    vec(dshader_t) shaders;
    vec(dmodel_t) models;
    vec(entity_t) entities;
    vec(dplane_t) planes;
    vec(dbrush_t) brushes;
    vec(dbrushside_t) brushSides;
} bsp_t;

extern bsp_t bsp;

void CG_LoadBSP(void);

//
// cg_consolecmds.c
//

// This type alias is simply here for readability/convenience so we don't
// have to remember what true/false is or go look it up each time. NOTE:
// It is not its own enum type as we use it as CG_ConsoleCommand's return
// type and making it a separate enum would cause conflicting
// redeclaration errors with the signature declared in SDK's cg_local.h
typedef qboolean consoleCommandStatus_t;
#define CON_CMD_NOT_HANDLED qfalse
#define CON_CMD_HANDLED qtrue

#define FOR_EACH_CONSOLE_COMMAND(V)                   \
    V("savestate", CG_SaveState_f)                    \
    V(RESTORE_STATE_CMD, CG_RestoreState_f)           \
    V("recall", CG_Recall_f)                          \
    V("recall_save_history", CG_Recall_SaveHistory_f) \
    V("recall_load_history", CG_Recall_LoadHistory_f) \
    V("+recall_advance", IN_Recall_AdvanceDown)       \
    V("-recall_advance", IN_Recall_AdvanceUp)         \
    V("+recall_rewind", IN_Recall_RewindDown)         \
    V("-recall_rewind", IN_Recall_RewindUp)           \
    V("cgMallocStats", CG_MallocStats_f)

#define DECLARE_COMMAND_(name, func) consoleCommandStatus_t func(void);
FOR_EACH_CONSOLE_COMMAND(DECLARE_COMMAND_)
#undef DECLARE_COMMAND_

//
// cg_draw.c
//
void CG_DrawBoundingBox(const vec3_t origin, const vec3_t mins,
                        const vec3_t maxs, const byte color[4]);
//
// cg_entity_viewer.c
//
void CG_AddEntityPOIs(void);
void CG_AddCEntityPOI(centity_t* cent);
void CG_DrawEntityConnections(void);
void CG_DrawSpawnPoints(void);

//
// cg_overbounce.c
//
void CG_DrawOBs(void);

//
// cg_poi.c
//
void CG_AddTextPOI(const vec3_t origin, const char* text, float max_dist);
void CG_DrawPOIs(void);

//
// cg_recall.c
//

// Returns NULL if not recalling.
saveState_t* CG_Recall_GetState(void);

void CG_Recall_AddState(void);
void CG_Recall_Draw(void);
void CG_Recall_AutoSaveHistory(void);
void CG_Recall_AutoLoadHistory(void);

//
// cg_savestate.c
//

// Returns false if current state is not supported.
qboolean CaptureCurrentState(saveState_t* out);

//
// df_stuff.c (og df stuff that you don't know/care where to place)
//
extern vmCvar_t cg_drawBBox;
extern vmCvar_t df_cl_alwaysDrawItems;
extern vmCvar_t df_ob_AllSlopes;

extern defragInfo_t defragInfo;

extern checkpointStatus_t checkpoint_status;
extern int num_checkpoints_hit;
extern int timer_time;

qboolean IsItemEntityAvailableToClient(entityState_t* state, int clientNum);

void DF_LoadBestCheckpoints(void);
void DF_LoadSessionCheckpoints(void);

// Updates some global timer thingy if it changed between snap and prev
// and returns the timer
int UpdateTimer(snapshot_t* snap, snapshot_t* prev);

extern int obStatus[OB_MAX];
extern float obLastOffset[OB_MAX];
extern float obOffset[OB_MAX];
extern int obCurrentStickyType, obCurrentWeapon;

int CheckOB(obType_t obType, float originZ, float velocityZ, float downFloorZ,
            float viewFloorZ, float* outDist1, float* outDist2);
qboolean NeedOBInfo(obType_t obType);
void TraceDown(trace_t* trace, vec3_t origin);
void TraceCrosshair(trace_t* trace, vec3_t origin, vec3_t viewangles);

#endif  // CGAME_HEADER_GUARD__
