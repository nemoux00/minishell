#ifndef __MINIMISC_H__
#define __MINIMISC_H__

#include <nemoconfig.h>

#ifdef __cplusplus
NEMO_BEGIN_EXTERN_C
#endif

typedef enum {
	MINISHELL_NONE_SITE = 0,
	MINISHELL_TOP_SITE = 1,
	MINISHELL_BOTTOM_SITE = 2,
	MINISHELL_LEFT_SITE = 3,
	MINISHELL_RIGHT_SITE = 4,
	MINISHELL_LAST_SITE
} MiniShellSite;

extern int minishell_get_site(struct minishell *mini, double x, double y);
extern double minishell_get_site_rotation(struct minishell *mini, int site);
extern int minishell_get_edge(struct minishell *mini, double x, double y, uint32_t edgesize);

#ifdef __cplusplus
NEMO_END_EXTERN_C
#endif

#endif
