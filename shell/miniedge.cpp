#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <float.h>
#include <wayland-server.h>

#include <compz.h>
#include <shell.h>
#include <actor.h>
#include <timer.h>

#include <miniedge.h>
#include <minishell.h>
#include <minimisc.h>
#include <talehelper.h>
#include <showhelper.h>
#include <nemolog.h>
#include <nemomisc.h>

struct miniedge *minishell_edge_create(struct minishell *mini, int site)
{
	struct nemoshell *shell = mini->shell;
	struct nemocompz *compz = shell->compz;
	struct miniedge *edge;

	edge = (struct miniedge *)malloc(sizeof(struct miniedge));
	if (edge == NULL)
		return NULL;
	memset(edge, 0, sizeof(struct miniedge));

	edge->mini = mini;

	edge->site = site;

	edge->groupidx = -1;
	edge->itemidx = -1;

	nemosignal_init(&edge->destroy_signal);

	return edge;
}

void minishell_edge_destroy(struct miniedge *edge)
{
	int i;

	nemosignal_emit(&edge->destroy_signal, edge);

	if (edge->timer != NULL)
		nemotimer_destroy(edge->timer);

	for (i = 0; i < edge->ngroups; i++) {
		nemoshow_one_destroy(edge->grouprings[i]);

		if (edge->groups[i] != NULL)
			nemoshow_one_destroy(edge->groups[i]);
	}

	free(edge);
}

static void minishell_edge_dispatch_destroy_done(void *data)
{
	struct miniedge *edge = (struct miniedge *)data;

	if (minishell_edge_unreference(edge) == 0)
		minishell_edge_destroy(edge);
}

static void minishell_edge_show_rings(struct minishell *mini, struct miniedge *edge, uint32_t ngroups)
{
	struct showone *one;
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0;
	int i;

	for (i = edge->ngroups; i < ngroups; i++) {
		const char *ring = mini->groups[i]->ring;

		edge->grouprings[i] = one = nemoshow_item_create(NEMOSHOW_PATH_ITEM);
		nemoshow_attach_one(mini->show, one);
		nemoshow_one_attach(mini->canvas, one);
		nemoshow_item_set_x(one, 0.0f);
		nemoshow_item_set_y(one, 0.0f);
		nemoshow_item_set_width(one, mini->edgesize);
		nemoshow_item_set_height(one, mini->edgesize);
		nemoshow_item_set_fill_color(one, 0x1e, 0xdc, 0xdc, 0xff);
		nemoshow_item_set_filter(one, mini->solid);
		nemoshow_item_set_tsr(one);
		nemoshow_item_pivot(one, mini->edgesize / 2.0f, mini->edgesize / 2.0f);
		nemoshow_item_scale(one, 0.0f, 0.0f);
		nemoshow_item_rotate(one, edge->r);
		nemoshow_item_load_svg(one, ring);

		if (edge->site == MINISHELL_TOP_SITE) {
			nemoshow_item_translate(one,
					(edge->x1 + edge->x0) / 2.0f - mini->edgesize / 2.0f,
					(i + 0) * mini->edgesize);
		} else if (edge->site == MINISHELL_BOTTOM_SITE) {
			nemoshow_item_translate(one,
					(edge->x1 + edge->x0) / 2.0f - mini->edgesize / 2.0f,
					mini->height - (i + 1) * mini->edgesize);
		} else if (edge->site == MINISHELL_LEFT_SITE) {
			nemoshow_item_translate(one,
					(i + 0) * mini->edgesize,
					(edge->y1 + edge->y0) / 2.0f - mini->edgesize / 2.0f);
		} else if (edge->site == MINISHELL_RIGHT_SITE) {
			nemoshow_item_translate(one,
					mini->width - (i + 1) * mini->edgesize,
					(edge->y1 + edge->y0) / 2.0f - mini->edgesize / 2.0f);
		}

		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, one);
		nemoshow_sequence_set_dattr(set0, "sx", 1.0f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sy", 1.0f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease1, 700, 0);
		nemoshow_transition_check_one(trans, one);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);
	}

	edge->ngroups = i;
}

static void minishell_edge_hide_rings(struct minishell *mini, struct miniedge *edge, int needs_destroy)
{
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0;
	int i;

	for (i = 0; i < edge->ngroups; i++) {
		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, edge->grouprings[i]);
		nemoshow_sequence_set_dattr(set0, "alpha", 0.0f, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sx", 0.3f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sy", 0.3f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease0, 700, 0);
		if (needs_destroy != 0) {
			minishell_edge_reference(edge);

			nemoshow_transition_set_dispatch_done(trans, minishell_edge_dispatch_destroy_done);
			nemoshow_transition_set_userdata(trans, edge);
		}
		nemoshow_transition_check_one(trans, edge->grouprings[i]);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);
	}
}

static void minishell_edge_show_groups(struct minishell *mini, struct miniedge *edge)
{
	struct showone *one;
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0;
	int i;

	for (i = 0; i < edge->ngroups; i++) {
		const char *icon = mini->groups[i]->icon;

		edge->groups[i] = one = nemoshow_item_create(NEMOSHOW_PATH_ITEM);
		nemoshow_attach_one(mini->show, one);
		nemoshow_one_attach(mini->canvas, one);
		nemoshow_one_set_tag(one, 1000 + i);
		nemoshow_one_set_userdata(one, edge);
		nemoshow_item_set_x(one, 0.0f);
		nemoshow_item_set_y(one, 0.0f);
		nemoshow_item_set_width(one, mini->edgesize);
		nemoshow_item_set_height(one, mini->edgesize);
		nemoshow_item_set_fill_color(one, 0x1e, 0xdc, 0xdc, 0xff);
		nemoshow_item_set_filter(one, mini->solid);
		nemoshow_item_set_tsr(one);
		nemoshow_item_pivot(one, mini->edgesize / 2.0f, mini->edgesize / 2.0f);
		nemoshow_item_scale(one, 0.7f, 0.7f);
		nemoshow_item_rotate(one, edge->r);
		nemoshow_item_load_svg(one, icon);
		nemoshow_item_set_alpha(one, 0.0f);

		if (edge->site == MINISHELL_TOP_SITE) {
			nemoshow_item_translate(one,
					(edge->x1 + edge->x0) / 2.0f - mini->edgesize / 2.0f,
					(i + 0) * mini->edgesize);
		} else if (edge->site == MINISHELL_BOTTOM_SITE) {
			nemoshow_item_translate(one,
					(edge->x1 + edge->x0) / 2.0f - mini->edgesize / 2.0f,
					mini->height - (i + 1) * mini->edgesize);
		} else if (edge->site == MINISHELL_LEFT_SITE) {
			nemoshow_item_translate(one,
					(i + 0) * mini->edgesize,
					(edge->y1 + edge->y0) / 2.0f - mini->edgesize / 2.0f);
		} else if (edge->site == MINISHELL_RIGHT_SITE) {
			nemoshow_item_translate(one,
					mini->width - (i + 1) * mini->edgesize,
					(edge->y1 + edge->y0) / 2.0f - mini->edgesize / 2.0f);
		}

		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, one);
		nemoshow_sequence_set_dattr(set0, "alpha", 1.0f, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sx", 1.0f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sy", 1.0f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease1, 700, 300);
		nemoshow_transition_check_one(trans, one);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);
	}
}

static void minishell_edge_hide_groups(struct minishell *mini, struct miniedge *edge, int needs_destroy)
{
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0;
	int i;

	for (i = 0; i < edge->ngroups; i++) {
		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, edge->groups[i]);
		nemoshow_sequence_set_dattr(set0, "alpha", 0.0f, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sx", 0.7f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sy", 0.7f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease0, 700, 0);
		if (needs_destroy != 0) {
			minishell_edge_reference(edge);

			nemoshow_transition_set_dispatch_done(trans, minishell_edge_dispatch_destroy_done);
			nemoshow_transition_set_userdata(trans, edge);
		}
		nemoshow_transition_check_one(trans, edge->groups[i]);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);
	}
}

int minishell_edge_shutdown(struct minishell *mini, struct miniedge *edge)
{
	minishell_edge_deactivate_group(mini, edge);

	minishell_edge_hide_rings(mini, edge, 1);
	minishell_edge_hide_groups(mini, edge, 1);

	nemoactor_dispatch_frame(mini->actor);

	nemotimer_set_timeout(edge->timer, 0);

	edge->state = MINISHELL_EDGE_DONE_STATE;

	return 0;
}

int minishell_edge_down(struct minishell *mini, struct miniedge *edge, double x, double y)
{
	if (edge->site == MINISHELL_TOP_SITE) {
		edge->x0 = x;
		edge->y0 = 0.0f;
		edge->x1 = edge->x0;
		edge->y1 = y;

		edge->r = 180.0f;
	} else if (edge->site == MINISHELL_BOTTOM_SITE) {
		edge->x0 = x;
		edge->y0 = y;
		edge->x1 = edge->x0;
		edge->y1 = mini->height;

		edge->r = 0.0f;
	} else if (edge->site == MINISHELL_LEFT_SITE) {
		edge->x0 = 0.0f;
		edge->y0 = y;
		edge->x1 = x;
		edge->y1 = edge->y0;

		edge->r = 90.0f;
	} else if (edge->site == MINISHELL_RIGHT_SITE) {
		edge->x0 = x;
		edge->y0 = y;
		edge->x1 = mini->width;
		edge->y1 = edge->y0;

		edge->r = -90.0f;
	}

	return 0;
}

static void minishell_edge_dispatch_timer(struct nemotimer *timer, void *data)
{
	struct miniedge *edge = (struct miniedge *)data;
	struct minishell *mini = edge->mini;

	minishell_edge_shutdown(mini, edge);
}

int minishell_edge_motion(struct minishell *mini, struct miniedge *edge, double x, double y)
{
	if (edge->state == MINISHELL_EDGE_READY_STATE) {
		double size;

		if (edge->site == MINISHELL_TOP_SITE) {
			edge->y1 = y;

			size = edge->y1 - edge->y0;
		} else if (edge->site == MINISHELL_BOTTOM_SITE) {
			edge->y0 = y;

			size = edge->y1 - edge->y0;
		} else if (edge->site == MINISHELL_LEFT_SITE) {
			edge->x1 = x;

			size = edge->x1 - edge->x0;
		} else if (edge->site == MINISHELL_RIGHT_SITE) {
			edge->x0 = x;

			size = edge->x1 - edge->x0;
		}

		minishell_edge_show_rings(mini, edge, MIN(floor(size / mini->edgesize) + 1, mini->ngroups));

		if (edge->ngroups >= mini->ngroups) {
			struct nemotimer *timer;

			edge->timer = timer = nemotimer_create(mini->compz);
			nemotimer_set_callback(timer, minishell_edge_dispatch_timer);
			nemotimer_set_userdata(timer, edge);
			nemotimer_set_timeout(timer, mini->edgetimeout);

			edge->state = MINISHELL_EDGE_ACTIVE_STATE;

			minishell_edge_show_groups(mini, edge);
		}
	} else if (edge->state == MINISHELL_EDGE_ACTIVE_STATE) {
	}

	return 0;
}

int minishell_edge_up(struct minishell *mini, struct miniedge *edge, double x, double y)
{
	if (edge->state == MINISHELL_EDGE_READY_STATE) {
		minishell_edge_hide_rings(mini, edge, 1);
	}

	return 0;
}

int minishell_edge_activate_group(struct minishell *mini, struct miniedge *edge, uint32_t group)
{
	struct showone *one;
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0, *set1;
	double size;
	double x, y;
	int dd = group % 2 == 0 ? -1 : 1;
	int dx = 0, dy = 0;
	int i;

	if (edge->state != MINISHELL_EDGE_ACTIVE_STATE)
		return 0;

	size = mini->groups[group]->nitems * mini->edgesize;

	if (edge->site == MINISHELL_TOP_SITE) {
		x = (edge->x1 + edge->x0) / 2.0f - mini->edgesize / 2.0f;
		y = (group + 0) * mini->edgesize;

		dx = -1 * dd;
	} else if (edge->site == MINISHELL_BOTTOM_SITE) {
		x = (edge->x1 + edge->x0) / 2.0f - mini->edgesize / 2.0f;
		y = mini->height - (group + 1) * mini->edgesize;

		dx = 1 * dd;
	} else if (edge->site == MINISHELL_LEFT_SITE) {
		x = (group + 0) * mini->edgesize;
		y = (edge->y1 + edge->y0) / 2.0f - mini->edgesize / 2.0f;

		dy = 1 * dd;
	} else if (edge->site == MINISHELL_RIGHT_SITE) {
		x = mini->width - (group + 1) * mini->edgesize;
		y = (edge->y1 + edge->y0) / 2.0f - mini->edgesize / 2.0f;

		dy = -1 * dd;
	}

	if (dx > 0 && x + mini->edgesize / 2.0f > mini->width - size)
		dx *= -1;
	if (dx < 0 && x + mini->edgesize / 2.0f < size)
		dx *= -1;
	if (dy > 0 && y + mini->edgesize / 2.0f > mini->height - size)
		dy *= -1;
	if (dy < 0 && y + mini->edgesize / 2.0f < size)
		dy *= -1;

	set0 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set0, edge->grouprings[group]);
	nemoshow_sequence_set_dattr(set0, "sx", 0.8f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set0, "sy", 0.8f, NEMOSHOW_MATRIX_DIRTY);

	set1 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set1, edge->groups[group]);
	nemoshow_sequence_set_dattr(set1, "sx", 0.8f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set1, "sy", 0.8f, NEMOSHOW_MATRIX_DIRTY);

	sequence = nemoshow_sequence_create_easy(mini->show,
			nemoshow_sequence_create_frame_easy(mini->show,
				1.0f, set0, set1, NULL),
			NULL);

	trans = nemoshow_transition_create(mini->ease1, 500, 0);
	nemoshow_transition_check_one(trans, edge->grouprings[group]);
	nemoshow_transition_check_one(trans, edge->groups[group]);
	nemoshow_transition_attach_sequence(trans, sequence);
	nemoshow_attach_transition(mini->show, trans);

	for (i = 0; i < mini->groups[group]->nitems; i++) {
		const char *ring = mini->groups[group]->items[i]->ring;
		const char *icon = mini->groups[group]->items[i]->icon;

		edge->itemrings[i] = one = nemoshow_item_create(NEMOSHOW_PATH_ITEM);
		nemoshow_attach_one(mini->show, one);
		nemoshow_one_attach(mini->canvas, one);
		nemoshow_one_set_tag(one, 2000 + i);
		nemoshow_item_set_x(one, 0.0f);
		nemoshow_item_set_y(one, 0.0f);
		nemoshow_item_set_width(one, mini->edgesize);
		nemoshow_item_set_height(one, mini->edgesize);
		nemoshow_item_set_fill_color(one, 0x1e, 0xdc, 0xdc, 0xff);
		nemoshow_item_set_filter(one, mini->solid);
		nemoshow_item_set_tsr(one);
		nemoshow_item_pivot(one, mini->edgesize / 2.0f, mini->edgesize / 2.0f);
		nemoshow_item_scale(one, 0.3f, 0.3f);
		nemoshow_item_rotate(one, edge->r);
		nemoshow_item_load_svg(one, ring);
		nemoshow_item_set_alpha(one, 0.0f);

		nemoshow_item_translate(one,
				x + (i + 1) * mini->edgesize * dx,
				y + (i + 1) * mini->edgesize * dy);

		edge->items[i] = one = nemoshow_item_create(NEMOSHOW_PATH_ITEM);
		nemoshow_attach_one(mini->show, one);
		nemoshow_one_attach(mini->canvas, one);
		nemoshow_one_set_tag(one, 2000 + i);
		nemoshow_item_set_x(one, 0.0f);
		nemoshow_item_set_y(one, 0.0f);
		nemoshow_item_set_width(one, mini->edgesize);
		nemoshow_item_set_height(one, mini->edgesize);
		nemoshow_item_set_fill_color(one, 0x1e, 0xdc, 0xdc, 0xff);
		nemoshow_item_set_filter(one, mini->solid);
		nemoshow_item_set_tsr(one);
		nemoshow_item_pivot(one, mini->edgesize / 2.0f, mini->edgesize / 2.0f);
		nemoshow_item_scale(one, 0.6f, 0.1f);
		nemoshow_item_rotate(one, edge->r);
		nemoshow_item_load_svg(one, icon);
		nemoshow_item_set_alpha(one, 0.0f);

		nemoshow_item_translate(one,
				x + (i + 1) * mini->edgesize * dx,
				y + (i + 1) * mini->edgesize * dy);

		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, edge->itemrings[i]);
		nemoshow_sequence_set_dattr(set0, "alpha", 1.0f, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sx", 1.0f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sy", 1.0f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease1, 500, i * 100);
		nemoshow_transition_check_one(trans, edge->itemrings[i]);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);

		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, edge->items[i]);
		nemoshow_sequence_set_dattr(set0, "alpha", 1.0f, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sx", 0.8f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sy", 0.8f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease1, 300, i * 100 + 200);
		nemoshow_transition_check_one(trans, edge->items[i]);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);
	}

	nemoactor_dispatch_frame(mini->actor);

	nemotimer_set_timeout(edge->timer, 0);

	edge->groupidx = group;
	edge->nitems = i;

	return 0;
}

int minishell_edge_deactivate_group(struct minishell *mini, struct miniedge *edge)
{
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0, *set1;
	int group = edge->groupidx;
	int i;

	if (group < 0)
		return 0;

	set0 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set0, edge->grouprings[group]);
	nemoshow_sequence_set_dattr(set0, "sx", 1.0f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set0, "sy", 1.0f, NEMOSHOW_MATRIX_DIRTY);

	set1 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set1, edge->groups[group]);
	nemoshow_sequence_set_dattr(set1, "sx", 1.0f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set1, "sy", 1.0f, NEMOSHOW_MATRIX_DIRTY);

	sequence = nemoshow_sequence_create_easy(mini->show,
			nemoshow_sequence_create_frame_easy(mini->show,
				1.0f, set0, set1, NULL),
			NULL);

	trans = nemoshow_transition_create(mini->ease0, 500, 0);
	nemoshow_transition_check_one(trans, edge->grouprings[group]);
	nemoshow_transition_check_one(trans, edge->groups[group]);
	nemoshow_transition_attach_sequence(trans, sequence);
	nemoshow_attach_transition(mini->show, trans);

	for (i = 0; i < edge->nitems; i++) {
		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, edge->itemrings[i]);
		nemoshow_sequence_set_dattr(set0, "alpha", 0.0f, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sx", 0.4f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set0, "sy", 0.4f, NEMOSHOW_MATRIX_DIRTY);

		set1 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set1, edge->items[i]);
		nemoshow_sequence_set_dattr(set1, "alpha", 0.0f, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set1, "sx", 0.2f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set1, "sy", 0.2f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, set1, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease0, 500, 0);
		nemoshow_transition_check_one(trans, edge->itemrings[i]);
		nemoshow_transition_check_one(trans, edge->items[i]);
		nemoshow_transition_destroy_one(trans, edge->itemrings[i]);
		nemoshow_transition_destroy_one(trans, edge->items[i]);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);
	}

	nemoactor_dispatch_frame(mini->actor);

	nemotimer_set_timeout(edge->timer, mini->edgetimeout);

	edge->groupidx = -1;
	edge->itemidx = -1;

	return 0;
}

int minishell_edge_activate_item(struct minishell *mini, struct miniedge *edge, uint32_t item)
{
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0, *set1;

	if (item < 0)
		return 0;

	set0 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set0, edge->itemrings[item]);
	nemoshow_sequence_set_dattr(set0, "sx", 0.8f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set0, "sy", 0.8f, NEMOSHOW_MATRIX_DIRTY);

	set1 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set1, edge->items[item]);
	nemoshow_sequence_set_dattr(set1, "sx", 0.6f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set1, "sy", 0.6f, NEMOSHOW_MATRIX_DIRTY);

	sequence = nemoshow_sequence_create_easy(mini->show,
			nemoshow_sequence_create_frame_easy(mini->show,
				1.0f, set0, set1, NULL),
			NULL);

	trans = nemoshow_transition_create(mini->ease1, 300, 0);
	nemoshow_transition_check_one(trans, edge->itemrings[item]);
	nemoshow_transition_check_one(trans, edge->items[item]);
	nemoshow_transition_attach_sequence(trans, sequence);
	nemoshow_attach_transition(mini->show, trans);

	nemoactor_dispatch_frame(mini->actor);

	edge->itemidx = item;

	return 0;
}

int minishell_edge_deactivate_item(struct minishell *mini, struct miniedge *edge)
{
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0, *set1;
	int item = edge->itemidx;

	if (item < 0)
		return 0;

	set0 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set0, edge->itemrings[item]);
	nemoshow_sequence_set_dattr(set0, "sx", 1.0f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set0, "sy", 1.0f, NEMOSHOW_MATRIX_DIRTY);

	set1 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set1, edge->items[item]);
	nemoshow_sequence_set_dattr(set1, "sx", 0.8f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set1, "sy", 0.8f, NEMOSHOW_MATRIX_DIRTY);

	sequence = nemoshow_sequence_create_easy(mini->show,
			nemoshow_sequence_create_frame_easy(mini->show,
				1.0f, set0, set1, NULL),
			NULL);

	trans = nemoshow_transition_create(mini->ease0, 300, 0);
	nemoshow_transition_check_one(trans, edge->itemrings[item]);
	nemoshow_transition_check_one(trans, edge->items[item]);
	nemoshow_transition_attach_sequence(trans, sequence);
	nemoshow_attach_transition(mini->show, trans);

	nemoactor_dispatch_frame(mini->actor);

	edge->itemidx = -1;

	return 0;
}
