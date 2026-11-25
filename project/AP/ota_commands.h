#include "ht32.h"
#include "cJSON.h"

// Dispatch a parsed command to its handler.
// ty  : command name (e.g., "pu", "fw", ...)
// cid : correlation id
// root: full JSON object
// data: "data" field (may be NULL)
extern bool keep_alive_changed;
extern bool persistent_flag_changed;
extern bool updateAvailable;
extern bool updateAnimationActive;

extern char fw_url_buf[256];

extern bool fotaAvailable;

bool gsmFotaDownload(const char *atCmd);

bool dispatch_command(const char* ty, int cid, cJSON* root, cJSON* data);
