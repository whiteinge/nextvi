#include <stdint.h>
typedef uint64_t u64;
typedef int64_t s64;
/*
Nextvi main header
==================
The purpose of this file is to provide high level overview
of entire Nextvi. Due to absence of any build system some of
these definitions may not be required to successfully compile
Nextvi. They are kept here for your benefit and organization.
If something is listed here, it must be used across multiple
files and thus is never static.
*/

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif
extern char **xenvp;

/* helper macros */
#define LEN(a)		(s64)(sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) < (b) ? (b) : (a))
/* for debug; printf() but to file */
#define p(s, ...)\
	{FILE *f = fopen("file", "a");\
	fprintf(f, s, ##__VA_ARGS__);\
	fclose(f);}\
/* ease up ridiculous global stuffing */
#define preserve(type, name, value) \
type tmp##name = name; \
name = value; \

#define restore(name) \
name = tmp##name; \

/* utility funcs */
void *emalloc(size_t size);
void *erealloc(void *p, size_t size);
s64 dstrlen(const char *s, char delim);
char *itoa(s64 n, char s[]);

/* main functions */
extern s64 xgrec;
void vi(s64 init);
void ex(void);

/* sbuf string buffer, variable-sized string */
#define NEXTSZ(o, r)	MAX(o * 2, o + r)
typedef struct sbuf {
	char *s;	/* allocated buffer */
	s64 s_n;	/* length of the string stored in s[] */
	s64 s_sz;	/* size of memory allocated for s[] */
} sbuf;

#define sbuf_extend(sb, newsz) \
{ \
	char *s = sb->s; \
	sb->s_sz = newsz; \
	sb->s = emalloc(sb->s_sz); \
	memcpy(sb->s, s, sb->s_n); \
	free(s); \
} \

#define _sbuf_make(sb, newsz, alloc) \
{ \
	alloc; \
	sb->s_sz = newsz; \
	sb->s = emalloc(newsz); \
	sb->s_n = 0; \
} \

#define sbuf_chr(sb, c) \
{ \
	if (sb->s_n + 1 >= sb->s_sz) \
		sbuf_extend(sb, NEXTSZ(sb->s_sz, 1)) \
	sb->s[sb->s_n++] = c; \
} \

#define sbuf_(sb, x, len, func) \
if (sb->s_n + len >= sb->s_sz) \
	sbuf_extend(sb, NEXTSZ(sb->s_sz, len + 1)) \
mem##func(sb->s + sb->s_n, x, len); \
sb->s_n += len; \

#define sbuf_smake(sb, newsz) sbuf _##sb, *sb = &_##sb; _sbuf_make(sb, newsz,)
#define sbuf_make(sb, newsz) { _sbuf_make(sb, newsz, sb = emalloc(sizeof(*sb))) }
#define sbuf_free(sb) { free(sb->s); free(sb); }
#define sbuf_set(sb, ch, len) { sbuf_(sb, ch, len, set) }
#define sbuf_mem(sb, s, len) { sbuf_(sb, s, len, cpy) }
#define sbuf_str(sb, s) { const char *p = s; while(*p) sbuf_chr(sb, *p++) }
#define sbuf_cut(sb, len) { sb->s_n = len; }
/* sbuf functions that NULL terminate strings */
#define sbuf_null(sb) { sb->s[sb->s_n] = '\0'; }
#define sbufn_make(sb, newsz) { sbuf_make(sb, newsz) sbuf_null(sb) }
#define sbufn_set(sb, ch, len) { sbuf_set(sb, ch, len) sbuf_null(sb) }
#define sbufn_mem(sb, s, len) { sbuf_mem(sb, s, len) sbuf_null(sb) }
#define sbufn_str(sb, s) { sbuf_str(sb, s) sbuf_null(sb) }
#define sbufn_cut(sb, len) { sbuf_cut(sb, len) sbuf_null(sb) }
#define sbufn_chr(sb, c) { sbuf_chr(sb, c) sbuf_null(sb) }
#define sbufn_sret(sb) { sbuf_set(sb, '\0', 4) return sb->s; }

/* regex.c regular expression sets */
#define REG_ICASE	0x01
#define REG_NEWLINE	0x02	/* Unlike posix, controls termination by '\n' */
#define REG_NOTBOL	0x04
#define REG_NOTEOL	0x08
typedef struct {
	char *rm_so;
	char *rm_eo;
} regmatch_t;
typedef struct {
	s64 unilen;	/* number of integers in insts */
	s64 len;	/* number of atoms/instructions */
	s64 sub;	/* interim val = save count; final val = nsubs size */
	s64 presub;	/* interim val = save count; final val = 1 rsub size */
	s64 splits;	/* number of split insts */
	s64 sparsesz;	/* sdense size */
	s64 flg;	/* stored flags */
	s64 insts[];	/* re code */
} rcode;
/* regular expression set */
typedef struct {
	rcode *regex;		/* the combined regular expression */
	s64 n;			/* number of regular expressions in this set */
	s64 *grp;		/* the group assigned to each subgroup */
	s64 *setgrpcnt;		/* number of groups in each regular expression */
	s64 grpcnt;		/* group count */
} rset;
rset *rset_make(s64 n, char **pat, s64 flg);
rset *rset_smake(char *pat, s64 flg)
	{ char *ss[1] = {pat}; return rset_make(1, ss, flg); }
s64 rset_find(rset *re, char *s, s64 *grps, s64 flg);
void rset_free(rset *re);
char *re_read(char **src);

/* lbuf.c line buffer, managing a number of lines */
#define NMARKS_BASE		('z' - 'a' + 2)
#define NMARKS			32
struct lopt {
	char *ins;		/* inserted text */
	char *del;		/* deleted text */
	s64 pos, n_ins, n_del;	/* modification location */
	s64 pos_off;		/* cursor line offset */
	s64 seq;		/* operation number */
	s64 *mark, *mark_off;	/* saved marks */
};
struct linfo {
	s64 len;
	s64 grec;
};
struct lbuf {
	char **ln;		/* buffer lines */
	struct lopt *hist;	/* buffer history */
	s64 mark[NMARKS];	/* mark lines */
	s64 mark_off[NMARKS];	/* mark line offsets */
	s64 ln_n;		/* number of lines in ln[] */
	s64 ln_sz;		/* size of ln[] */
	s64 useq;		/* current operation sequence */
	s64 hist_sz;		/* size of hist[] */
	s64 hist_n;		/* current history head in hist[] */
	s64 hist_u;		/* current undo head in hist[] */
	s64 useq_zero;		/* useq for lbuf_saved() */
	s64 useq_last;		/* useq before hist[] */
};
#define lbuf_len(lb) lb->ln_n
#define lbuf_s(ln) ((struct linfo*)(ln - sizeof(struct linfo)))
#define lbuf_i(lb, pos) ((struct linfo*)(lb->ln[pos] - sizeof(struct linfo)))
struct lbuf *lbuf_make(void);
void lbuf_free(struct lbuf *lbuf);
s64 lbuf_rd(struct lbuf *lbuf, s64 fd, s64 beg, s64 end, s64 init);
s64 lbuf_wr(struct lbuf *lbuf, s64 fd, s64 beg, s64 end);
void lbuf_iedit(struct lbuf *lbuf, char *s, s64 beg, s64 end, s64 init);
#define lbuf_edit(lb, s, beg, end) lbuf_iedit(lb, s, beg, end, 0)
char *lbuf_cp(struct lbuf *lbuf, s64 beg, s64 end);
char *lbuf_get(struct lbuf *lbuf, s64 pos);
void lbuf_emark(struct lbuf *lb, struct lopt *lo, s64 beg, s64 end);
struct lopt *lbuf_opt(struct lbuf *lb, char *buf, s64 pos, s64 n_del, s64 init);
void lbuf_mark(struct lbuf *lbuf, s64 mark, s64 pos, s64 off);
s64 lbuf_jump(struct lbuf *lbuf, s64 mark, s64 *pos, s64 *off);
s64 lbuf_undojump(struct lbuf *lb, s64 *pos, s64 *off);
s64 lbuf_undo(struct lbuf *lbuf);
s64 lbuf_redo(struct lbuf *lbuf);
s64 lbuf_modified(struct lbuf *lb);
void lbuf_saved(struct lbuf *lb, s64 clear);
s64 lbuf_indents(struct lbuf *lb, s64 r);
s64 lbuf_eol(struct lbuf *lb, s64 r);
s64 lbuf_findchar(struct lbuf *lb, char *cs, s64 cmd, s64 n, s64 *r, s64 *o);
s64 lbuf_search(struct lbuf *lb, rset *re, s64 dir, s64 *r,
			s64 *o, s64 ln_n, s64 skip);
#define lbuf_dedup(lb, str, n) \
{ for (s64 i = 0; i < lbuf_len(lb);) { \
	char *s = lbuf_get(lb, i); \
	if (n == lbuf_s(s)->len && !memcmp(str, s, n)) \
		lbuf_edit(lb, NULL, i, i + 1); \
	else \
		i++; \
}} \

/* regions */
s64 lbuf_sectionbeg(struct lbuf *lb, s64 dir, s64 *row, s64 *off, s64 ch);
s64 lbuf_wordbeg(struct lbuf *lb, s64 big, s64 dir, s64 *row, s64 *off);
s64 lbuf_wordend(struct lbuf *lb, s64 big, s64 dir, s64 *row, s64 *off);
s64 lbuf_pair(struct lbuf *lb, s64 *row, s64 *off);

/* ren.c rendering lines */
typedef struct {
	char **chrs;
	char *s;	/* to prevent redundant computations, ensure pointer uniqueness */
	s64 *wid;
	s64 *col;
	s64 *pos;
	s64 n;
	s64 cmax;
	s64 ctx;
} ren_state;
extern ren_state *rstate;
ren_state *ren_position(char *s);
s64 ren_next(char *s, s64 p, s64 dir);
s64 ren_eol(char *s, s64 dir);
s64 ren_pos(char *s, s64 off);
s64 ren_cursor(char *s, s64 pos);
s64 ren_noeol(char *s, s64 p);
s64 ren_off(char *s, s64 p);
char *ren_translate(char *s, char *ln);
/* text direction */
s64 dir_context(char *s);
void dir_init(void);
/* syntax highlighting */
#define SYN_BD		0x010000
#define SYN_IT		0x020000
#define SYN_RV		0x040000
#define SYN_FGMK(f)	(0x100000 | (f))
#define SYN_BGMK(b)	(0x200000 | ((b) << 8))
#define SYN_FLG		0xff0000
#define SYN_FGSET(a)	((a) & 0x1000ff)
#define SYN_BGSET(a)	((a) & 0x20ff00)
#define SYN_FG(a)	((a) & 0xff)
#define SYN_BG(a)	(((a) >> 8) & 0xff)
extern s64 syn_reload;
extern s64 syn_blockhl;
char *syn_setft(char *ft);
void syn_scdir(s64 scdir);
void syn_highlight(s64 *att, char *s, s64 n);
char *syn_filetype(char *path);
s64 syn_merge(s64 old, s64 new);
void syn_reloadft(void);
s64 syn_findhl(s64 id);
void syn_addhl(char *reg, s64 id, s64 reload);
void syn_init(void);

/* uc.c utf-8 helper functions */
extern unsigned char utf8_length[256];
extern s64 zwlen, def_zwlen;
extern s64 bclen, def_bclen;
/* return the length of a utf-8 character */
#define uc_len(s) utf8_length[(unsigned char)s[0]]
/* the unicode codepoint of the given utf-8 character */
#define uc_code(dst, s, l) \
dst = (unsigned char)s[0]; \
l = utf8_length[dst]; \
if (l == 1); \
else if (l == 2) \
	dst = ((dst & 0x1f) << 6) | (s[1] & 0x3f); \
else if (l == 3) \
	dst = ((dst & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f); \
else if (l == 4) \
	dst = ((dst & 0x07) << 18) | ((s[1] & 0x3f) << 12) | \
		((s[2] & 0x3f) << 6) | (s[3] & 0x3f); \
else \
	dst = 0; \

s64 uc_wid(s64 c);
s64 uc_slen(char *s);
char *uc_chr(char *s, s64 off);
s64 uc_off(char *s, s64 off);
char *uc_subl(char *s, s64 beg, s64 end, s64 *rlen);
char *uc_sub(char *s, s64 beg, s64 end)
	{ s64 l; return uc_subl(s, beg, end, &l); }
char *uc_dup(const char *s);
s64 uc_isspace(char *s);
s64 uc_isprint(char *s);
s64 uc_isdigit(char *s);
s64 uc_isalpha(char *s);
s64 uc_kind(char *c);
s64 uc_isbell(s64 c);
s64 uc_acomb(s64 c);
char **uc_chop(char *s, u64 *n);
char *uc_beg(char *beg, char *s);
char *uc_shape(char *beg, char *s, s64 c);

/* term.c managing the terminal */
extern sbuf *term_sbuf;
extern s64 term_record;
extern s64 xrows, xcols;
extern u64 ibuf_pos, ibuf_cnt, ibuf_sz, icmd_pos;
extern unsigned char *ibuf, icmd[4096];
extern u64 texec, tn;
#define term_write(s, n) if (xled) write(1, s, n);
void term_init(void);
void term_done(void);
void term_clean(void);
void term_suspend(void);
void term_chr(s64 ch);
void term_pos(s64 r, s64 c);
void term_kill(void);
void term_room(s64 n);
s64 term_read(void);
void term_commit(void);
char *term_att(s64 att);
void term_push(char *s, u64 n);
void term_back(s64 c);
#define term_dec() ibuf_pos--; icmd_pos--;
#define term_exec(s, n, type) \
{ \
	preserve(s64, ibuf_cnt, ibuf_cnt) \
	preserve(s64, ibuf_pos, ibuf_cnt) \
	term_push(s, n); \
	preserve(s64, texec, type) \
	tn = 0; \
	vi(0); \
	tn = 0; \
	restore(texec) \
	if (xquit > 0) \
		xquit = 0; \
	restore(ibuf_pos) \
	restore(ibuf_cnt) \
} \

/* process management */
char *cmd_pipe(char *cmd, char *ibuf, s64 oproc);
char *xgetenv(char* q[]);

#define TK_ESC		(TK_CTL('['))
#define TK_CTL(x)	((x) & 037)
#define TK_INT(c)	((c) <= 0 || (c) == TK_ESC || (c) == TK_CTL('c'))

/* led.c line-oriented input and output */
typedef struct {
	char *s;
	s64 off;
	s64 att;
} led_att;
extern sbuf *led_attsb;
char *led_prompt(char *pref, char *post, char *insert, s64 *kmap);
sbuf *led_input(char *pref, char **post, s64 row, s64 lsh);
void led_render(char *s0, s64 cbeg, s64 cend);
#define _led_render(msg, row, col, beg, end, kill) \
{ \
	s64 record = term_record; \
	term_record = 1; \
	term_pos(row, col); \
	kill \
	led_render(msg, beg, end); \
	if (!record) \
		term_commit(); \
} \

#define led_prender(msg, row, col, beg, end) _led_render(msg, row, col, beg, end, /**/)
#define led_crender(msg, row, col, beg, end) _led_render(msg, row, col, beg, end, term_kill();)
#define led_recrender(msg, row, col, beg, end) \
{ rstate->s = NULL; led_crender(msg, row, col, beg, end); }
char *led_read(s64 *kmap, s64 c);
s64 led_pos(char *s, s64 pos);
void led_done(void);

/* ex.c ex commands */
extern char *xregs[256];
struct buf {
	char *ft;			/* file type */
	char *path;			/* file path */
	struct lbuf *lb;
	s64 plen, row, off, top;
	long mtime;			/* modification time */
	signed char td;			/* text direction */
};
extern s64 xbufcur;
extern struct buf *ex_buf;
extern struct buf *ex_pbuf;
extern struct buf *bufs;
extern struct buf tempbufs[2];
#define istempbuf(buf) (buf - bufs < 0 || buf - bufs >= xbufcur)
#define EXLEN	512	/* ex line length */
#define ex_path ex_buf->path
#define ex_ft ex_buf->ft
#define xb ex_buf->lb
#define exbuf_load(buf) \
	xrow = buf->row; \
	xoff = buf->off; \
	xtop = buf->top; \
	xtd = buf->td; \

#define exbuf_save(buf) \
	buf->row = xrow; \
	buf->off = xoff; \
	buf->top = xtop; \
	buf->td = xtd; \

void temp_open(s64 i, char *name, char *ft);
void temp_switch(s64 i);
void temp_write(s64 i, char *str);
void temp_pos(s64 i, s64 row, s64 off, s64 top);
s64 ex_exec(const char *ln);
#define ex_command(ln) { ex_exec(ln); vi_regputraw(':', ln, 0, 0); }
char *ex_read(char *msg);
void ex_cprint(char *line, s64 r, s64 c, s64 ln);
#define ex_print(line) ex_cprint(line, -1, 0, 1)
void ex_init(char **files, s64 n);
void ex_bufpostfix(struct buf *p, s64 clear);
s64 ex_krs(rset **krs, s64 *dir);
void ex_krsset(char *kwd, s64 dir);
s64 ex_edit(const char *path, s64 len);
void ec_bufferi(s64 id);
void bufs_switch(s64 idx);
#define bufs_switchwft(idx) \
{ if (&bufs[idx] != ex_buf) { bufs_switch(idx); syn_setft(ex_ft); } } \

/* conf.c configuration variables */
/* map file names to file types */
extern s64 conf_mode;
struct filetype {
	char *ft;		/* file type */
	char *pat;		/* file name pattern */
};
extern struct filetype fts[];
extern s64 ftslen;
/* syntax highlighting patterns */
struct highlight {
	char *ft;		/* the filetype of this pattern */
	char *pat;		/* regular expression */
	s64 att[16];		/* attributes of the matched groups */
	signed char end[16];	/* the group ending this pattern;
				if set on multi-line the block emits all other matches in rset
				else defines hl continuation for the group:
				positive value - continue at rm_so
				zero (default) - continue at rm_eo
				negative value - continue at sp+1 */
	signed char blkend;	/* the ending group for multi-line patterns;
				negative group is able to start and end itself */
	char id;		/* id of this hl */
};
extern struct highlight hls[];
extern s64 hlslen;
/* direction context patterns; specifies the direction of a whole line */
struct dircontext {
	s64 dir;
	char *pat;
};
extern struct dircontext dctxs[];
extern s64 dctxlen;
/* direction marks; the direction of a few words in a line */
struct dirmark {
	s64 ctx;	/* the direction context for this mark; 0 means any */
	s64 dir[8];	/* the direction of a matched text group */
	char *pat;
};
extern struct dirmark dmarks[];
extern s64 dmarkslen;
/* character placeholders */
struct placeholder {
	s64 cp[2];	/* the source character codepoint */
	char d[8];	/* the placeholder */
	s64 wid;	/* the width of the placeholder */
	s64 l;		/* the length of the codepoint */
};
extern struct placeholder _ph[];
extern struct placeholder *ph;
extern s64 phlen;
extern s64 conf_hlrev;
char **conf_kmap(s64 id);
s64 conf_kmapfind(char *name);
char *conf_digraph(s64 c1, s64 c2);

/* vi.c */
void vi_regputraw(unsigned char c, const char *s, s64 ln, s64 append);
void vi_regput(s64 c, const char *s, s64 ln);
/* file system */
void dir_calc(char *path);
/* global variables */
extern s64 xrow;
extern s64 xoff;
extern s64 xtop;
extern s64 xleft;
extern s64 xvis;
extern s64 xled;
extern s64 xquit;
extern s64 xic;
extern s64 xai;
extern s64 xtd;
extern s64 xshape;
extern s64 xorder;
extern s64 xhl;
extern s64 xhll;
extern s64 xhlw;
extern s64 xhlp;
extern s64 xhlr;
extern s64 xkmap;
extern s64 xkmap_alt;
extern s64 xtabspc;
extern s64 xish;
extern s64 xgrp;
extern s64 xpac;
extern s64 xkwdcnt;
extern s64 xkwddir;
extern s64 xmpt;
extern s64 xpr;
extern s64 xsep;
extern rset *xkwdrs;
extern sbuf *xacreg;
extern rset *fsincl;
extern char *fs_exdir;
extern s64 vi_hidch;
extern s64 vi_insmov;
extern s64 vi_lncol;
