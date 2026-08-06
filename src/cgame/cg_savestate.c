#include "cgame.h"

consoleCommandStatus_t CG_SaveState_f(void)
{
    char cvar_name[MAX_TOKEN_CHARS];
    char restore_command[MAX_STRING_CHARS];
    saveState_t* state;
    saveState_t current_state;

    trap_Argv(1, cvar_name, sizeof(cvar_name));
    if (cvar_name[0] == '\0') {
        Com_Printf("Usage: /%s <cvar name>\n", CG_Argv(0));
        return CON_CMD_HANDLED;
    }

    if (!Cvar_ValidateName(cvar_name)) {
        Com_Printf(LOG_ERROR "Invalid cvar name\n");
        return CON_CMD_HANDLED;
    }

    state = CG_Recall_GetState();
    if (!state) {
        if (!CaptureCurrentState(&current_state)) {
            trap_Print(LOG_ERROR "Saving current state is not supported\n");
            return CON_CMD_HANDLED;
        }
        state = &current_state;
    }

#define PREFIX_ RESTORE_STATE_CMD " "
#define PREFIX_LEN_ sizeof(PREFIX_)
    static_assert_stmt(
       PREFIX_LEN_ + SERIALIZED_SAVESTATE_SIZE < sizeof(restore_command),
       "restore command won't fit");
    Q_strncpyz(restore_command, PREFIX_, sizeof(restore_command));
    SerializeSaveState(state, restore_command + PREFIX_LEN_ - 1);
#undef PREFIX_LEN_
#undef PREFIX_

    trap_Cvar_Set(cvar_name, restore_command);

    trap_Print(LOG_INFO "Saved\n");
    return CON_CMD_HANDLED;
}

// Command interceptor to restore some stuff client-side before forwarding the
// command along to the server. Ideally the server would handle everything, and
// nothing needs to be done here, but some stuff stuff is just inherently
// client-side like the checkpoint history...
consoleCommandStatus_t CG_RestoreState_f(void)
{
    char serialized_state[MAX_TOKEN_CHARS];
    saveState_t state;

    // NOTE: The server does more validation than here and can eventually reject
    // the command while we already change stuff here anyway. While we can come
    // up with some fancy design to keep them synced somehow, it's not really
    // that important. Like, yeah you might switch weapons or restore cp while
    // the server doesn't restore the rest. But, so what? What is important is
    // that we don't try to restore stuff client side when cheats are off, lest
    // it break normal gameplay somehow.
    if (!defragInfo.sv_cheats) {
        trap_Print(LOG_ERROR "Cheats are not enabled on this server\n");
        return CON_CMD_HANDLED;
    }

    trap_Argv(1, serialized_state, sizeof(serialized_state));
    if (serialized_state[0] == '\0') {
        Com_Printf("Usage: /%s <savestate>\n", CG_Argv(0));
        return CON_CMD_HANDLED;
    }

    if (!DeserializeSaveState(serialized_state, &state)) {
        trap_Print(LOG_ERROR "Corrupted/invalid savestate\n");
        return CON_CMD_HANDLED;
    }

    // If the currently equipped weapon is different from the one in the
    // savestate, the server will think the client is trying to weapon change
    // away from the one in the savestate. This is because, the server processes
    // ucmds after the restorestate command (see SV_ExecuteClientMessage), and
    // thinks the client wants to switch to the current weapon instead. That's
    // the exact opposite of what we want, so to prevent it, patch the weapon
    // in the current ucmd to be consistent with the savestate.
    cg.weaponSelect = state.weapon;
    // The ucmd set technically isn't needed as engines run console commands
    // before draw active where cg.weaponSelect is ucmd set, but just in case.
    trap_SetUserCmdValue(cg.weaponSelect, cg.zoomSensitivity);

    num_checkpoints_hit = state.num_checkpoints_hit;
    // This needs to be set accordingly to handle the edge case of restoring to
    // a state where the timer had started from one where we haven't started it
    // yet. As otherwise, the checkpoint print function will fallback to not
    // delta-ing even if df_checkpoints cvar was set to do so.
    checkpoint_status.timer_started = state.timer_running || state.timer_time;

    // let it go through so the server can do the actual restoring
    return CON_CMD_NOT_HANDLED;
}

// This function is kinda fragile in that if defrag ever changes the
// implementation of the underlying UpdateTimer to add/remove side-effects, we
// might end up doing unintended stuff. I doubt defrag would ever change though,
// so let's not care too much, since the alternative would be to reimplement the
// decryption logic ourselves.
static int GetTimerTime(snapshot_t* snap)
{
    int tmp = timer_time;
    int time = UpdateTimer(snap, NULL);

    timer_time = tmp;
    return time;
}

qboolean CaptureCurrentState(saveState_t* out)
{
    playerState_t* ps = &cg.snap->ps;

    if (ps->pm_type != PM_NORMAL) {
        return qfalse;
    }

    out->pm_flags = ps->pm_flags;
    out->pm_time = ps->pm_time;
    VectorCopy(ps->origin, out->origin);
    VectorCopy(ps->velocity, out->velocity);
    out->weaponTime = ps->weaponTime;
    VectorCopy(ps->viewangles, out->viewangles);
    out->health = ps->stats[STAT_HEALTH];
    out->holdable = ps->stats[STAT_HOLDABLE_ITEM];
    out->weapons = ps->stats[STAT_WEAPONS];
    out->armor = ps->stats[STAT_ARMOR];
    out->jump_time = ps->stats[STAT_JUMPTIME];
    out->djing = ps->stats[STAT_DJING];
    out->score = ps->persistant[PERS_SCORE];
    memcpy(out->powerups, ps->powerups, sizeof(ps->powerups));
    memcpy(out->ammo, ps->ammo, sizeof(ps->ammo));
    out->weapon = ps->weapon;
    out->weaponstate = ps->weaponstate;
    out->serverTime = cg.snap->serverTime;
    out->timer_time = GetTimerTime(cg.snap);
    out->timer_running = !!(ps->stats[STAT_MISC] & MISC_TIMER_RUNNING);
    out->num_checkpoints_hit = num_checkpoints_hit;

    return qtrue;
}
