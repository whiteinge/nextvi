/* rendering strings */

static rset *dir_rslr;	/* pattern of marks for left-to-right strings */
static rset *dir_rsrl;	/* pattern of marks for right-to-left strings */
static rset *dir_rsctx;	/* direction context patterns */

static void dir_reverse(s64 *ord, s64 beg, s64 end)
{
	end--;
	while (beg < end) {
		s64 tmp = ord[beg];
		ord[beg] = ord[end];
		ord[end] = tmp;
		beg++;
		end--;
	}
}

/* reorder the characters based on direction marks and characters */
static s64 dir_reorder(char **chrs, s64 *ord, s64 end, s64 dir)
{
	rset *rs = dir < 0 ? dir_rsrl : dir_rslr;
	s64 beg = 0, end1 = end, c_beg, c_end;
	s64 subs[LEN(dmarks[0].dir) * 2], gdir, found, i;
	while (beg < end) {
		char *s = chrs[beg];
		found = rset_find(rs, s, subs,
				*chrs[end-1] == '\n' ? REG_NEWLINE : 0);
		if (found >= 0) {
			for (i = 0; i < end1; i++)
				ord[i] = i;
			c_end = 0;
			end1 = -1;
			for (i = 0; i < rs->setgrpcnt[found]; i++) {
				gdir = dmarks[found].dir[i];
				if (subs[i * 2] < 0 || gdir >= 0)
					continue;
				c_beg = uc_off(s, subs[i * 2]);
				c_end = uc_off(s, subs[i * 2 + 1]);
				dir_reverse(ord, beg+c_beg, beg+c_end);
			}
			beg += c_end ? c_end : 1;
		} else
			break;
	}
	return end1 < 0;
}

/* return the direction context of the given line */
s64 dir_context(char *s)
{
	s64 found;
	if (xtd > +1)
		return +1;
	if (xtd < -1)
		return -1;
	if (dir_rsctx && s)
		if ((found = rset_find(dir_rsctx, s, NULL, 0)) >= 0)
			return dctxs[found].dir;
	return xtd < 0 ? -1 : +1;
}

void dir_init(void)
{
	char *relr[128];
	char *rerl[128];
	char *ctx[128];
	s64 i;
	for (i = 0; i < dmarkslen; i++) {
		relr[i] = dmarks[i].ctx >= 0 ? dmarks[i].pat : NULL;
		rerl[i] = dmarks[i].ctx <= 0 ? dmarks[i].pat : NULL;
	}
	dir_rslr = rset_make(i, relr, 0);
	dir_rsrl = rset_make(i, rerl, 0);
	for (i = 0; i < dctxlen; i++)
		ctx[i] = dctxs[i].pat;
	dir_rsctx = rset_make(i, ctx, 0);
}

static s64 ren_cwid(char *s, s64 pos)
{
	if (s[0] == '\t')
		return xtabspc - (pos & (xtabspc-1));
	if (s[0] == '\n')
		return 1;
	s64 c, l; uc_code(c, s, l)
	for (s64 i = 0; i < phlen; i++)
		if (c >= ph[i].cp[0] && c <= ph[i].cp[1] && l == ph[i].l)
			return ph[i].wid;
	return uc_wid(c);
}

static ren_state rstates[2];
ren_state *rstate = &rstates[0];

/* specify the screen position of the characters in s */
ren_state *ren_position(char *s)
{
	if (rstate->s == s)
		return rstate;
	else if (rstate->col) {
		free(rstate->col - 2);
		free(rstate->pos);
		free(rstate->chrs);
	}
	u64 n, i, c = 2;
	s64 cpos = 0, wid, *off, *pos, *col;
	char **chrs = uc_chop(s, &n);
	pos = emalloc(((n + 1) * sizeof(pos[0])) * 2);
	off = &pos[n+1];
	rstate->ctx = dir_context(s);
	if (xorder && dir_reorder(chrs, off, n, rstate->ctx)) {
		s64 *wids = emalloc(n * sizeof(wids[0]));
		for (i = 0; i < n; i++) {
			pos[off[i]] = cpos;
			cpos += ren_cwid(chrs[off[i]], cpos);
		}
		col = emalloc((cpos + 2) * sizeof(col[0]));
		pos[n] = cpos;
		for (i = 0; i < n; i++) {
			wid = ren_cwid(chrs[off[i]], pos[off[i]]);
			wids[off[i]] = wid;
			while (wid--)
				col[c++] = off[i];
		}
		memcpy(off, wids, n * sizeof(wids[0]));
		free(wids);
	} else {
		for (i = 0; i < n; i++) {
			pos[i] = cpos;
			cpos += ren_cwid(chrs[i], cpos);
		}
		col = emalloc((cpos + 2) * sizeof(col[0]));
		pos[n] = cpos;
		for (i = 0; i < n; i++) {
			wid = pos[i+1] - pos[i];
			off[i] = wid;
			while (wid--)
				col[c++] = i;
		}
	}
	off[n] = 0;
	col[0] = n;
	col[1] = n;
	rstate->wid = off;
	rstate->cmax = cpos - 1;
	rstate->col = col + 2;
	rstate->s = s;
	rstate->pos = pos;
	rstate->chrs = chrs;
	rstate->n = n;
	return rstate;
}

/* convert character offset to visual position */
s64 ren_pos(char *s, s64 off)
{
	ren_state *r = ren_position(s);
	return off < r->n ? r->pos[off] : 0;
}

/* convert visual position to character offset */
s64 ren_off(char *s, s64 p)
{
	ren_state *r = ren_position(s);
	return r->col[p < r->cmax ? p : r->cmax];
}

/* adjust cursor position */
s64 ren_cursor(char *s, s64 p)
{
	if (!s)
		return 0;
	ren_state *r = ren_position(s);
	if (p >= r->cmax)
		p = r->cmax - (*r->chrs[r->col[r->cmax]] == '\n');
	s64 i = r->col[p];
	return r->pos[i] + r->wid[i] - 1;
}

/* return an offset before EOL */
s64 ren_noeol(char *s, s64 o)
{
	if (!s)
		return 0;
	ren_state *r = ren_position(s);
	o = o >= r->n ? r->n - 1 : MAX(0, o);
	return o - (o > 0 && *r->chrs[o] == '\n');
}

/* the visual position of the next character */
s64 ren_next(char *s, s64 p, s64 dir)
{
	ren_state *r = ren_position(s);
	if (p+dir < 0 || p > r->cmax)
		return r->pos[r->col[r->cmax]];
	s64 i = r->col[p];
	if (r->wid[i] > 1 && dir > 0)
		return r->pos[i] + r->wid[i];
	return r->pos[i] + dir;
}

char *ren_translate(char *s, char *ln)
{
	if (s[0] == '\t' || s[0] == '\n')
		return NULL;
	s64 c, l; uc_code(c, s, l)
	for (s64 i = 0; i < phlen; i++)
		if (c >= ph[i].cp[0] && c <= ph[i].cp[1] && l == ph[i].l)
			return ph[i].d;
	if (l == 1)
		return NULL;
	if (uc_acomb(c)) {
		static char buf[16] = "ـ";
		*((char*)memcpy(buf+2, s, l)+l) = '\0';
		return buf;
	}
	if (uc_isbell(c))
		return "�";
	return xshape ? uc_shape(ln, s, c) : NULL;
}

#define NFTS		30
/* mapping filetypes to regular expression sets */
static struct ftmap {
	s64 setbidx;
	s64 seteidx;
	char *ft;
	rset *rs;
} ftmap[NFTS];
static s64 ftmidx;
static s64 ftidx;

static rset *syn_ftrs;
static s64 last_scdir;
static s64 *blockatt;
static s64 blockcont;
s64 syn_reload;
s64 syn_blockhl;

static void syn_initft(s64 fti, s64 n, char *name)
{
	s64 i = n;
	char *pats[hlslen];
	for (; i < hlslen && !strcmp(hls[i].ft, name); i++)
		pats[i - n] = hls[i].pat;
	ftmap[fti].setbidx = n;
	ftmap[fti].ft = name;
	ftmap[fti].rs = rset_make(i - n, pats, 0);
	ftmap[fti].seteidx = i;
}

char *syn_setft(char *ft)
{
	for (s64 i = 1; i < 4; i++)
		syn_addhl(NULL, i, 0);
	for (s64 i = 0; i < ftmidx; i++)
		if (!strcmp(ft, ftmap[i].ft)) {
			ftidx = i;
			return ftmap[ftidx].ft;
		}
	for (s64 i = 0; i < hlslen; i++)
		if (!strcmp(ft, hls[i].ft)) {
			ftidx = ftmidx;
			syn_initft(ftmidx++, i, hls[i].ft);
			break;
		}
	return ftmap[ftidx].ft;
}

void syn_scdir(s64 scdir)
{
	if (last_scdir != scdir) {
		last_scdir = scdir;
		syn_blockhl = 0;
	}
}

s64 syn_merge(s64 old, s64 new)
{
	s64 fg = SYN_FGSET(new) ? SYN_FG(new) : SYN_FG(old);
	s64 bg = SYN_BGSET(new) ? SYN_BG(new) : SYN_BG(old);
	return ((old | new) & SYN_FLG) | (bg << 8) | fg;
}

void syn_highlight(s64 *att, char *s, s64 n)
{
	rset *rs = ftmap[ftidx].rs;
	s64 subs[rs->grpcnt * 2], sl;
	s64 blk = 0, blkm = 0, sidx = 0, flg = 0, hl, j, i;
	s64 bend = 0, cend = 0;
	while ((sl = rset_find(rs, s + sidx, subs, flg)) >= 0) {
		hl = sl + ftmap[ftidx].setbidx;
		s64 *catt = hls[hl].att;
		s64 blkend = hls[hl].blkend;
		if (blkend && sidx >= bend) {
			for (i = 0; i < rs->setgrpcnt[sl]; i++)
				if (subs[i * 2] >= 0)
					blk = i;
			blkm += blkm > labs(hls[hl].blkend) ? -1 : 1;
			if (blkm == 1 && last_scdir > 0)
				blkend = blkend < 0 ? -1 : 1;
			if (syn_blockhl == hl && blk == labs(blkend))
				syn_blockhl = 0;
			else if (!syn_blockhl && blk != blkend) {
				syn_blockhl = hl;
				blockatt = catt;
				blockcont = hls[hl].end[blk];
			} else
				blk = 0;
		}
		for (i = 0; i < rs->setgrpcnt[sl]; i++) {
			if (subs[i * 2] >= 0) {
				s64 beg = uc_off(s, sidx + subs[i * 2 + 0]);
				s64 end = uc_off(s, sidx + subs[i * 2 + 1]);
				for (j = beg; j < end; j++)
					att[j] = syn_merge(att[j], catt[i]);
				if (!hls[hl].end[i])
					cend = MAX(cend, subs[i * 2 + 1]);
				else {
					if (blkend)
						bend = MAX(cend, subs[i * 2 + 1]) + sidx;
					if (hls[hl].end[i] > 0)
						cend = MAX(cend, subs[i * 2]);
				}
			}
		}
		sidx += cend;
		cend = 1;
		flg = REG_NOTBOL;
	}
	if (syn_blockhl && !blk)
		for (j = 0; j < n; j++)
			att[j] = blockcont && att[j] ? att[j] : *blockatt;
}

char *syn_filetype(char *path)
{
	s64 hl = rset_find(syn_ftrs, path, NULL, 0);
	return hl >= 0 && hl < ftslen ? fts[hl].ft : hls[0].ft;
}

void syn_reloadft(void)
{
	if (syn_reload) {
		rset *rs = ftmap[ftidx].rs;
		syn_initft(ftidx, ftmap[ftidx].setbidx, ftmap[ftidx].ft);
		if (!ftmap[ftidx].rs) {
			ftmap[ftidx].rs = rs;
		} else
			rset_free(rs);
		syn_reload = 0;
	}
}

s64 syn_findhl(s64 id)
{
	for (s64 i = ftmap[ftidx].setbidx; i < ftmap[ftidx].seteidx; i++)
		if (hls[i].id == id)
			return i;
	return -1;
}

void syn_addhl(char *reg, s64 id, s64 reload)
{
	s64 ret = syn_findhl(id);
	if (ret >= 0) {
		hls[ret].pat = reg;
		syn_reload = reload;
	}
}

void syn_init(void)
{
	char *pats[ftslen];
	s64 i = 0;
	for (; i < ftslen; i++)
		pats[i] = fts[i].pat;
	syn_ftrs = rset_make(i, pats, 0);
}
