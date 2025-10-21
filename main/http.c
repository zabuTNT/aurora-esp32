#include "http.h"
#include "esp_http_server.h"
#include "aurora.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static esp_err_t health_get_handler(httpd_req_t *req) {
    const uint8_t *keys[1] = { (const uint8_t*)"ST" };
    uint8_t CMD_ST[6] = { 0x02, 0x32, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t *cmds[1] = { CMD_ST };
    aurora_reply_t out[1] = {0};
    if (aurora_query_batch(out, keys, cmds, 1, CONFIG_INVERTER_IP, CONFIG_INVERTER_PORT)) {
        const char *ok = "OK";
        httpd_resp_send(req, ok, strlen(ok));
        return ESP_OK;
    }
    httpd_resp_set_status(req, "503 Service Unavailable");
    const char *err = "ERROR";
    httpd_resp_send(req, err, strlen(err));
    return ESP_OK;
}

static void bytes_to_hex(const uint8_t *in, int len, char *out, int outsz) {
    static const char *hex = "0123456789ABCDEF";
    int j = 0;
    for (int i = 0; i < len && j + 2 < outsz; i++) {
        out[j++] = hex[(in[i] >> 4) & 0xF];
        out[j++] = hex[(in[i] & 0xF)];
    }
    if (j < outsz) out[j] = '\0';
}

static void build_command_set(const uint8_t **cmds, const char **names, int *count) {
    static uint8_t CMD_ST[6]   = { 0x02, 0x32, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t CMD_CET[6]  = { 0x02, 0x4e, 0x05, 0x00, 0x00, 0x00 };
    static uint8_t CMD_CED[6]  = { 0x02, 0x4e, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t CMD_DSP3[6] = { 0x02, 0x3b, 0x03, 0x00, 0x00, 0x00 };
    static uint8_t CMD_DSP23[6]= { 0x02, 0x3b, 0x17, 0x00, 0x00, 0x00 };
    static uint8_t CMD_DSP25[6]= { 0x02, 0x3b, 0x19, 0x00, 0x00, 0x00 };
    static uint8_t CMD_DSP26[6]= { 0x02, 0x3b, 0x1A, 0x00, 0x00, 0x00 };
    static uint8_t CMD_DSP27[6]= { 0x02, 0x3b, 0x1B, 0x00, 0x00, 0x00 };
    static uint8_t CMD_SN[6]   = { 0x02, 0x3f, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t CMD_VR[6]   = { 0x02, 0x3A, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t *local_cmds[] = { CMD_ST, CMD_CET, CMD_CED, CMD_DSP3, CMD_DSP23, CMD_DSP25, CMD_DSP26, CMD_DSP27, CMD_SN, CMD_VR };
    const char *local_names[]   = { "ST",  "CET",  "CED",  "DSP3",  "DSP23",  "DSP25",  "DSP26",  "DSP27",  "SN",  "VR" };
    for (int i = 0; i < 10; i++) { cmds[i] = local_cmds[i]; names[i] = local_names[i]; }
    *count = 10;
}

// Human-readable labels (ported from Go statics)
static const char *GLOBAL_STATE_LABEL[] = {
    "Sending Parameters", "Wait Sun/Grid", "Checking Grid", "Measuring Riso",
    "DcDc Start", "Inverter", "Run ", "Recovery", "Pause", "Ground Fault",
    "OTH Fault", "Address Setting", "Self Test", "Self Test Fail", "Sensor Test + Meas.Riso",
    "Leak Fault", "Waiting for manual reset",
    "Internal Error E026", "Internal Error E027", "Internal Error E028", "Internal Error E029", "Internal Error E030",
    "Sending Wind Table", "Failed Sending table", "UTH Fault", "Remote OFF", "Interlock Fail",
    "Executing Autotest", "Waiting Sun", "Temperature Fault", "Fan Staucked",
    "Int. Com. Fault", "Slave Insertion", "DC Switch Open", "TRAS Switch Open", "MASTER Exclusion",
    "Auto Exclusion", "Erasing Internal EEprom", "Erasing External EEpro", "Counting EEprom", "Freeze"
};

static const char *INVERTER_STATE_LABEL[] = {
    "Stand By", "Checking Grid", "Run", "Bulk OV", "Out OC",
    "IGBT Sat", "Bulk UV", "Degauss Error", "No Parameters", "Bulk Low", "Grid OV",
    "Communication Error", "Degaussing", "Starting", "Bulk Cap Fail", "Leak Fail",
    "DcDc Fail", "Ileak Sensor Fail", "SelfTest: relay inverter", "SelfTest: wait for sensor test",
    "SelfTest: test relay DcDc + sensor", "SelfTest: relay inverter fail", "SelfTest timeout fail",
    "SelfTest: relay DcDc fail", "Self Test 1", "Waiting self test start", "Dc Injection", "Self Test 2",
    "Self Test 3", "Self Test 4", "Internal Error", "Internal Error", "", "", "", "", "", "", "", "", "Forbidden State",
    "Input UC", "Zero Power", "Grid Not Present", "Waiting Start", "MPPT", "Grid Fail", "Input OC"
};

static const char *ALARM_STATE_LABEL[][2] = {
    {"No Alarm", ""},
    {"Sun Low", "W001"},
    {"Input OC", "E001"},
    {"Input UV", "W002"},
    {"Input OV", "E002"},
    {"Sun Low", "W001"},
    {"No Parameters", "E003"},
    {"Bulk OV", "E004"},
    {"Comm.Error", "E005"},
    {"Output OC", "E006"},
    {"IGBT Sat", "E007"},
    {"Bulk UV", "W011"},
    {"Internal error", "E009"},
    {"Grid Fail", "W003"},
    {"Bulk Low", "E010"},
    {"Ramp Fail", "E011"},
    {"Dc/Dc Fail", "E012"},
    {"Wrong Mode", "E013"},
    {"Ground Fault", "---"},
    {"Over Temp.", "E014"},
    {"Bulk Cap Fail", "E015"},
    {"Inverter Fail", "E016"},
    {"Start Timeout", "E017"},
    {"Ground Fault", "E018"},
    {"Degauss error", "---"},
    {"Ileak sens.fail", "E019"},
    {"DcDc Fail", "E012"},
    {"Self Test Error 1", "E020"},
    {"Self Test Error 2", "E021"},
    {"Self Test Error 3", "E019"},
    {"Self Test Error 4", "E022"},
    {"DC inj error", "E023"},
    {"Grid OV", "W004"},
    {"Grid UV", "W005"},
    {"Grid OF", "W006"},
    {"Grid UF", "W007"},
    {"Z grid Hi", "W008"},
    {"Internal error", "E024"},
    {"Riso Low", "E025"},
    {"Vref Error", "E026"},
    {"Error Meas V", "E027"},
    {"Error Meas F", "E028"},
    {"Error Meas Z", "E029"},
    {"Error Meas Ileak", "E030"},
    {"Error Read V", "E031"},
    {"Error Read I", "E032"},
    {"Table fail", "W009"},
    {"Fan Fail", "W010"},
    {"UTH", "E033"},
    {"Interlock fail", "E034"},
    {"Remote Off", "E035"},
    {"Vout Avg errror", "E036"},
    {"Battery low", "W012"},
    {"Clk fail", "W013"},
    {"Input UC", "E037"},
    {"Zero Power", "W014"},
    {"Fan Stucked", "E038"},
    {"DC Switch Open", "E039"},
    {"Tras Switch Open", "E040"},
    {"AC Switch Open", "E041"},
    {"Bulk UV", "E042"},
    {"Autoexclusion", "E043"},
    {"Grid df/dt", "W015"},
    {"Den switch Open", "W016"},
    {"box fail", "W017"}
};

static const char *VERSION_PART1_LABEL[] = {
    "Aurora 2 kW indoor",      // 'i'
    "Aurora 2 kW outdoor",     // 'o'
    "Aurora 3.6 kW indoor",    // 'I'
    "Aurora 3.0-3.6 kW outdoor", // 'O'
    "Aurora 5.0 kW outdoor",   // '5'
    "Aurora 6 kW outdoor",     // '6'
    "3-phase interface (3G74)", // 'P'
    "Aurora 50kW module",      // 'C'
    "Aurora 4.2kW new",       // '4'
    "Aurora 3.6kW new",       // '3'
    "Aurora 3.3kW new",       // '2'
    "Aurora 3.0kW new",       // '1'
    "Aurora 12.0kW",          // 'D'
    "Aurora 10.0kW"           // 'X'
};

static const char *get_model_from_vr(const uint8_t vr_data[6]) {
    // VR[2] contains the version part1 character
    char version_char = (char)vr_data[2];
    switch (version_char) {
        case 'i': return VERSION_PART1_LABEL[0];
        case 'o': return VERSION_PART1_LABEL[1];
        case 'I': return VERSION_PART1_LABEL[2];
        case 'O': return VERSION_PART1_LABEL[3];
        case '5': return VERSION_PART1_LABEL[4];
        case '6': return VERSION_PART1_LABEL[5];
        case 'P': return VERSION_PART1_LABEL[6];
        case 'C': return VERSION_PART1_LABEL[7];
        case '4': return VERSION_PART1_LABEL[8];
        case '3': return VERSION_PART1_LABEL[9];
        case '2': return VERSION_PART1_LABEL[10];
        case '1': return VERSION_PART1_LABEL[11];
        case 'D': return VERSION_PART1_LABEL[12];
        case 'X': return VERSION_PART1_LABEL[13];
        default: return "Unknown Model";
    }
}

static int find_index(const char **names, int count, const char *key) {
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], key) == 0) return i;
    }
    return -1;
}

static uint32_t be_u32_from_reply(const uint8_t data[6]) {
    // bytes 2..5 form a big-endian 32-bit value
    return ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];
}

static esp_err_t txt_get_handler(httpd_req_t *req) {
    const uint8_t *cmds[10];
    const char *names[10];
    int count = 0;
    build_command_set(cmds, names, &count);
    aurora_reply_t out[10] = {0};
    // keys parameter unused by query_batch; pass names array casted
    if (!aurora_query_batch(out, (const uint8_t**)names, cmds, count, CONFIG_INVERTER_IP, CONFIG_INVERTER_PORT)) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "State: ERROR\n");
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/plain");
    // human-friendly summary
    int i_vr = find_index(names, count, "VR");
    int i_sn = find_index(names, count, "SN");
    int i_st = find_index(names, count, "ST");
    int i_cet = find_index(names, count, "CET");
    int i_ced = find_index(names, count, "CED");
    int i_dsp3 = find_index(names, count, "DSP3");
    int i_dsp23 = find_index(names, count, "DSP23");
    int i_dsp25 = find_index(names, count, "DSP25");
    int i_dsp26 = find_index(names, count, "DSP26");
    int i_dsp27 = find_index(names, count, "DSP27");

    char hex[16];
    char line[96];
    httpd_resp_sendstr_chunk(req, "State: OK\n");
    if (i_vr >= 0) {
        bytes_to_hex(out[i_vr].data, 6, hex, sizeof(hex));
        snprintf(line, sizeof(line), "versionRaw: %s\n", hex);
        httpd_resp_sendstr_chunk(req, line);
        const char *model = get_model_from_vr(out[i_vr].data);
        snprintf(line, sizeof(line), "model: %s\n", model);
        httpd_resp_sendstr_chunk(req, line);
    }
    if (i_sn >= 0) { char sn[8] = {0}; memcpy(sn, out[i_sn].data, 6); snprintf(line, sizeof(line), "serial: %s\n", sn); httpd_resp_sendstr_chunk(req, line);}
    if (i_st >= 0) { bytes_to_hex(out[i_st].data, 6, hex, sizeof(hex)); snprintf(line, sizeof(line), "globalStateRaw: %02X\n", out[i_st].data[2]); httpd_resp_sendstr_chunk(req, line); snprintf(line, sizeof(line), "inverterStateRaw: %02X\n", out[i_st].data[3]); httpd_resp_sendstr_chunk(req, line); snprintf(line, sizeof(line), "alarmRaw: %02X\n", out[i_st].data[5]); httpd_resp_sendstr_chunk(req, line);}
    if (i_st >= 0) {
        uint8_t gs = out[i_st].data[2];
        uint8_t is = out[i_st].data[3];
        uint8_t al = out[i_st].data[5];
        const char *gs_label = (gs < (sizeof(GLOBAL_STATE_LABEL)/sizeof(GLOBAL_STATE_LABEL[0]))) ? GLOBAL_STATE_LABEL[gs] : "";
        const char *is_label = (is < (sizeof(INVERTER_STATE_LABEL)/sizeof(INVERTER_STATE_LABEL[0]))) ? INVERTER_STATE_LABEL[is] : "";
        const char *al_label = (al < (sizeof(ALARM_STATE_LABEL)/sizeof(ALARM_STATE_LABEL[0]))) ? ALARM_STATE_LABEL[al][0] : "";
        const char *al_code  = (al < (sizeof(ALARM_STATE_LABEL)/sizeof(ALARM_STATE_LABEL[0]))) ? ALARM_STATE_LABEL[al][1] : "";
        snprintf(line, sizeof(line), "globalState: %s\n", gs_label); httpd_resp_sendstr_chunk(req, line);
        snprintf(line, sizeof(line), "inverterState: %s\n", is_label); httpd_resp_sendstr_chunk(req, line);
        snprintf(line, sizeof(line), "alarm: %s\n", al_label); httpd_resp_sendstr_chunk(req, line);
        if (al_code[0]) { snprintf(line, sizeof(line), "alarmCode: %s\n", al_code); httpd_resp_sendstr_chunk(req, line); }
    }
    if (i_cet >= 0) { double kwh = (double)be_u32_from_reply(out[i_cet].data) / 1000.0; snprintf(line, sizeof(line), "totalKWh: %.2f\n", kwh); httpd_resp_sendstr_chunk(req, line);}
    if (i_ced >= 0) { double kwh = (double)be_u32_from_reply(out[i_ced].data) / 1000.0; snprintf(line, sizeof(line), "todayKWh: %.2f\n", kwh); httpd_resp_sendstr_chunk(req, line);}
    if (i_dsp3 >= 0) { double pw = aurora_dsp_value(out[i_dsp3].data); snprintf(line, sizeof(line), "powerOutW: %.2f\n", pw); httpd_resp_sendstr_chunk(req, line);}
    if (i_dsp23 >= 0 && i_dsp25 >= 0 && i_dsp26 >= 0 && i_dsp27 >= 0) {
        double pvin = aurora_dsp_value(out[i_dsp23].data) * aurora_dsp_value(out[i_dsp25].data) + aurora_dsp_value(out[i_dsp26].data) * aurora_dsp_value(out[i_dsp27].data);
        snprintf(line, sizeof(line), "powerInW: %.2f\n", pvin); httpd_resp_sendstr_chunk(req, line);
    }
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t json_get_handler(httpd_req_t *req) {
    const uint8_t *cmds[10];
    const char *names[10];
    int count = 0;
    build_command_set(cmds, names, &count);
    aurora_reply_t out[10] = {0};
    if (!aurora_query_batch(out, (const uint8_t**)names, cmds, count, CONFIG_INVERTER_IP, CONFIG_INVERTER_PORT)) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"state\":\"ERROR\"}");
        return ESP_OK;
    }
    httpd_resp_set_type(req, "application/json");
    // build human-friendly JSON similar to Go output
    int i_vr = find_index(names, count, "VR");
    int i_sn = find_index(names, count, "SN");
    int i_st = find_index(names, count, "ST");
    int i_cet = find_index(names, count, "CET");
    int i_ced = find_index(names, count, "CED");
    int i_dsp3 = find_index(names, count, "DSP3");
    int i_dsp23 = find_index(names, count, "DSP23");
    int i_dsp25 = find_index(names, count, "DSP25");
    int i_dsp26 = find_index(names, count, "DSP26");
    int i_dsp27 = find_index(names, count, "DSP27");

    char vr_hex[16] = {0}; if (i_vr>=0) bytes_to_hex(out[i_vr].data, 6, vr_hex, sizeof(vr_hex));
    const char *model = (i_vr>=0) ? get_model_from_vr(out[i_vr].data) : "";
    char sn[8] = {0}; if (i_sn>=0) memcpy(sn, out[i_sn].data, 6);
    char buf[160];
    httpd_resp_sendstr_chunk(req, "{\"state\":\"OK\",");
    // versionRaw & model & serial
    snprintf(buf, sizeof(buf), "\"versionRaw\":\"%s\",\"model\":\"%s\",\"serial\":\"%s\",", vr_hex, model, sn);
    httpd_resp_sendstr_chunk(req, buf);
    // global/inverter/alarm raw codes
    if (i_st>=0) {
        uint8_t gs = out[i_st].data[2];
        uint8_t is = out[i_st].data[3];
        uint8_t al = out[i_st].data[5];
        const char *gs_label = (gs < (sizeof(GLOBAL_STATE_LABEL)/sizeof(GLOBAL_STATE_LABEL[0]))) ? GLOBAL_STATE_LABEL[gs] : "";
        const char *is_label = (is < (sizeof(INVERTER_STATE_LABEL)/sizeof(INVERTER_STATE_LABEL[0]))) ? INVERTER_STATE_LABEL[is] : "";
        const char *al_label = (al < (sizeof(ALARM_STATE_LABEL)/sizeof(ALARM_STATE_LABEL[0]))) ? ALARM_STATE_LABEL[al][0] : "";
        const char *al_code  = (al < (sizeof(ALARM_STATE_LABEL)/sizeof(ALARM_STATE_LABEL[0]))) ? ALARM_STATE_LABEL[al][1] : "";
        snprintf(buf, sizeof(buf), "\"globalStateRaw\":%u,\"inverterStateRaw\":%u,\"alarmRaw\":%u,", gs, is, al);
        httpd_resp_sendstr_chunk(req, buf);
        // labels
        httpd_resp_sendstr_chunk(req, "\"global\":\""); httpd_resp_sendstr_chunk(req, gs_label); httpd_resp_sendstr_chunk(req, "\",");
        httpd_resp_sendstr_chunk(req, "\"inverter\":\""); httpd_resp_sendstr_chunk(req, is_label); httpd_resp_sendstr_chunk(req, "\",");
        httpd_resp_sendstr_chunk(req, "\"alarm\":\""); httpd_resp_sendstr_chunk(req, al_label); httpd_resp_sendstr_chunk(req, "\",");
        httpd_resp_sendstr_chunk(req, "\"alarmCode\":\""); httpd_resp_sendstr_chunk(req, al_code); httpd_resp_sendstr_chunk(req, "\",");
    }
    // energy
    if (i_cet>=0) { double kwh = (double)be_u32_from_reply(out[i_cet].data)/1000.0; snprintf(buf,sizeof(buf),"\"totalKWh\":%.2f,", kwh); httpd_resp_sendstr_chunk(req, buf);}
    if (i_ced>=0) { double kwh = (double)be_u32_from_reply(out[i_ced].data)/1000.0; snprintf(buf,sizeof(buf),"\"todayKWh\":%.2f,", kwh); httpd_resp_sendstr_chunk(req, buf);}
    // power
    if (i_dsp3>=0) { double pw = aurora_dsp_value(out[i_dsp3].data); snprintf(buf,sizeof(buf),"\"powerOutW\":%.2f,", pw); httpd_resp_sendstr_chunk(req, buf);}
    if (i_dsp23>=0 && i_dsp25>=0 && i_dsp26>=0 && i_dsp27>=0) {
        double pvin = aurora_dsp_value(out[i_dsp23].data) * aurora_dsp_value(out[i_dsp25].data) + aurora_dsp_value(out[i_dsp26].data) * aurora_dsp_value(out[i_dsp27].data);
        snprintf(buf, sizeof(buf), "\"powerInW\":%.2f", pvin);
        httpd_resp_sendstr_chunk(req, buf);
    }
    httpd_resp_sendstr_chunk(req, "}");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t xml_get_handler(httpd_req_t *req) {
    const uint8_t *cmds[10];
    const char *names[10];
    int count = 0;
    build_command_set(cmds, names, &count);
    aurora_reply_t out[10] = {0};
    httpd_resp_set_type(req, "application/xml");
    if (!aurora_query_batch(out, (const uint8_t**)names, cmds, count, CONFIG_INVERTER_IP, CONFIG_INVERTER_PORT)) {
        httpd_resp_sendstr(req, "<inverter><state>ERROR</state></inverter>");
        return ESP_OK;
    }
    // compose human-friendly XML
    int i_vr = find_index(names, count, "VR");
    int i_sn = find_index(names, count, "SN");
    int i_st = find_index(names, count, "ST");
    int i_cet = find_index(names, count, "CET");
    int i_ced = find_index(names, count, "CED");
    int i_dsp3 = find_index(names, count, "DSP3");
    int i_dsp23 = find_index(names, count, "DSP23");
    int i_dsp25 = find_index(names, count, "DSP25");
    int i_dsp26 = find_index(names, count, "DSP26");
    int i_dsp27 = find_index(names, count, "DSP27");

    httpd_resp_sendstr_chunk(req, "<inverter><state>OK</state>");
    if (i_vr>=0){
        char vhex[16]={0}; bytes_to_hex(out[i_vr].data,6,vhex,sizeof(vhex));
        char p[128];
        snprintf(p,sizeof(p),"<versionRaw>%s</versionRaw>", vhex); httpd_resp_sendstr_chunk(req,p);
        const char *model = get_model_from_vr(out[i_vr].data);
        snprintf(p,sizeof(p),"<model>%s</model>", model); httpd_resp_sendstr_chunk(req,p);
    }
    if (i_sn>=0){ char sn[8]={0}; memcpy(sn,out[i_sn].data,6); char p[64]; snprintf(p,sizeof(p),"<serial>%s</serial>", sn); httpd_resp_sendstr_chunk(req,p);}
    if (i_st>=0){
        uint8_t gs=out[i_st].data[2], is=out[i_st].data[3], al=out[i_st].data[5];
        const char *gs_label = (gs < (sizeof(GLOBAL_STATE_LABEL)/sizeof(GLOBAL_STATE_LABEL[0]))) ? GLOBAL_STATE_LABEL[gs] : "";
        const char *is_label = (is < (sizeof(INVERTER_STATE_LABEL)/sizeof(INVERTER_STATE_LABEL[0]))) ? INVERTER_STATE_LABEL[is] : "";
        const char *al_label = (al < (sizeof(ALARM_STATE_LABEL)/sizeof(ALARM_STATE_LABEL[0]))) ? ALARM_STATE_LABEL[al][0] : "";
        const char *al_code  = (al < (sizeof(ALARM_STATE_LABEL)/sizeof(ALARM_STATE_LABEL[0]))) ? ALARM_STATE_LABEL[al][1] : "";
        char p[256];
        snprintf(p,sizeof(p),"<globalStateRaw>%u</globalStateRaw><inverterStateRaw>%u</inverterStateRaw><alarmRaw>%u</alarmRaw>", gs, is, al); httpd_resp_sendstr_chunk(req,p);
        snprintf(p,sizeof(p),"<global>%s</global><inverter>%s</inverter><alarm>%s</alarm>", gs_label, is_label, al_label); httpd_resp_sendstr_chunk(req,p);
        if (al_code[0]) { snprintf(p,sizeof(p),"<alarmCode>%s</alarmCode>", al_code); httpd_resp_sendstr_chunk(req,p);}
    }
    if (i_cet>=0){ double kwh=(double)be_u32_from_reply(out[i_cet].data)/1000.0; char p[64]; snprintf(p,sizeof(p),"<totalKWh>%.2f</totalKWh>", kwh); httpd_resp_sendstr_chunk(req,p);}
    if (i_ced>=0){ double kwh=(double)be_u32_from_reply(out[i_ced].data)/1000.0; char p[64]; snprintf(p,sizeof(p),"<todayKWh>%.2f</todayKWh>", kwh); httpd_resp_sendstr_chunk(req,p);}
    if (i_dsp3>=0){ double pw=aurora_dsp_value(out[i_dsp3].data); char p[64]; snprintf(p,sizeof(p),"<powerOutW>%.2f</powerOutW>", pw); httpd_resp_sendstr_chunk(req,p);}
    if (i_dsp23>=0 && i_dsp25>=0 && i_dsp26>=0 && i_dsp27>=0){ double pvin=aurora_dsp_value(out[i_dsp23].data)*aurora_dsp_value(out[i_dsp25].data)+aurora_dsp_value(out[i_dsp26].data)*aurora_dsp_value(out[i_dsp27].data); char p[64]; snprintf(p,sizeof(p),"<powerInW>%.2f</powerInW>", pvin); httpd_resp_sendstr_chunk(req,p);}
    httpd_resp_sendstr_chunk(req, "</inverter>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t http_start_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_HTTP_SERVER_PORT;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        return ESP_FAIL;
    }
    httpd_uri_t health = {
        .uri = "/health/",
        .method = HTTP_GET,
        .handler = health_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &health);
    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = txt_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &root);
    httpd_uri_t json = { .uri = "/json/", .method = HTTP_GET, .handler = json_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &json);
    httpd_uri_t xml = { .uri = "/xml/", .method = HTTP_GET, .handler = xml_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &xml);
    return ESP_OK;
}
