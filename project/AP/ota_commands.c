#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "Utils.h"
#include "Payload.h"
#include "ota_commands.h"
#include "FSM.h"
#include "LEDC.h"
#include "RTC.h"
#include "EEPROM.h"

typedef struct {
    const char* name;
    bool (*apply)(cJSON* value, void* ctx, char* err, size_t err_sz);
    bool required;
} OptionSpec;

typedef struct {
    const char* name;
    bool (*handle)(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz);
} CommandSpec;

static cJSON* json_get_opt(cJSON* data, cJSON* root, const char* key) {
    cJSON* v = data ? cJSON_GetObjectItemCaseSensitive(data, key) : NULL;
    if (!v && root) v = cJSON_GetObjectItemCaseSensitive(root, key);
    return v;
}

static bool json_get_bool(cJSON* node, bool* out) {
    if (!node) return false;
    if (cJSON_IsBool(node)) { *out = (bool)cJSON_IsTrue(node); return true; }
    if (cJSON_IsNumber(node)) { *out = (bool)(node->valuedouble != 0); return true; }
    if (cJSON_IsString(node) && node->valuestring) {
        const char* s = node->valuestring;
        if (!strcmp(s, "true") || !strcmp(s, "TRUE") || !strcmp(s, "1")) { *out = true; return true; }
        if (!strcmp(s, "false")|| !strcmp(s, "FALSE")|| !strcmp(s, "0")) { *out = false; return true; }
    }
    return false;
}

static bool json_get_int(cJSON* node, int* out) {
    if (!node) return false;
    if (cJSON_IsNumber(node)) { *out = (int)node->valuedouble; return true; }
    if (cJSON_IsString(node) && node->valuestring) { *out = atoi(node->valuestring); return true; }
    return false;
}
static bool json_get_str(cJSON* node, const char** out) {
    if (!node || !cJSON_IsString(node) || !node->valuestring) return false;
    *out = node->valuestring; return true;
}
static bool apply_options(const OptionSpec* specs, size_t n_specs,
                          cJSON* root, cJSON* data, void* ctx,
                          bool* any_applied, char* err, size_t err_sz) {
    bool ok = true; *any_applied = false;
    for (size_t i = 0; i < n_specs; i++) {
        const OptionSpec* s = &specs[i];
        cJSON* v = json_get_opt(data, root, s->name);
        if (!v) {
            if (s->required) {
                snprintf(err, err_sz, "missing required option '%s'", s->name);
                return false;
            }
            continue;
        }
        if (!s->apply(v, ctx, err, err_sz)) {
            ok = false;
            break;
        }
        *any_applied = true;
    }
    return ok;
}
													
typedef struct {
    int _unused;
} FWContext;

static bool command_fw(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
    (void)err; (void)err_sz;
    cJSON* d = data ? data : cJSON_GetObjectItemCaseSensitive(root, "data");

    ParsedPayload parsed = (ParsedPayload){0};
    strlcpy0(parsed.ty, "fw", sizeof(parsed.ty));
    parsed.cid = cid;
    parsed.success = false;  // Default to false, set true only if we have valid data
    parsed.xapi = 0;  // Default to 0 (no API key)
    
    cJSON* xapi_node = json_get_opt(data, root, "xapi");
    if (xapi_node) {
        int xapi_val;
        if (json_get_int(xapi_node, &xapi_val)) {
            if (xapi_val == 0 || xapi_val == 1) {
                parsed.xapi = xapi_val;
            }
        }
    }
    
    // Copy URL from JSON directly to parsed.data first (safer)
    if (d && cJSON_IsString(d) && d->valuestring && d->valuestring[0] != '\0') {
        // Copy directly to parsed.data first
        strlcpy0(parsed.data, d->valuestring, sizeof(parsed.data));
        // Then copy to fw_url_buf for later use
        strlcpy0(fw_url_buf, d->valuestring, sizeof(fw_url_buf));
        parsed.success = true;
    } else {
        // No URL provided - clear both
        parsed.data[0] = '\0';
        fw_url_buf[0] = '\0';
    }

    fwUpdate = parsed;
    fotaAvailable = false;  /* fwUpdate no longer holds FOTA data */
    updateAvailable = true;

    mqtt_send_action_response("fw", cid, parsed.success, 1);

    return parsed.success;
}

// ---------- "pu": parameter update ----------
typedef struct {
    int changes;
} PUContext;

static bool pu_apply_sendCookingSessions(cJSON* node, void* ctx, char* err, size_t err_sz) {
    bool v; if (!json_get_bool(node, &v)) { snprintf(err, err_sz, "sendCookingSessions must be bool"); return false; }
    g_params.send_cooking_sessions = v; ((PUContext*)ctx)->changes++; return true;
}
static bool pu_apply_sendErrors(cJSON* node, void* ctx, char* err, size_t err_sz) {
    bool v; if (!json_get_bool(node, &v)) { snprintf(err, err_sz, "sendErrors must be bool"); return false; }
    g_params.send_error_payloads = v; ((PUContext*)ctx)->changes++; return true;
}
static bool pu_apply_sendMeta(cJSON* node, void* ctx, char* err, size_t err_sz) {
    bool v; if (!json_get_bool(node, &v)) { snprintf(err, err_sz, "sendMeta must be bool"); return false; }
    g_params.send_meta_payloads = v; ((PUContext*)ctx)->changes++; return true;
}
bool keep_alive_changed = false;
bool persistent_flag_changed = false;

static bool pu_apply_keepAlive(cJSON* node, void* ctx, char* err, size_t err_sz) {
    int v;
    if (!json_get_int(node, &v)) { snprintf(err, err_sz, "keepAlive must be int or string"); return false; }
    if (v < 10 || v > 24*3600)   { snprintf(err, err_sz, "keepAlive must be 10..86400");   return false; }
    g_params.keep_alive_s = v;   ((PUContext*)ctx)->changes++;
		keep_alive_changed = true;
    return true;
}

static bool pu_apply_numCookBlock(cJSON* node, void* ctx, char* err, size_t err_sz) {
    int v;
    if (!json_get_int(node, &v)) { snprintf(err, err_sz, "numCookBlock must be int or string"); return false; }
    if (v < 1 || v > 10)   { snprintf(err, err_sz, "numCookBlock must be 1..10");   return false; }
    g_params.num_cooking_blocks = v;   ((PUContext*)ctx)->changes++;
    return true;
}

static bool pu_apply_persistentFlag(cJSON* node, void* ctx, char* err, size_t err_sz) {
    int v;
    if (!json_get_int(node, &v)) {
        snprintf(err, err_sz, "persistent must be 0 or 1");
        return false;
    }
    if (v != 0 && v != 1) {
        snprintf(err, err_sz, "persistent must be 0 or 1");
        return false;
    }

    if ((int)g_params.persistent_flag != v) {
        g_params.persistent_flag = (uint8_t)v;
        ((PUContext*)ctx)->changes++;
        persistent_flag_changed = true;
    }
    return true;
}

static bool pu_apply_payloadMode(cJSON* node, void* ctx, char* err, size_t err_sz) {
    const char* s;
    if (!json_get_str(node, &s)) { snprintf(err, err_sz, "payloadMode must be string"); return false; }
    if (!strcmp(s, "all"))        g_params.payload_mode = PAYLOAD_ALL;
    else if (!strcmp(s, "essential")) g_params.payload_mode = PAYLOAD_ESSENTIAL;
    else { snprintf(err, err_sz, "payloadMode invalid"); return false; }
    ((PUContext*)ctx)->changes++; return true;
}
static bool pu_apply_connStrategy(cJSON* node, void* ctx, char* err, size_t err_sz) {
    const char* s;
    if (!json_get_str(node, &s)) { snprintf(err, err_sz, "connStrategy must be string"); return false; }
    if (!strcmp(s, "always_on"))  g_params.conn_strategy = CONN_ALWAYS_ON;
    else if (!strcmp(s, "on_demand")) g_params.conn_strategy = CONN_ON_DEMAND;
		else if (!strcmp(s, "once")) g_params.conn_strategy = CONN_ONCE;
    else { snprintf(err, err_sz, "connStrategy invalid"); return false; }
    ((PUContext*)ctx)->changes++; return true;
}

static bool pu_apply_metaIntervalS(cJSON* node, void* ctx, char* err, size_t err_sz) {
    int v;
    if (!json_get_int(node, &v)) { snprintf(err, err_sz, "metaIntervalS must be int"); return false; }
    if (v < 1 || v > 24*3600)    { snprintf(err, err_sz, "metaIntervalS 1..86400");   return false; }
    g_params.meta_payload_interval_s = v;
    ((PUContext*)ctx)->changes++; return true;
}
static bool pu_apply_metaFreqHz(cJSON* node, void* ctx, char* err, size_t err_sz) {
    double hz = 0.0;
    if (cJSON_IsNumber(node)) {
        hz = node->valuedouble;
    } else if (cJSON_IsString(node) && node->valuestring) {
        hz = atof(node->valuestring);
    } else {
        snprintf(err, err_sz, "metaFreqHz must be number or string");
        return false;
    }
    if (hz <= 0.0 || hz > 10.0) { snprintf(err, err_sz, "metaFreqHz must be (0,10]"); return false; }
    int interval_ms = (int)(1000.0 / hz);
    if (interval_ms < 100) interval_ms = 100;
    g_params.meta_payload_interval_s = interval_ms / 1000;
    ((PUContext*)ctx)->changes++; return true;
}

static bool command_pu(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
		static const OptionSpec pu_specs[] = {
				{ "keepAlive",        pu_apply_keepAlive,     false },
				{ "persistentFlag",   pu_apply_persistentFlag,     false },
				{ "payloadMode",      pu_apply_payloadMode,   false },
				{ "connStrategy",     pu_apply_connStrategy,  false },
				{ "metaIntervalS",    pu_apply_metaIntervalS, false },
				{ "metaFreqHz",       pu_apply_metaFreqHz,    false },
				{ "sendCookingSessions", pu_apply_sendCookingSessions, false },
				{ "sendErrors",          pu_apply_sendErrors,          false },
				{ "sendMeta",            pu_apply_sendMeta,            false },
				{ "numCookBlock",            pu_apply_numCookBlock,            false },
		};

    PUContext ctx = {0};
    bool applied = false;
    bool ok = apply_options(pu_specs, sizeof(pu_specs)/sizeof(pu_specs[0]),
                            root, data, &ctx, &applied, err, err_sz);

 //    printf("pu: cid=%d, applied=%s (keepAlive=%d, payloadMode=%d, connStrategy=%d, metaIntervalS=%d)\n",
 //          cid, (ok && applied) ? "true" : "false",
 //          g_params.keep_alive_s, (int)g_params.payload_mode,
 //          (int)g_params.conn_strategy, g_params.meta_payload_interval_s);

    if (ok && applied) {
        Emulated_EEPROM_Write_RuntimeParams();
    }
    
    mqtt_send_action_response("pu", cid, (bool)(ok && applied), 1);
    return ok;
}

// ---------- "lb": load balancer / max power ----------
//typedef struct { int new_max; } LBContext;
typedef struct { int new_max; int duration_s; bool has_duration; } LBContext;

static bool lb_apply_durationS(cJSON* node, void* ctx, char* err, size_t err_sz) {
    int v;
    if (!json_get_int(node, &v)) { snprintf(err, err_sz, "durationS must be int/string"); return false; }
    if (v < 0) { snprintf(err, err_sz, "durationS must be >= 0"); return false; }
    ((LBContext*)ctx)->duration_s = v;
    ((LBContext*)ctx)->has_duration = true;
    return true;
}

static bool lb_apply_maxPower(cJSON* node, void* ctx, char* err, size_t err_sz) {
    int v;
    if (!json_get_int(node, &v)) { snprintf(err, err_sz, "maxPower must be int/string"); return false; }
    if (v < 0 || v > 10)         { snprintf(err, err_sz, "invalid maxPower (0-10)");   return false; }
    ((LBContext*)ctx)->new_max = v;
    return true;
}

static bool command_lb(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
		static const OptionSpec specs[] = {
				{ "maxPower",  lb_apply_maxPower,  false },
				{ "max_power", lb_apply_maxPower,  false },
				{ "duration",  lb_apply_durationS, false },
		};
		
    LBContext ctx = { .new_max = -1, .duration_s = 0, .has_duration = false };
    bool applied = false;
    bool ok = apply_options(specs, sizeof(specs)/sizeof(specs[0]),
                            root, data, &ctx, &applied, err, err_sz);

    if (!applied) {
        int v;
        if ((data && json_get_int(data, &v))) { ctx.new_max = v; applied = true; ok = true; }
        else if (!ok) { /* keep err */ }
        else { snprintf(err, err_sz, "no usable maxPower"); ok = false; }
    }

    if (ok && ctx.new_max >= 0) {
        g_params.max_power_w = ctx.new_max;
			
						if (ctx.has_duration) {
				if (ctx.duration_s > 0) {
						g_params.lb_cap_until_epoch = (uint32_t)device_time + (uint32_t)ctx.duration_s;
				} else {
						g_params.lb_cap_until_epoch = (uint32_t)device_time + (uint32_t)DEFAULT_LOAD_BALANCER_DURATION;
				}
		}
				else {
						g_params.lb_cap_until_epoch = (uint32_t)device_time + (uint32_t)DEFAULT_LOAD_BALANCER_DURATION;
				}

       Emulated_EEPROM_Write_RuntimeParams();

        // --- NEW: enforce cap immediately ---
        int cap = g_params.max_power_w;
        bool any = false;

        // Disable boost if new cap < 10
        if (cap < 10) {
            if (left.boost)  { BoostMode(0, false); any = true; }
            if (right.boost) { BoostMode(1, false); any = true; }
        }

        // Clamp running power to the new cap
        if (left.power  > cap)  { startSessionForSelectedPan(0, cap); any = true; }
        if (right.power > cap)  { startSessionForSelectedPan(1, cap); any = true; }

        // Refresh LEDs/display if anything changed
        if (any) {
            writePowerLevel(left.power, right.power);
            if (selectedPan == 0)      LEDC_setSingleRedLED(left.power  <= 9 ? left.power  : 9);
            else if (selectedPan == 1) LEDC_setSingleRedLED(right.power <= 9 ? right.power : 9);
        }
        // ------------------------------------

//        printf("lb: cid=%d, max_power_w set to %d\n", cid, g_params.max_power_w);
        mqtt_send_action_response("lb", cid, true, 1);
        return true;
    }

//    printf("lb: cid=%d, %s\n", cid, err);
    mqtt_send_action_response("lb", cid, false, 1);
    return false;
}


// ---------- "so": shutoff ----------
static bool command_so(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
    (void)root; (void)data; (void)err; (void)err_sz;
    shutoff_func();
    mqtt_send_action_response("so", cid, true, 1);
    return true;
}

// ---------- "pg": paygo placeholder ----------
static bool command_pg(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
    (void)root; (void)data; (void)err; (void)err_sz;
    mqtt_send_action_response("pg", cid, false, 1);
    return false;
}

static bool command_mr(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
    (void)root; (void)data; (void)err; (void)err_sz;
    /* Queue through sendPayload → send24Hrs(); do not publish from here. */
    metaForceRequest();
    mqtt_send_action_response("mr", cid, true, 1);
    return true;
}

static bool command_rd(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
    (void)root; (void)data; (void)err; (void)err_sz;
		params_reset_to_defaults();
		Emulated_EEPROM_Write_RuntimeParams();
    mqtt_send_action_response("rd", cid, true, 1);
    return false;
}

// ---------- "rp": read runtime parameters from EEPROM ----------
static bool command_rp(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
    (void)root; (void)data; (void)err; (void)err_sz;
    
   ErrStatus status = Emulated_EEPROM_Read_RuntimeParams();
    
   mqtt_send_action_response("rp", cid, (bool)(status == SUCCESS), 1);
   return (bool)(status == SUCCESS);
}

bool fotaAvailable = false;

static bool command_fota(int cid, cJSON* root, cJSON* data, char* err, size_t err_sz) {
    cJSON* url_node = json_get_opt(data, root, "data");
    cJSON* usr_node = json_get_opt(data, root, "usr");
    cJSON* pwd_node = json_get_opt(data, root, "pwd");

    const char *url = NULL, *usr = NULL, *pwd = NULL;

    if (!json_get_str(url_node, &url) || url[0] == '\0') {
        snprintf(err, err_sz, "fota: missing or empty 'data' (URL)");
        mqtt_send_action_response("fota", cid, false, 1);
        return false;
    }

    if (!json_get_str(usr_node, &usr)) usr = "";
    if (!json_get_str(pwd_node, &pwd)) pwd = "";

    snprintf(fwUpdate.data, sizeof(fwUpdate.data),
             "AT+CFOTA=0,1,\"%s\",\"%s\",\"%s\"", url, usr, pwd);
    strlcpy0(fwUpdate.ty, "fota", sizeof(fwUpdate.ty));

    fotaAvailable = true;
    mqtt_send_action_response("fota", cid, true, 1);

    return true;
}

// ======================= Registry + dispatch ======================
bool dispatch_command(const char* ty, int cid, cJSON* root, cJSON* data) {
    if (ty == NULL) {
        return false;
    }

    char err[128] = {0};
    static const CommandSpec CMDS[] = {
        { "fw",   command_fw   },
        { "fota", command_fota },
        { "pu", command_pu },
        { "lb", command_lb },
        { "so", command_so },
        { "pg", command_pg },
				{ "mr", command_mr },
				{ "rd", command_rd },
				{ "rp", command_rp },
        // add new commands here
    };
    for (size_t i = 0; i < sizeof(CMDS)/sizeof(CMDS[0]); i++) {
        if (strcmp(ty, CMDS[i].name) == 0) {
            return CMDS[i].handle(cid, root, data, err, sizeof(err));
        }
    }
//    printf("Unknown 'ty': %s (cid=%d)\n", ty, cid);
    mqtt_send_action_response(ty, cid, false, 1);
    return false;
}
