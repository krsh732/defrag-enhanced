#include "cgame.h"

#define RECALL_STATES_CAPACITY (60 * 1000 / 8)  // 1 min worth at 125fps
#define RECALL_HISTORY_EXT ".cd"
#define RECALL_DEFAULT_HISTORY_PATH "temp/current" RECALL_HISTORY_EXT

typedef struct {
    int write_idx;
    int length;
    saveState_t states[RECALL_STATES_CAPACITY];
} recallHistory_t;

static recallHistory_t history;

typedef enum {
    RECALL_RECORD_OFF,
    RECALL_RECORD_ON,
    RECALL_RECORD_PERSIST
} recallRecordMode_t;

typedef enum {
    HISTORY_LOAD_SUCCESS,
    HISTORY_LOAD_OPEN_FAILED,
    HISTORY_LOAD_INVALID_FILE
} historyLoadStatus_t;

static struct {
    qboolean visible;
    int cursor;
    qboolean advancing;
    qboolean rewinding;
} viewer;

static void Viewer_Scrub(int offset)
{
    int length = history.length;

    if (length <= 0) {
        return;
    }

    viewer.cursor = (viewer.cursor + (offset % length) + length) % length;
}

static void Viewer_SeekToLatest(void)
{
    viewer.cursor = history.write_idx;
    Viewer_Scrub(-1);
}

void CG_Recall_AddState(void)
{
    if (!cg_recallRecordMode.integer || viewer.visible) {
        return;
    }

    if (!CaptureCurrentState(&history.states[history.write_idx])) {
        return;
    }

    if (history.length < RECALL_STATES_CAPACITY) {
        history.length += 1;
    }
    history.write_idx = (history.write_idx + 1) % RECALL_STATES_CAPACITY;
}

saveState_t* CG_Recall_GetState(void)
{
    if (!viewer.visible) {
        return NULL;
    }

    return &history.states[viewer.cursor];
}

void CG_Recall_Draw(void)
{
    refdef_t refdef;

    if (!viewer.visible) {
        return;
    }

    if (viewer.advancing) {
        Viewer_Scrub(+1);
    }
    if (viewer.rewinding) {
        Viewer_Scrub(-1);
    }

    memset(&refdef, 0, sizeof(refdef));
    AxisClear(refdef.viewaxis);
    AnglesToAxis(history.states[viewer.cursor].viewangles, refdef.viewaxis);
    refdef.fov_x = cg.refdef.fov_x;
    refdef.fov_y = cg.refdef.fov_y;
    // KTODO: customizable?
    refdef.x = 0;
    refdef.y = 0;
    refdef.width = 300;
    refdef.height = 300;
    refdef.time = cg.time;
    VectorCopy(history.states[viewer.cursor].origin, refdef.vieworg);
    trap_R_RenderScene(&refdef);
}

consoleCommandStatus_t CG_Recall_f(void)
{
    if (!viewer.visible && history.length <= 0) {
        Com_Printf(LOG_ERROR "Nothing to recall\n");
        return CON_CMD_HANDLED;
    }

    viewer.visible = !viewer.visible;
    if (viewer.visible) {
        Viewer_SeekToLatest();
    }

    Com_Printf(LOG_INFO "Recall (%s)\n", (viewer.visible) ? "ON" : "OFF");
    return CON_CMD_HANDLED;
}

consoleCommandStatus_t IN_Recall_AdvanceDown(void)
{
    viewer.advancing = qtrue;
    // KTODO: too much cruft each time, I wonder if restorestate should be
    // special-cased as it's far more unlikely that one would want forwardable
    // commands
    return CON_CMD_HANDLED;
}

consoleCommandStatus_t IN_Recall_AdvanceUp(void)
{
    viewer.advancing = qfalse;
    return CON_CMD_HANDLED;
}

consoleCommandStatus_t IN_Recall_RewindDown(void)
{
    viewer.rewinding = qtrue;
    return CON_CMD_HANDLED;
}

consoleCommandStatus_t IN_Recall_RewindUp(void)
{
    viewer.rewinding = qfalse;
    return CON_CMD_HANDLED;
}

static void DiscardHistory(void)
{
    history.length = history.write_idx = 0;
    viewer.visible = qfalse;
}

// Returns false on failure.
static qboolean SaveHistory(const char* path)
{
    fileHandle_t f;

    trap_FS_FOpenFile(path, &f, FS_WRITE);
    if (!f) {
        return qfalse;
    }

    trap_FS_Write(&history, sizeof(history), f);
    trap_FS_FCloseFile(f);
    return qtrue;
}

consoleCommandStatus_t CG_Recall_SaveHistory_f(void)
{
    char path[MAX_OSPATH];

    trap_Argv(1, path, sizeof(path));
    if (path[0] == '\0') {
        Com_Printf("Usage: /%s <path>\n", CG_Argv(0));
        return CON_CMD_HANDLED;
    }

    if (history.length <= 0) {
        Com_Printf(LOG_ERROR "recall history is empty, nothing to save\n");
        return CON_CMD_HANDLED;
    }

    // KTODO: extension
    if (!SaveHistory(path)) {
        Com_Printf(LOG_ERROR "failed to save recall history to \"%s\"\n", path);
        return CON_CMD_HANDLED;
    }

    Com_Printf(LOG_INFO "saved recall history to \"%s\"\n", path);
    return CON_CMD_HANDLED;
}

void CG_Recall_AutoSaveHistory(void)
{
    if (
       cg_recallRecordMode.integer != RECALL_RECORD_PERSIST ||
       history.length <= 0
    ) {
        return;
    }

    if (!SaveHistory(RECALL_DEFAULT_HISTORY_PATH)) {
        Com_Printf(
           LOG_ERROR
           "failed to autosave recall history to \"" RECALL_DEFAULT_HISTORY_PATH
           "\"\n");
    }
}

// Returns false on validation failure.
static qboolean ValidateHistory(void)
{
    if (history.length <= 0 || history.length > RECALL_STATES_CAPACITY) {
        return qfalse;
    }
    if (history.write_idx < 0 || history.write_idx >= RECALL_STATES_CAPACITY) {
        return qfalse;
    }
    if (
       history.length < RECALL_STATES_CAPACITY &&
       history.write_idx != history.length
    ) {
        return qfalse;
    }
    // KTODO: validate states

    return qtrue;
}

static historyLoadStatus_t LoadHistory(const char* path)
{
    int len;
    fileHandle_t f;

    len = trap_FS_FOpenFile(path, &f, FS_READ);
    if (!f) {
        return HISTORY_LOAD_OPEN_FAILED;
    }
    if (len != sizeof(history)) {
        trap_FS_FCloseFile(f);
        return HISTORY_LOAD_INVALID_FILE;
    }

    trap_FS_Read(&history, sizeof(history), f);
    trap_FS_FCloseFile(f);
    if (!ValidateHistory()) {
        DiscardHistory();
        return HISTORY_LOAD_INVALID_FILE;
    }

    Viewer_SeekToLatest();
    return HISTORY_LOAD_SUCCESS;
}

consoleCommandStatus_t CG_Recall_LoadHistory_f(void)
{
    char path[MAX_OSPATH];
    historyLoadStatus_t status;

    trap_Argv(1, path, sizeof(path));
    if (path[0] == '\0') {
        Com_Printf("Usage: /%s <path>\n", CG_Argv(0));
        return CON_CMD_HANDLED;
    }

    // KTODO: extension
    status = LoadHistory(path);
    switch (status) {
        case HISTORY_LOAD_SUCCESS:
            Com_Printf(LOG_INFO "loaded recall history file \"%s\"\n", path);
            break;
        case HISTORY_LOAD_OPEN_FAILED:
            Com_Printf(LOG_ERROR "failed to load recall history file \"%s\"\n",
                       path);
            break;
        case HISTORY_LOAD_INVALID_FILE:
            Com_Printf(LOG_ERROR "corrupted/invalid history in \"%s\"\n", path);
            break;
    }
    return CON_CMD_HANDLED;
}

void CG_Recall_AutoLoadHistory(void)
{
    if (cg_recallRecordMode.integer != RECALL_RECORD_PERSIST) {
        return;
    }

    if (LoadHistory(RECALL_DEFAULT_HISTORY_PATH) == HISTORY_LOAD_INVALID_FILE) {
        Com_Printf(LOG_ERROR
                   "failed to autoload corrupted/invalid recall history in "
                   "\"" RECALL_DEFAULT_HISTORY_PATH "\"\n");
    }
}
