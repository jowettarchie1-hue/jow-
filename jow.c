#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_TOK     128
#define MAX_TOK_LEN 256
#define MAX_LINE    2048
#define MAX_VARS    256
#define MAX_VAR_LEN 128
#define MAX_VAL_LEN 512
#define MAX_SEGS    32

/* ── variables ──────────────────────────────────────────────────────────── */
static char var_names[MAX_VARS][MAX_VAR_LEN];
static char var_vals [MAX_VARS][MAX_VAL_LEN];
static int  num_vars = 0;
static char vars_path[512];

static const char *get_var(const char *name) {
    for (int i = 0; i < num_vars; i++)
        if (strcasecmp(var_names[i], name) == 0) return var_vals[i];
    return NULL;
}
static void set_var(const char *name, const char *val) {
    for (int i = 0; i < num_vars; i++) {
        if (strcasecmp(var_names[i], name) == 0) {
            strncpy(var_vals[i], val, MAX_VAL_LEN-1); return;
        }
    }
    if (num_vars < MAX_VARS) {
        strncpy(var_names[num_vars], name, MAX_VAR_LEN-1);
        strncpy(var_vals [num_vars], val,  MAX_VAL_LEN-1);
        num_vars++;
    }
}
static void load_vars(void) {
    FILE *f = fopen(vars_path, "r"); if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = '\0'; eq[1 + strcspn(eq+1, "\n")] = '\0';
        set_var(line, eq+1);
    }
    fclose(f);
}
static void save_vars(void) {
    FILE *f = fopen(vars_path, "w"); if (!f) return;
    for (int i = 0; i < num_vars; i++)
        fprintf(f, "%s=%s\n", var_names[i], var_vals[i]);
    fclose(f);
}

/* ── word numbers ────────────────────────────────────────────────────────── */
static const struct { const char *w; int v; } WNUMS[] = {
    {"zero",0},{"one",1},{"two",2},{"three",3},{"four",4},{"five",5},
    {"six",6},{"seven",7},{"eight",8},{"nine",9},{"ten",10},
    {"eleven",11},{"twelve",12},{"thirteen",13},{"fourteen",14},{"fifteen",15},
    {"sixteen",16},{"seventeen",17},{"eighteen",18},{"nineteen",19},{"twenty",20},
    {"thirty",30},{"forty",40},{"fifty",50},{"hundred",100},{"thousand",1000},
    {NULL,0}
};
static int word_to_num(const char *w, int *out) {
    for (int i = 0; WNUMS[i].w; i++)
        if (strcasecmp(w, WNUMS[i].w) == 0) { *out = WNUMS[i].v; return 1; }
    return 0;
}

/* ── evaluate: resolve one token to its string value ────────────────────── */
static void evaluate(const char *tok, char *out, size_t sz) {
    const char *v = get_var(tok);
    if (v) { strncpy(out, v, sz-1); return; }
    int n; if (word_to_num(tok, &n)) { snprintf(out, sz, "%d", n); return; }
    strncpy(out, tok, sz-1); out[sz-1] = '\0';
}

/* ── tokenize ────────────────────────────────────────────────────────────── */
static int tokenize(const char *line, char toks[][MAX_TOK_LEN], int maxn) {
    int n = 0; const char *p = line;
    while (*p && n < maxn) {
        while (*p==' '||*p=='\t') p++;
        if (!*p) break;
        int i = 0;
        while (*p && *p!=' ' && *p!='\t' && i < MAX_TOK_LEN-1) toks[n][i++] = *p++;
        toks[n][i] = '\0'; if (i) n++;
    }
    return n;
}

/* ── filler word stripping ───────────────────────────────────────────────── */
static const char *FILLER[] = {
    "the","a","an","my","your","our","its","that","this","some",NULL
};
static int is_filler(const char *w) {
    for (int i = 0; FILLER[i]; i++) if (strcasecmp(w, FILLER[i])==0) return 1;
    return 0;
}
static int strip_filler(char toks[][MAX_TOK_LEN], int n) {
    /* keep toks[0] (command), strip filler from rest */
    char tmp[MAX_TOK][MAX_TOK_LEN]; int m = 0;
    strncpy(tmp[m++], toks[0], MAX_TOK_LEN-1);
    for (int i = 1; i < n; i++)
        if (!is_filler(toks[i])) strncpy(tmp[m++], toks[i], MAX_TOK_LEN-1);
    for (int i = 0; i < m; i++) strncpy(toks[i], tmp[i], MAX_TOK_LEN-1);
    return m;
}

/* ── helpers ─────────────────────────────────────────────────────────────── */
static void join(char toks[][MAX_TOK_LEN], int s, int e, char *out, size_t sz) {
    out[0] = '\0';
    for (int i = s; i < e; i++) {
        if (i > s) strncat(out, " ", sz-strlen(out)-1);
        strncat(out, toks[i], sz-strlen(out)-1);
    }
}
static void resolve(char toks[][MAX_TOK_LEN], int s, int e, char *out, size_t sz) {
    out[0] = '\0';
    for (int i = s; i < e; i++) {
        if (i > s) strncat(out, " ", sz-strlen(out)-1);
        char v[MAX_VAL_LEN]; evaluate(toks[i], v, sizeof(v));
        strncat(out, v, sz-strlen(out)-1);
    }
}

/* ── forward declarations ────────────────────────────────────────────────── */
void execute_line(const char *line, int repl);

/* ── eval_condition ──────────────────────────────────────────────────────── */
/* content: already-tokenized segment (no leading "if")
   returns 1=matched, writes action string to action_out */
static int eval_cond(const char *content, char *action_out, size_t action_sz) {
    action_out[0] = '\0';
    char toks[MAX_TOK][MAX_TOK_LEN];
    int n = tokenize(content, toks, MAX_TOK);
    if (n == 0) return 0;

    /* "user says <phrase> [then] <action>" */
    if (n >= 2 && strcasecmp(toks[0],"user")==0 && strcasecmp(toks[1],"says")==0) {
        int then_i = -1;
        for (int i = 2; i < n; i++)
            if (strcasecmp(toks[i],"then")==0) { then_i = i; break; }
        char phrase[MAX_LINE] = "";
        int act_start;
        if (then_i >= 0) { join(toks,2,then_i,phrase,sizeof(phrase)); act_start=then_i+1; }
        else             { if (n>2) strncpy(phrase,toks[2],sizeof(phrase)-1); act_start=3; }
        join(toks, act_start, n, action_out, action_sz);
        const char *ans = get_var("answer"); if (!ans) return 0;
        /* strip filler from answer before comparing so "what is your pet called"
           matches phrase "what is pet called" (your = filler) */
        char atoks[MAX_TOK][MAX_TOK_LEN];
        int an = tokenize(ans, atoks, MAX_TOK);
        char stripped_ans[MAX_LINE] = "";
        for (int i = 0; i < an; i++) {
            if (is_filler(atoks[i])) continue;
            if (stripped_ans[0]) strncat(stripped_ans," ",sizeof(stripped_ans)-strlen(stripped_ans)-1);
            strncat(stripped_ans, atoks[i], sizeof(stripped_ans)-strlen(stripped_ans)-1);
        }
        char la[MAX_VAL_LEN], lp[MAX_LINE];
        strncpy(la,stripped_ans,sizeof(la)-1); strncpy(lp,phrase,sizeof(lp)-1);
        for (char *p=la;*p;p++) *p=tolower(*p);
        for (char *p=lp;*p;p++) *p=tolower(*p);
        return strstr(la,lp) != NULL;
    }

    /* "<var> is [not] <val> [then] <action>" */
    int then_i=-1, is_i=-1;
    for (int i=0;i<n;i++) {
        if (strcasecmp(toks[i],"then")==0 && then_i<0) then_i=i;
        if (strcasecmp(toks[i],"is"  )==0 && is_i  <0) is_i  =i;
    }
    if (is_i < 0) return 0;

    char lval[MAX_VAL_LEN], ltok[MAX_VAL_LEN];
    join(toks,0,is_i,ltok,sizeof(ltok)); evaluate(ltok,lval,sizeof(lval));

    int negate=0, val_s=is_i+1;
    if (val_s<n && strcasecmp(toks[val_s],"not")==0) { negate=1; val_s++; }

    int val_e = (then_i>=0) ? then_i : val_s+1;
    if (then_i>=0) join(toks,then_i+1,n,action_out,action_sz);
    else           join(toks,val_e,   n,action_out,action_sz);

    char rtok[MAX_VAL_LEN], rval[MAX_VAL_LEN];
    join(toks,val_s,val_e,rtok,sizeof(rtok)); evaluate(rtok,rval,sizeof(rval));

    int match = strcasecmp(lval,rval)==0;
    return negate ? !match : match;
}

/* ── chain segment types ─────────────────────────────────────────────────── */
#define SEG_IF        0
#define SEG_BUT_IF    1
#define SEG_OTHERWISE 2
#define SEG_DONT      3

typedef struct { int type; char content[MAX_LINE]; } Segment;

/* ── parse_chain ─────────────────────────────────────────────────────────── */
/* toks/n: everything after the initial "if" token */
static int parse_chain(char toks[][MAX_TOK_LEN], int n, Segment *segs, int maxs) {
    int ns=0, cur_type=SEG_IF;
    char cur[MAX_LINE]="";

    #define FLUSH() do { \
        if (ns<maxs) { segs[ns].type=cur_type; strncpy(segs[ns].content,cur,MAX_LINE-1); ns++; } \
        cur[0]='\0'; } while(0)
    #define APPEND(w) do { \
        if (cur[0]) strncat(cur," ",MAX_LINE-strlen(cur)-1); \
        strncat(cur,w,MAX_LINE-strlen(cur)-1); } while(0)

    for (int i=0; i<n; ) {
        const char *t  = toks[i];
        const char *t1 = (i+1<n)?toks[i+1]:"";
        const char *t2 = (i+2<n)?toks[i+2]:"";
        const char *t3 = (i+3<n)?toks[i+3]:"";

        /* but dont/don't answer if */
        if (strcasecmp(t,"but")==0 &&
            (strcasecmp(t1,"dont")==0||strcasecmp(t1,"don't")==0) &&
            strcasecmp(t2,"answer")==0 && strcasecmp(t3,"if")==0) {
            FLUSH(); cur_type=SEG_DONT; i+=4;
        }
        /* but if */
        else if (strcasecmp(t,"but")==0 && strcasecmp(t1,"if")==0) {
            FLUSH(); cur_type=SEG_BUT_IF; i+=2;
        }
        /* otherwise */
        else if (strcasecmp(t,"otherwise")==0) {
            FLUSH(); cur_type=SEG_OTHERWISE; i++;
        }
        /* bare mid-line "if" starts a new condition */
        else if (strcasecmp(t,"if")==0 && cur[0]) {
            FLUSH(); cur_type=SEG_BUT_IF; i++;
        }
        else { APPEND(toks[i]); i++; }
    }
    FLUSH();
    #undef FLUSH
    #undef APPEND
    return ns;
}

/* ── run_chain ───────────────────────────────────────────────────────────── */
static void run_chain(Segment *segs, int ns, int repl) {
    char action[MAX_LINE];
    for (int i=0; i<ns; i++) {
        if (segs[i].type==SEG_OTHERWISE) { execute_line(segs[i].content, repl); return; }
        if (segs[i].type==SEG_DONT) {
            if (eval_cond(segs[i].content, action, sizeof(action))) return;
            continue;
        }
        if (eval_cond(segs[i].content, action, sizeof(action))) {
            execute_line(action, repl); return;
        }
    }
}

/* ── execute_line ────────────────────────────────────────────────────────── */
void execute_line(const char *src, int repl) {
    while (*src==' '||*src=='\t') src++;
    if (!*src || *src=='#') return;

    char toks[MAX_TOK][MAX_TOK_LEN];
    int n = tokenize(src, toks, MAX_TOK);
    if (!n) return;
    n = strip_filler(toks, n);
    const char *cmd = toks[0];

    /* say */
    if (strcasecmp(cmd,"say")==0) {
        char out[MAX_LINE]; resolve(toks,1,n,out,sizeof(out)); puts(out); return;
    }
    /* set <var> to <val> */
    if (strcasecmp(cmd,"set")==0 && n>=4 && strcasecmp(toks[2],"to")==0) {
        char raw[MAX_VAL_LEN], val[MAX_VAL_LEN];
        join(toks,3,n,raw,sizeof(raw)); evaluate(raw,val,sizeof(val));
        set_var(toks[1],val); save_vars(); return;
    }
    /* remember <var> is <val>  e.g. "remember the world is blue" */
    if ((strcasecmp(cmd,"remember")==0 || strcasecmp(cmd,"remeber")==0) && n>=4 && strcasecmp(toks[2],"is")==0) {
        if (strcasecmp(cmd,"remeber")==0)
            puts("(it's r-e-m-e-m-b-e-r, but fine, i got you)");
        char raw[MAX_VAL_LEN], val[MAX_VAL_LEN];
        join(toks,3,n,raw,sizeof(raw)); evaluate(raw,val,sizeof(val));
        set_var(toks[1],val); save_vars(); return;
    }
    /* show <var> */
    if (strcasecmp(cmd,"show")==0) {
        for (int i=1;i<n;i++) {
            char v[MAX_VAL_LEN]; evaluate(toks[i],v,sizeof(v)); printf("%s ",v);
        }
        puts(""); return;
    }
    /* ask <prompt> */
    if (strcasecmp(cmd,"ask")==0) {
        char p[MAX_LINE]; resolve(toks,1,n,p,sizeof(p));
        printf("%s ", p); fflush(stdout);
        char buf[MAX_LINE];
        if (fgets(buf,sizeof(buf),stdin)) {
            buf[strcspn(buf,"\n")]='\0'; set_var("answer",buf); save_vars();
        }
        return;
    }
    /* wait <n> */
    if (strcasecmp(cmd,"wait")==0 && n>=2) {
        char v[MAX_VAL_LEN]; evaluate(toks[1],v,sizeof(v)); sleep((unsigned)atoi(v)); return;
    }
    /* run <shell cmd> */
    if (strcasecmp(cmd,"run")==0) {
        char s[MAX_LINE]; join(toks,1,n,s,sizeof(s)); system(s); return;
    }
    /* in <lang> <description> */
    if (strcasecmp(cmd,"in")==0 && n>=3) {
        const char *lang=toks[1], *ext="";
        if (strcasecmp(lang,"python")==0) ext=".py";
        else if (strcasecmp(lang,"node")==0) ext=".js";
        else if (strcasecmp(lang,"ruby")==0) ext=".rb";
        else if (strcasecmp(lang,"bash")==0) ext=".sh";
        char fname[MAX_LINE]="";
        for (int i=2;i<n;i++) {
            if (i>2) strncat(fname,"_",sizeof(fname)-strlen(fname)-1);
            strncat(fname,toks[i],sizeof(fname)-strlen(fname)-1);
        }
        strncat(fname,ext,sizeof(fname)-strlen(fname)-1);
        char s[MAX_LINE]; snprintf(s,sizeof(s),"%s %s",lang,fname); system(s); return;
    }
    /* make file <name> */
    if (strcasecmp(cmd,"make")==0 && n>=3 && strcasecmp(toks[1],"file")==0) {
        char fname[MAX_LINE]; join(toks,2,n,fname,sizeof(fname));
        FILE *f=fopen(fname,"r");
        if (f) { fclose(f); printf("%s already exists\n",fname); }
        else { f=fopen(fname,"w"); if(f){fclose(f); printf("created %s\n",fname);} }
        return;
    }
    /* delete file <name> */
    if (strcasecmp(cmd,"delete")==0 && n>=3 && strcasecmp(toks[1],"file")==0) {
        char fname[MAX_LINE]; join(toks,2,n,fname,sizeof(fname));
        remove(fname)==0 ? printf("deleted %s\n",fname) : printf("%s not found\n",fname);
        return;
    }
    /* repeat <n> times <action> */
    if (strcasecmp(cmd,"repeat")==0 && n>=4 && strcasecmp(toks[2],"times")==0) {
        char nv[MAX_VAL_LEN]; evaluate(toks[1],nv,sizeof(nv));
        int count=atoi(nv);
        char action[MAX_LINE]; join(toks,3,n,action,sizeof(action));
        for (int i=0;i<count;i++) execute_line(action,repl);
        return;
    }
    /* if chain */
    if (strcasecmp(cmd,"if")==0) {
        static Segment segs[MAX_SEGS];
        int ns = parse_chain(toks+1, n-1, segs, MAX_SEGS);

        /* auto-listen if any segment uses "user says" */
        int listen=0;
        for (int i=0;i<ns;i++) {
            char tmp[MAX_TOK][MAX_TOK_LEN];
            int tn=tokenize(segs[i].content,tmp,MAX_TOK);
            if (tn>=2 && strcasecmp(tmp[0],"user")==0 && strcasecmp(tmp[1],"says")==0)
                { listen=1; break; }
        }
        if (listen) {
            printf("(listening — press Ctrl+C to stop)\n");
            char buf[MAX_LINE];
            while (1) {
                printf("you: "); fflush(stdout);
                if (!fgets(buf,sizeof(buf),stdin)) break;
                buf[strcspn(buf,"\n")]='\0';
                if (!buf[0]) continue;
                set_var("answer",buf); save_vars();
                run_chain(segs,ns,repl);
            }
            printf("\n(stopped listening)\n");
        } else {
            run_chain(segs,ns,repl);
        }
        return;
    }
    /* bye */
    if (strcasecmp(cmd,"bye")==0||strcasecmp(cmd,"exit")==0||strcasecmp(cmd,"quit")==0) {
        puts("bye!"); exit(0);
    }

    printf("i don't understand: %s\n", src);
}

/* ── run file ────────────────────────────────────────────────────────────── */
static void run_file(const char *path) {
    FILE *f=fopen(path,"r");
    if (!f) { fprintf(stderr,"cannot open %s\n",path); return; }
    char line[MAX_LINE];
    while (fgets(line,sizeof(line),f)) {
        line[strcspn(line,"\n")]='\0';
        execute_line(line,0);
    }
    fclose(f);
}

/* ── REPL ────────────────────────────────────────────────────────────────── */
static void repl(void) {
    puts("JOW lang — type 'bye' to quit");
    char line[MAX_LINE];
    while (1) {
        printf(">> "); fflush(stdout);
        if (!fgets(line,sizeof(line),stdin)) { puts("\nbye!"); break; }
        line[strcspn(line,"\n")]='\0';
        execute_line(line,1);
    }
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    snprintf(vars_path,sizeof(vars_path),"%s/.jow_vars",
             getenv("HOME")?getenv("HOME"):".");
    load_vars();
    if (argc==2) run_file(argv[1]);
    else repl();
    return 0;
}
