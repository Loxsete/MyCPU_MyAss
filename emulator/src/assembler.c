#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>

#define MAX_LINE 512
#define MAX_LABELS 2048
#define MAX_DATA 4096
#define MAX_CODE_WORDS 65536
#define FORBID_LO 0xFF00
#define FORBID_HI 0xFFFF

// ---------- utils ----------
static char* rstrip(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (unsigned char)s[n-1] <= 32) { s[--n] = 0; }
    return s;
}

static char* lskip(char* s) {
    while (*s && (unsigned char)*s <= 32) s++;
    return s;
}

static void lower(char* s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

static int is_ident_char(int c) { return isalnum(c) || c == '_' || c == '.'; }

// ---------- error handling ----------
typedef struct { int line; char* msg; } Err;
static Err* errs = NULL;
static size_t nerrs = 0;

static void add_err(int line, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char buf[512]; vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    errs = (Err*)realloc(errs, (nerrs + 1) * sizeof(Err));
    errs[nerrs].line = line; errs[nerrs].msg = strdup(buf); nerrs++;
}

static int has_errors(void) { return (int)nerrs; }

static void flush_errors(void) {
    for (size_t i = 0; i < nerrs; i++) { fprintf(stderr, "Error [line %d]: %s\n", errs[i].line, errs[i].msg); free(errs[i].msg); }
    free(errs); errs = NULL; nerrs = 0;
}

// ---------- symbols / data ----------
typedef struct { char* name; uint32_t addr; } Label;
static Label labels[MAX_LABELS];
static int nlabels = 0;

static int add_label(const char* name, uint32_t addr) {
    if (nlabels >= MAX_LABELS) return -1;
    labels[nlabels].name = strdup(name);
    labels[nlabels].addr = addr;
    return nlabels++;
}

static int find_label(const char* name) {
    for (int i = 0; i < nlabels; i++) if (strcmp(labels[i].name, name) == 0) return i;
    return -1;
}

typedef enum { DT_DB = 1, DT_DW = 2, DT_DD = 4 } DType;
typedef struct { char* name; DType type; uint32_t addr; size_t count; uint8_t* raw; } DataItem;
static DataItem data_items[MAX_DATA];
static int ndata = 0;
static uint32_t data_base = 0x0100; // Default data segment base address

// ---------- registers ----------
typedef struct { const char* name; uint8_t id; } Reg;
static Reg regs[] = {
    {"ax", 0}, {"bx", 1}, {"cx", 2}, {"dx", 3}, {"sp", 4}, {"bp", 5}, {"ip", 6}
};

static int reg_id(const char* s) {
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++)
        if (strcasecmp(s, regs[i].name) == 0) return regs[i].id;
    return -1;
}

// ---------- opcodes & aliases ----------
typedef struct { const char* mnem; uint8_t op; int argc; } Op;
static Op ops[] = {
    {"nop", 0, 0}, {"hlt", 1, 0},
    {"mov", 2, 2}, {"add", 3, 2}, {"sub", 4, 2}, {"mul", 5, 2}, {"div", 6, 2}, {"mod", 7, 2},
    {"and", 8, 2}, {"or", 9, 2}, {"xor", 10, 2}, {"not", 11, 1}, {"neg", 12, 1},
    {"shl", 13, 2}, {"shr", 14, 2}, {"cmp", 15, 2},
    {"push", 16, 1}, {"pop", 17, 1}, {"pusha", 18, 0}, {"popa", 19, 0},
    {"int", 20, 1},
    {"jmp", 21, 1}, {"call", 22, 1}, {"ret", 23, 0},
    {"jz", 24, 1}, {"jnz", 25, 1}, {"jg", 26, 1}, {"jl", 27, 1}
};

typedef struct { const char* alias; const char* canon; } Alias;
static Alias aliases[] = { {"je", "jz"}, {"jne", "jnz"} };

static int find_op(const char* mnem, Op* out) {
    char buf[32]; strncpy(buf, mnem, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0; lower(buf);
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++)
        if (strcmp(buf, aliases[i].alias) == 0) { strncpy(buf, aliases[i].canon, sizeof(buf)); }
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        if (strcmp(buf, ops[i].mnem) == 0) { if (out) *out = ops[i]; return 1; }
    }
    return 0;
}

// ---------- encoding ----------
// word0: [5b opcode][3b r1][3b r2][5b mode]
// mode: 0=none, 1=reg, 2=reg_reg, 3=reg_imm16, 4=reg_mem16, 5=imm16 only, 6=mem16 only
typedef struct { uint16_t words[2]; int nwords; } Enc;

static Enc enc_rr(uint8_t op, uint8_t r1, uint8_t r2) {
    Enc e = {{0}, 1};
    e.words[0] = (uint16_t)((op << 11) | ((r1 & 7) << 8) | ((r2 & 7) << 5) | 2);
    return e;
}

static Enc enc_r(uint8_t op, uint8_t r1) {
    Enc e = {{0}, 1};
    e.words[0] = (uint16_t)((op << 11) | ((r1 & 7) << 8) | 1);
    return e;
}

static Enc enc_r_imm(uint8_t op, uint8_t r1, uint16_t imm) {
    Enc e = {{0}, 2};
    e.words[0] = (uint16_t)((op << 11) | ((r1 & 7) << 8) | 3);
    e.words[1] = imm;
    return e;
}

static Enc enc_r_mem(uint8_t op, uint8_t r1, uint16_t addr) {
    Enc e = {{0}, 2};
    e.words[0] = (uint16_t)((op << 11) | ((r1 & 7) << 8) | 4);
    e.words[1] = addr;
    return e;
}

static Enc enc_imm(uint8_t op, uint16_t imm) {
    Enc e = {{0}, 2};
    e.words[0] = (uint16_t)((op << 11) | 5);
    e.words[1] = imm;
    return e;
}

static Enc enc_none(uint8_t op) {
    Enc e = {{0}, 1};
    e.words[0] = (uint16_t)(op << 11);
    return e;
}

// ---------- number parsing ----------
static int parse_number(const char* s, uint32_t* out) {
    char* end; if (!s || !*s) return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        *out = (uint32_t)strtoul(s + 2, &end, 16); return *end == 0;
    } else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        uint32_t v = 0; for (const char* p = s + 2; *p; p++) { if (*p != '0' && *p != '1') return 0; v = (v << 1) | (*p - '0'); }
        *out = v; return 1;
    } else {
        *out = (uint32_t)strtoul(s, &end, 10); return *end == 0;
    }
}

// ---------- tokenizer for args ----------
static void trim_comm(char* s) { char* sc = strchr(s, ';'); if (sc) *sc = 0; rstrip(s); }

static void split_args(char* s, char* a1, char* a2) {
    a1[0] = a2[0] = 0;
    char* comma = strchr(s, ',');
    if (!comma) { strncpy(a1, lskip(s), 128 - 1); rstrip(a1); return; }
    *comma = 0;
    strncpy(a1, lskip(s), 128 - 1); rstrip(a1);
    strncpy(a2, lskip(comma + 1), 128 - 1); rstrip(a2);
}

static void clean_ident(char* s) {
    char* p = lskip(s); memmove(s, p, strlen(p) + 1); rstrip(s);
}

// ---------- .data parsing ----------
static int emit_scalar(DType t, uint32_t v, uint8_t** out, size_t* len, int line) {
    if (t == DT_DB && v > 0xFF) {
        add_err(line, "value 0x%X too large for db (max 0xFF)", v);
        return 0;
    }
    if (t == DT_DW && v > 0xFFFF) {
        add_err(line, "value 0x%X too large for dw (max 0xFFFF)", v);
        return 0;
    }
    if (t == DT_DB) {
        *out = (uint8_t*)realloc(*out, *len + 1);
        (*out)[(*len)++] = (uint8_t)(v & 0xFF);
        return 1;
    }
    if (t == DT_DW) {
        *out = (uint8_t*)realloc(*out, *len + 2);
        (*out)[(*len)++] = (uint8_t)(v & 0xFF);
        (*out)[(*len)++] = (uint8_t)((v >> 8) & 0xFF);
        return 1;
    }
    if (t == DT_DD) {
        *out = (uint8_t*)realloc(*out, *len + 4);
        (*out)[(*len)++] = (uint8_t)(v & 0xFF);
        (*out)[(*len)++] = (uint8_t)((v >> 8) & 0xFF);
        (*out)[(*len)++] = (uint8_t)((v >> 16) & 0xFF);
        (*out)[(*len)++] = (uint8_t)((v >> 24) & 0xFF);
        return 1;
    }
    add_err(line, "invalid data type");
    return 0;
}

static int parse_string(const char* p, uint8_t** raw, size_t* rawlen, int line) {
    p++; // Skip opening quote
    const char* end = strchr(p, '"');
    if (!end) {
        add_err(line, "unterminated string");
        return 0;
    }
    size_t len = (size_t)(end - p);
    *raw = (uint8_t*)realloc(*raw, *rawlen + len + 1);
    memcpy(*raw + *rawlen, p, len);
    *rawlen += len;
    (*raw)[*rawlen] = 0; // Zero-terminated
    *rawlen += 1;
    return 1;
}

static int parse_data_values(int line, DType t, const char* rhs, uint8_t** raw, size_t* rawlen) {
    const char* p = lskip((char*)rhs);
    if (*p == '"') {
        if (t != DT_DB) {
            add_err(line, "strings are only allowed with db");
            return 0;
        }
        return parse_string(p, raw, rawlen, line);
    }
    char buf[MAX_LINE];
    strncpy(buf, p, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* tok = strtok(buf, ",");
    if (!tok) {
        add_err(line, "empty data list");
        return 0;
    }
    while (tok) {
        char tmp[128];
        strncpy(tmp, lskip(tok), sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        rstrip(tmp);
        uint32_t v;
        if (!parse_number(tmp, &v)) {
            add_err(line, "invalid number in data: '%s'", tmp);
            return 0;
        }
        if (!emit_scalar(t, v, raw, rawlen, line)) {
            return 0;
        }
        tok = strtok(NULL, ",");
    }
    return 1;
}

static int add_data(int line, const char* name, DType t, const char* rhs) {
    if (ndata >= MAX_DATA) {
        add_err(line, "too many data items");
        return 0;
    }
    for (int i = 0; i < ndata; i++) {
        if (strcmp(data_items[i].name, name) == 0) {
            add_err(line, "duplicate data name '%s'", name);
            return 0;
        }
    }
    // Align dw to 2 bytes, dd to 4 bytes
    if (t == DT_DW && (data_base & 1)) {
        data_base = (data_base + 1) & ~1;
    } else if (t == DT_DD && (data_base & 3)) {
        data_base = (data_base + 3) & ~3;
    }
    uint8_t* raw = NULL;
    size_t len = 0;
    if (!parse_data_values(line, t, rhs, &raw, &len)) {
        free(raw);
        return 0;
    }
    data_items[ndata].name = strdup(name[0] ? name : "");
    data_items[ndata].type = t;
    data_items[ndata].addr = data_base;
    data_items[ndata].count = len;
    data_items[ndata].raw = raw;
    ndata++;
    data_base += (uint32_t)len;
    if (name[0]) {
        add_label(name, data_items[ndata - 1].addr);
    }
    return 1;
}

// ---------- code buffer ----------
static uint16_t code[MAX_CODE_WORDS];
static size_t code_words = 0;
static uint32_t org_address = 0;

static void emit_enc(Enc e) {
    for (int i = 0; i < e.nwords; i++) {
        if (code_words >= MAX_CODE_WORDS) { fprintf(stderr, "FATAL: code buffer overflow\n"); exit(2); }
        code[code_words++] = e.words[i];
    }
}

static uint32_t cur_ip(void) { return org_address + (uint32_t)(code_words * 2); }

// ---------- first pass: labels + data items + .org ----------
static void handle_org(int line, const char* rhs) {
    uint32_t v;
    if (!parse_number(rhs, &v)) { add_err(line, ".org: bad number '%s'", rhs); return; }
    if (v >= FORBID_LO && v <= FORBID_HI) { add_err(line, ".org 0x%04X forbidden (BIOS/MMIO)", (unsigned)v); return; }
    org_address = (uint32_t)v;
}

static void first_pass(FILE* in) {
    char linebuf[MAX_LINE];
    int line = 0;
    while (fgets(linebuf, sizeof(linebuf), in)) {
        line++;
        rstrip(linebuf);
        char* s = lskip(linebuf);
        trim_comm(s);
        if (!*s) continue;

        // Handle .org directive
        char low[MAX_LINE];
        strncpy(low, s, sizeof(low) - 1);
        low[sizeof(low) - 1] = 0;
        lower(low);
        if (strncmp(low, ".org", 4) == 0) {
            char* p = s + 4;
            p = lskip(p);
            if (!*p) { add_err(line, ".org needs value"); continue; }
            handle_org(line, p);
            continue;
        }

        // Check for label or data directive (e.g., "name: db|dw|dd values")
        char* col = strchr(s, ':');
        if (col) {
            size_t L = (size_t)(col - s);
            if (L == 0) { add_err(line, "empty label"); continue; }
            char name[256];
            strncpy(name, s, L);
            name[L] = 0;
            rstrip(name);

            s = lskip(col + 1);
            trim_comm(s);
            if (!*s) {
                // Register as code label if no data type follows
                if (find_label(name) >= 0) { add_err(line, "duplicate label '%s'", name); }
                else { add_label(name, cur_ip()); }
                continue;
            }

            strncpy(low, s, sizeof(low) - 1);
            low[sizeof(low) - 1] = 0;
            lower(low);

            // Check for data directive after label (e.g., "msg: db values")
            char type[8] = {0};
            int i = 0;
            while (*s && isalpha((unsigned char)*s) && i < 7) type[i++] = *s++;
            type[i] = 0;
            s = lskip(s);
            DType t = 0;
            if (strcasecmp(type, "db") == 0) t = DT_DB;
            else if (strcasecmp(type, "dw") == 0) t = DT_DW;
            else if (strcasecmp(type, "dd") == 0) t = DT_DD;
            if (t != 0) {
                if (!*s) { add_err(line, "data '%s' has no values", name); continue; }
                add_data(line, name, t, s);
                continue;
            }

            // If not a data directive, register as code label
            if (find_label(name) >= 0) { add_err(line, "duplicate label '%s'", name); }
            else { add_label(name, cur_ip()); }
            continue;
        }

        // Handle .data directive (e.g., ".data name: db|dw|dd values")
        if (strncmp(low, ".data", 5) == 0) {
            char* p = s + 5;
            p = lskip(p);
            char name[256] = {0};
            int i = 0;
            while (*p && is_ident_char(*p)) name[i++] = *p++;
            name[i] = 0;
            p = lskip(p);
            if (*p != ':') { add_err(line, "data syntax: .data name: type values or name: type values"); continue; }
            p++;
            p = lskip(p);
            char type[8] = {0};
            i = 0;
            while (*p && isalpha((unsigned char)*s) && i < 7) type[i++] = *p++;
            type[i] = 0;
            p = lskip(p);
            DType t = 0;
            if (strcasecmp(type, "db") == 0) t = DT_DB;
            else if (strcasecmp(type, "dw") == 0) t = DT_DW;
            else if (strcasecmp(type, "dd") == 0) t = DT_DD;
            else { add_err(line, "unknown data type '%s'", type); continue; }
            if (!*p) { add_err(line, "data '%s' has no values", name); continue; }
            add_data(line, name, t, p);
            continue;
        }
    }
}

// ---------- eval operand (label/data/number/reg) ----------
typedef enum { OPK_NONE, OPK_REG, OPK_IMM, OPK_MEM } OpKind;
typedef struct { OpKind k; int reg; uint32_t val; } Opr;

static Opr parse_operand(const char* s) {
    Opr o = {OPK_NONE, -1, 0};
    if (!s || !*s) return o;
    char tmp[256]; strncpy(tmp, s, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = 0; clean_ident(tmp);
    int r = reg_id(tmp);
    if (r >= 0) { o.k = OPK_REG; o.reg = r; return o; }
    uint32_t v;
    if (parse_number(tmp, &v)) { o.k = OPK_IMM; o.val = v; return o; }
    int li = find_label(tmp);
    if (li >= 0) { o.k = OPK_MEM; o.val = labels[li].addr; return o; }
    for (int i = 0; i < ndata; i++) {
        if (strcmp(tmp, data_items[i].name) == 0) { o.k = OPK_MEM; o.val = data_items[i].addr; return o; }
    }
    return o;
}

// ---------- second pass: encode ----------
static void second_pass(FILE* in, const char* outpath) {
    rewind(in);
    char linebuf[MAX_LINE];
    int line = 0;
    while (fgets(linebuf, sizeof(linebuf), in)) {
        line++;
        rstrip(linebuf);
        char* s = lskip(linebuf);
        trim_comm(s);
        if (!*s) continue;

        char low[MAX_LINE];
        strncpy(low, s, sizeof(low) - 1);
        low[sizeof(low) - 1] = 0;
        lower(low);
        if (strncmp(low, ".org", 4) == 0 || strncmp(low, ".data", 5) == 0) {
            continue;
        }

        char* col = strchr(s, ':');
        if (col) {
            size_t L = (size_t)(col - s);
            char name[256];
            strncpy(name, s, L);
            name[L] = 0;
            rstrip(name);
            s = lskip(col + 1);
            trim_comm(s);
            if (!*s) continue;
            strncpy(low, s, sizeof(low) - 1);
            low[sizeof(low) - 1] = 0;
            lower(low);

            // Skip if it's a data directive
            char type[8] = {0};
            int i = 0;
            while (*s && isalpha((unsigned char)*s) && i < 7) type[i++] = *s++;
            type[i] = 0;
            if (strcasecmp(type, "db") == 0 || strcasecmp(type, "dw") == 0 || strcasecmp(type, "dd") == 0) {
                continue;
            }
        }

        char mnem[64] = {0};
        int i = 0;
        while (*s && !isspace((unsigned char)*s) && *s != ':') mnem[i++] = *s++;
        mnem[i] = 0;
        s = lskip(s);
        Op op;
        if (!find_op(mnem, &op)) { add_err(line, "unknown mnemonic '%s'", mnem); continue; }

        char a1[128] = {0}, a2[128] = {0};
        if (op.argc == 2) {
            if (!*s) { add_err(line, "%s needs 2 args", mnem); continue; }
            split_args(s, a1, a2);
            if (!*a1 || !*a2) { add_err(line, "%s needs 2 args", mnem); continue; }
        } else if (op.argc == 1) {
            if (!*s) { add_err(line, "%s needs 1 arg", mnem); continue; }
            split_args(s, a1, a2);
            if (!*a1 || *a2) { if (!*a1) add_err(line, "%s needs 1 arg", mnem); else add_err(line, "%s takes 1 arg (got more)", mnem); continue; }
        } else {
            if (*s) add_err(line, "%s takes no args", mnem);
        }

        Opr o1 = parse_operand(a1);
        Opr o2 = parse_operand(a2);

        if (op.argc == 0) {
            emit_enc(enc_none(op.op));
            continue;
        }
        if (op.argc == 1) {
            if (o1.k == OPK_REG) emit_enc(enc_r(op.op, (uint8_t)o1.reg));
            else if (o1.k == OPK_IMM || o1.k == OPK_MEM) emit_enc(enc_imm(op.op, (uint16_t)(o1.val & 0xFFFF)));
            else add_err(line, "bad operand");
            continue;
        }
        if (o1.k == OPK_REG && o2.k == OPK_REG) {
            emit_enc(enc_rr(op.op, (uint8_t)o1.reg, (uint8_t)o2.reg));
        } else if (o1.k == OPK_REG && o2.k == OPK_IMM) {
            emit_enc(enc_r_imm(op.op, (uint8_t)o1.reg, (uint16_t)(o2.val & 0xFFFF)));
        } else if (o1.k == OPK_REG && o2.k == OPK_MEM) {
            emit_enc(enc_r_mem(op.op, (uint8_t)o1.reg, (uint16_t)(o2.val & 0xFFFF)));
        } else {
            add_err(line, "unsupported operand combo '%s %s,%s'", mnem, a1, a2);
        }
    }
    if (has_errors()) {
        flush_errors();
        fprintf(stderr, "Compilation failed. No output.\n");
        return;
    }

    FILE* out = fopen(outpath, "wb");
    if (!out) { fprintf(stderr, "Cannot open %s for write\n", outpath); exit(1); }
    if (org_address > 0) fseek(out, (long)org_address, SEEK_SET);
    fwrite(code, sizeof(uint16_t), code_words, out);
    for (int i = 0; i < ndata; i++) {
        fseek(out, (long)data_items[i].addr, SEEK_SET);
        fwrite(data_items[i].raw, 1, data_items[i].count, out);
    }
    fclose(out);
    printf("Compiled %zu word(s) to %s (org=0x%04X, data_end=0x%04X)\n",
           code_words, outpath, (unsigned)org_address, (unsigned)data_base);
}

// ---------- main ----------
int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.asm> <output.bin>\n", argv[0]);
        return 1;
    }
    FILE* in = fopen(argv[1], "r");
    if (!in) { fprintf(stderr, "Cannot open %s\n", argv[1]); return 1; }
    first_pass(in);
    second_pass(in, argv[2]);
    for (int i = 0; i < nlabels; i++) free(labels[i].name);
    for (int i = 0; i < ndata; i++) { free(data_items[i].name); free(data_items[i].raw); }
    fclose(in);
    return has_errors() ? 2 : 0;
}

