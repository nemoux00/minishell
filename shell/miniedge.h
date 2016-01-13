#ifndef __MINISHELL_EDGE_H__
#define __MINISHELL_EDGE_H__

#include <nemoconfig.h>

#ifdef __cplusplus
NEMO_BEGIN_EXTERN_C
#endif

#include <nemolist.h>
#include <nemolistener.h>

#include <minishell.h>
#include <showhelper.h>

typedef enum {
	MINISHELL_EDGE_READY_STATE = 0,
	MINISHELL_EDGE_ACTIVE_STATE = 1,
	MINISHELL_EDGE_DONE_STATE = 2,
	MINISHELL_EDGE_LAST_STATE
} MiniShellEdgeState;

struct nemotimer;

struct miniedge {
	struct minishell *mini;

	struct nemosignal destroy_signal;

	struct nemotimer *timer;

	struct showone *grouprings[MINISHELL_GROUP_MAX];
	struct showone *groups[MINISHELL_GROUP_MAX];
	int ngroups;

	struct showone *itemrings[MINISHELL_ITEM_MAX];
	struct showone *items[MINISHELL_ITEM_MAX];
	int nitems;

	int site;

	uint32_t state;
	uint32_t refs;

	int groupidx;
	int itemidx;

	double r;

	double x0, y0;
	double x1, y1;
};

extern struct miniedge *minishell_edge_create(struct minishell *mini, int site);
extern void minishell_edge_destroy(struct miniedge *edge);

extern int minishell_edge_shutdown(struct minishell *mini, struct miniedge *edge);

extern int minishell_edge_down(struct minishell *mini, struct miniedge *edge, double x, double y);
extern int minishell_edge_motion(struct minishell *mini, struct miniedge *edge, double x, double y);
extern int minishell_edge_up(struct minishell *mini, struct miniedge *edge, double x, double y);

extern int minishell_edge_activate_group(struct minishell *mini, struct miniedge *edge, uint32_t group);
extern int minishell_edge_deactivate_group(struct minishell *mini, struct miniedge *edge);

extern int minishell_edge_activate_item(struct minishell *mini, struct miniedge *edge, uint32_t item);
extern int minishell_edge_deactivate_item(struct minishell *mini, struct miniedge *edge);

static inline void minishell_edge_reference(struct miniedge *edge)
{
	edge->refs++;
}

static inline uint32_t minishell_edge_unreference(struct miniedge *edge)
{
	return --edge->refs;
}

#ifdef __cplusplus
NEMO_END_EXTERN_C
#endif

#endif
