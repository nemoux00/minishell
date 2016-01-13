#ifndef __MINISHELL_TAP_H__
#define __MINISHELL_TAP_H__

#include <nemoconfig.h>

#ifdef __cplusplus
NEMO_BEGIN_EXTERN_C
#endif

#include <nemolist.h>
#include <nemolistener.h>

#include <minishell.h>
#include <showhelper.h>

typedef enum {
	MINISHELL_TAP_NORMAL_STATE = 0,
	MINISHELL_TAP_COLLISION_STATE = 1,
	MINISHELL_TAP_LAST_STATE
} MiniShellTapState;

struct minitap {
	struct minishell *mini;

	struct nemolist link;

	struct nemosignal destroy_signal;

	struct showone *one0;
	struct showone *one1;

	uint32_t state;

	double x, y;
};

extern struct minitap *minishell_tap_create(struct minishell *mini);
extern void minishell_tap_destroy(struct minitap *tap);

extern int minishell_tap_down(struct minishell *mini, struct minitap *tap, double x, double y);
extern int minishell_tap_motion(struct minishell *mini, struct minitap *tap, double x, double y);
extern int minishell_tap_up(struct minishell *mini, struct minitap *tap, double x, double y);

#ifdef __cplusplus
NEMO_END_EXTERN_C
#endif

#endif
