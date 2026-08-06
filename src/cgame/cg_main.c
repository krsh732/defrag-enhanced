#include "cgame.h"

#define DEFINE_CVAR_(name, default, flags, description) vmCvar_t name;
FOR_EACH_CVAR(DEFINE_CVAR_)
#undef DEFINE_CVAR_

typedef struct {
    vmCvar_t* vmCvar;
    char* cvarName;
    char* defaultString;
    int cvarFlags;
    char* description;
} cvarTable_t;

static cvarTable_t cvarTable[] = {
#define CVAR_TABLE_ENTRY_(name, default, flags, description) \
    {&(name), #name, default, flags, description},
   FOR_EACH_CVAR(CVAR_TABLE_ENTRY_)
#undef CVAR_TABLE_ENTRY_
};

static int cvarTableSize = ARRAY_LENGTH(cvarTable);

/*
=================
CG_RegisterCvars
=================
*/
DEFINE_HOOK(void, CG_RegisterCvars, (void))
    int i;
    cvarTable_t* cv;

    ORIGINAL(CG_RegisterCvars)();

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        trap_Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString,
                           cv->cvarFlags);
        trap_Cvar_SetDescription_Q3E(cv->cvarName, cv->description);
    }
END_HOOK

/*
=================
CG_UpdateCvars
=================
*/
DEFINE_HOOK(void, CG_UpdateCvars, (void))
    int i;
    cvarTable_t* cv;

    ORIGINAL(CG_UpdateCvars)();

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        trap_Cvar_Update(cv->vmCvar);
    }
END_HOOK

/*
=================
CG_Init
=================
*/
DEFINE_HOOK(void, CG_Init,
            (int serverMessageNum, int serverCommandSequence, int clientNum))
    trap_Print("^0HELLO ^1WORLD ^7(CGAME)\n");
    ORIGINAL(CG_Init)(serverMessageNum, serverCommandSequence, clientNum);
    CG_Recall_AutoLoadHistory();
END_HOOK

/*
=================
CG_Shutdown

Called before every level change or subsystem restart
=================
*/
DEFINE_HOOK(void, CG_Shutdown, (void))
    CG_Recall_AutoSaveHistory();
    ORIGINAL(CG_Shutdown)();
END_HOOK
