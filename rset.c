#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "regex.h"
#include "vi.h"

/* regular expression set */
struct rset {
	regex_t regex;		/* the combined regular expression */
	int n;			/* number of regular expressions in this set */
	int *grp;		/* the group assigned to each subgroup */
	int *setgrpcnt;		/* number of groups in each regular expression */
	int grpcnt;		/* group count */
};

static int re_groupcount(char *s)
{
	int n = 0;
	while (*s) {
		if (s[0] == '(')
			n++;
		if (s[0] == '[') {
			int dep = 0;
			s += s[1] == '^' ? 3 : 2;
			while (s[0] && (s[0] != ']' || dep)) {
				if (s[0] == '[')
					dep++;
				if (s[0] == ']')
					dep--;
				s++;
			}
		}
		if (s[0] == '\\' && s[1])
			s++;
		s++;
	}
	return n;
}

struct rset *rset_make(int n, char **re, int flg)
{
	struct rset *rs = malloc(sizeof(*rs));
	struct sbuf *sb = sbuf_make();
	int regex_flg = REG_EXTENDED | (flg & RE_ICASE ? REG_ICASE : 0);
	int i;
	memset(rs, 0, sizeof(*rs));
	rs->grp = malloc((n + 1) * sizeof(rs->grp[0]));
	rs->setgrpcnt = malloc((n + 1) * sizeof(rs->setgrpcnt[0]));
	rs->grpcnt = 2;
	rs->n = n;
	sbuf_chr(sb, '(');
	for (i = 0; i < n; i++) {
		if (!re[i]) {
			rs->grp[i] = -1;
			rs->setgrpcnt[i] = 0;
			continue;
		}
		if (sbuf_len(sb) > 1)
			sbuf_chr(sb, '|');
		sbuf_chr(sb, '(');
		sbuf_str(sb, re[i]);
		sbuf_chr(sb, ')');
		rs->grp[i] = rs->grpcnt;
		rs->setgrpcnt[i] = re_groupcount(re[i]);
		rs->grpcnt += 1 + rs->setgrpcnt[i];
	}
	rs->grp[n] = rs->grpcnt;
	sbuf_chr(sb, ')');
	if (regcomp(&rs->regex, sbuf_buf(sb), regex_flg)) {
		free(rs->grp);
		free(rs->setgrpcnt);
		free(rs);
		sbuf_free(sb);
		return NULL;
	}
	sbuf_free(sb);
	return rs;
}

/* return the index of the matching regular expression or -1 if none matches */
int rset_find(struct rset *rs, char *s, int n, int *grps, int flg)
{
	int found, i, set = -1;
	int regex_flg = 0;
	if (rs->grpcnt <= 2)
		return -1;
	if (flg & RE_NOTBOL)
		regex_flg |= REG_NOTBOL;
	if (flg & RE_NOTEOL)
		regex_flg |= REG_NOTEOL;
	regmatch_t subs[rs->grpcnt];
	found = !regexec(&rs->regex, s, rs->grpcnt, subs, regex_flg);
	for (i = 0; found && i < rs->n; i++)
		if (rs->grp[i] >= 0 && subs[rs->grp[i]].rm_so >= 0)
			set = i;
	if (found && set >= 0) {
		for (i = 0; i < n; i++) {
			int grp = rs->grp[set] + i;
			if (i < rs->setgrpcnt[set] + 1) {
				grps[i * 2] = subs[grp].rm_so;
				grps[i * 2 + 1] = subs[grp].rm_eo;
			} else {
				grps[i * 2 + 0] = -1;
				grps[i * 2 + 1] = -1;
			}
		}
	}
	return set;
}

void rset_free(struct rset *rs)
{
	regfree(&rs->regex);
	free(rs->setgrpcnt);
	free(rs->grp);
	free(rs);
}

/* read a regular expression enclosed in a delimiter */
char *re_read(char **src)
{
	struct sbuf *sbuf = sbuf_make();
	char *s = *src;
	int delim = (unsigned char) *s++;
	if (!delim)
		return NULL;
	while (*s && *s != delim) {
		if (s[0] == '\\' && s[1])
			if (*(++s) != delim)
				sbuf_chr(sbuf, '\\');
		sbuf_chr(sbuf, (unsigned char) *s++);
	}
	*src = *s ? s + 1 : s;
	return sbuf_done(sbuf);
}
