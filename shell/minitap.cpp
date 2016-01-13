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

#include <minitap.h>
#include <minishell.h>
#include <talehelper.h>
#include <showhelper.h>
#include <nemolog.h>
#include <nemomisc.h>

struct minitap *minishell_tap_create(struct minishell *mini)
{
	struct nemoshell *shell = mini->shell;
	struct nemocompz *compz = shell->compz;
	struct minitap *tap;

	tap = (struct minitap *)malloc(sizeof(struct minitap));
	if (tap == NULL)
		return NULL;
	memset(tap, 0, sizeof(struct minitap));

	tap->mini = mini;

	nemolist_insert(&mini->tap_list, &tap->link);

	nemosignal_init(&tap->destroy_signal);

	return tap;
}

void minishell_tap_destroy(struct minitap *tap)
{
	nemolist_remove(&tap->link);

	nemosignal_emit(&tap->destroy_signal, tap);

	free(tap);
}

int minishell_tap_down(struct minishell *mini, struct minitap *tap, double x, double y)
{
	struct showone *one;
	struct showtransition *trans0, *trans1;
	struct showone *sequence;
	struct showone *set0, *set1;
	int attr;

	tap->x = x;
	tap->y = y;

	tap->one0 = one = nemoshow_item_create(NEMOSHOW_PATH_ITEM);
	nemoshow_attach_one(mini->show, one);
	nemoshow_one_attach(mini->canvas, one);
	nemoshow_item_set_x(one, 0.0f);
	nemoshow_item_set_y(one, 0.0f);
	nemoshow_item_set_width(one, mini->tapsize);
	nemoshow_item_set_height(one, mini->tapsize);
	nemoshow_item_set_fill_color(one, 0x1e, 0xdc, 0xdc, 0xff);
	nemoshow_item_set_filter(one, mini->solid);
	nemoshow_item_set_tsr(one);
	nemoshow_item_pivot(one, mini->tapsize / 2.0f, mini->tapsize / 2.0f);
	nemoshow_item_translate(one, x - mini->tapsize / 2.0f, y - mini->tapsize / 2.0f);
	nemoshow_item_scale(one, 0.0f, 0.0f);
	nemoshow_item_load_svg(one, MINISHELL_RESOURCES "/misc-icons/touch-1.svg");

	tap->one1 = one = nemoshow_item_create(NEMOSHOW_PATH_ITEM);
	nemoshow_attach_one(mini->show, one);
	nemoshow_one_attach(mini->canvas, one);
	nemoshow_item_set_x(one, 0.0f);
	nemoshow_item_set_y(one, 0.0f);
	nemoshow_item_set_width(one, mini->tapsize);
	nemoshow_item_set_height(one, mini->tapsize);
	nemoshow_item_set_fill_color(one, 0x1e, 0xdc, 0xdc, 0xff);
	nemoshow_item_set_filter(one, mini->solid);
	nemoshow_item_set_alpha(one, 0.0f);
	nemoshow_item_set_tsr(one);
	nemoshow_item_pivot(one, mini->tapsize / 2.0f, mini->tapsize / 2.0f);
	nemoshow_item_translate(one, x - mini->tapsize / 2.0f, y - mini->tapsize / 2.0f);
	nemoshow_item_scale(one, 0.9f, 0.9f);
	nemoshow_item_load_svg(one, MINISHELL_RESOURCES "/misc-icons/touch-2.svg");

	set0 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set0, tap->one0);
	nemoshow_sequence_set_dattr(set0, "sx", 1.0f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set0, "sy", 1.0f, NEMOSHOW_MATRIX_DIRTY);

	set1 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set1, tap->one1);
	nemoshow_sequence_set_dattr(set1, "alpha", 1.0f, NEMOSHOW_STYLE_DIRTY);

	sequence = nemoshow_sequence_create_easy(mini->show,
			nemoshow_sequence_create_frame_easy(mini->show,
				1.0f, set0, set1, NULL),
			NULL);

	trans0 = nemoshow_transition_create(mini->ease1, 800, 0);
	nemoshow_transition_check_one(trans0, tap->one0);
	nemoshow_transition_check_one(trans0, tap->one1);
	nemoshow_transition_attach_sequence(trans0, sequence);
	nemoshow_attach_transition(mini->show, trans0);

	set0 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set0, tap->one1);
	attr = nemoshow_sequence_set_dattr(set0, "ro", 360.0f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_fix_dattr(set0, attr, 0.0f);

	sequence = nemoshow_sequence_create_easy(mini->show,
			nemoshow_sequence_create_frame_easy(mini->show,
				1.0f, set0, NULL),
			NULL);

	trans1 = nemoshow_transition_create(mini->ease2, 3000, 0);
	nemoshow_transition_check_one(trans1, tap->one1);
	nemoshow_transition_attach_sequence(trans1, sequence);
	nemoshow_transition_set_repeat(trans1, 0);
	nemoshow_attach_transition_after(mini->show, trans0, trans1);

	return 0;
}

int minishell_tap_motion(struct minishell *mini, struct minitap *tap, double x, double y)
{
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0, *set1;
	struct minitap *otap;
	int has_collision = 0;
	double dx, dy;

	tap->x = x;
	tap->y = y;

	nemoshow_item_translate(tap->one0, x - mini->tapsize / 2.0f, y - mini->tapsize / 2.0f);
	nemoshow_item_translate(tap->one1, x - mini->tapsize / 2.0f, y - mini->tapsize / 2.0f);

	nemolist_for_each(otap, &mini->tap_list, link) {
		if (otap == tap)
			continue;

		dx = otap->x - tap->x;
		dy = otap->y - tap->y;

		if (sqrtf(dx * dx + dy * dy) < mini->tapsize) {
			has_collision = 1;
			break;
		}
	}

	if (has_collision == 0 && tap->state != MINISHELL_TAP_NORMAL_STATE) {
		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, tap->one0);
		nemoshow_sequence_set_cattr(set0, "fill", 0x1e, 0xdc, 0xdc, 0xff, NEMOSHOW_STYLE_DIRTY);

		set1 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set1, tap->one1);
		nemoshow_sequence_set_cattr(set1, "fill", 0x1e, 0xdc, 0xdc, 0xff, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set1, "sx", 0.9f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set1, "sy", 0.9f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, set1, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease1, 500, 0);
		nemoshow_transition_check_one(trans, tap->one0);
		nemoshow_transition_check_one(trans, tap->one1);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);

		tap->state = MINISHELL_TAP_NORMAL_STATE;
	} else if (has_collision != 0 && tap->state != MINISHELL_TAP_COLLISION_STATE) {
		set0 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set0, tap->one0);
		nemoshow_sequence_set_cattr(set0, "fill", 0xff, 0x8c, 0x32, 0xff, NEMOSHOW_STYLE_DIRTY);

		set1 = nemoshow_sequence_create_set();
		nemoshow_sequence_set_source(set1, tap->one1);
		nemoshow_sequence_set_cattr(set1, "fill", 0xff, 0x8c, 0x32, 0xff, NEMOSHOW_STYLE_DIRTY);
		nemoshow_sequence_set_dattr(set1, "sx", 0.5f, NEMOSHOW_MATRIX_DIRTY);
		nemoshow_sequence_set_dattr(set1, "sy", 0.5f, NEMOSHOW_MATRIX_DIRTY);

		sequence = nemoshow_sequence_create_easy(mini->show,
				nemoshow_sequence_create_frame_easy(mini->show,
					1.0f, set0, set1, NULL),
				NULL);

		trans = nemoshow_transition_create(mini->ease1, 500, 0);
		nemoshow_transition_check_one(trans, tap->one0);
		nemoshow_transition_check_one(trans, tap->one1);
		nemoshow_transition_attach_sequence(trans, sequence);
		nemoshow_attach_transition(mini->show, trans);

		tap->state = MINISHELL_TAP_COLLISION_STATE;
	}

	return 0;
}

static void minishell_tap_dispatch_destroy_done(void *data)
{
	struct minitap *tap = (struct minitap *)data;

	nemoshow_one_destroy(tap->one0);
	nemoshow_one_destroy(tap->one1);

	minishell_tap_destroy(tap);
}

int minishell_tap_up(struct minishell *mini, struct minitap *tap, double x, double y)
{
	struct showtransition *trans;
	struct showone *sequence;
	struct showone *set0, *set1;

	set0 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set0, tap->one0);
	nemoshow_sequence_set_dattr(set0, "sx", 0.0f, NEMOSHOW_MATRIX_DIRTY);
	nemoshow_sequence_set_dattr(set0, "sy", 0.0f, NEMOSHOW_MATRIX_DIRTY);

	set1 = nemoshow_sequence_create_set();
	nemoshow_sequence_set_source(set1, tap->one1);
	nemoshow_sequence_set_dattr(set1, "alpha", 0.0f, NEMOSHOW_STYLE_DIRTY);

	sequence = nemoshow_sequence_create_easy(mini->show,
			nemoshow_sequence_create_frame_easy(mini->show,
				1.0f, set0, set1, NULL),
			NULL);

	trans = nemoshow_transition_create(mini->ease0, 700, 0);
	nemoshow_transition_set_dispatch_done(trans, minishell_tap_dispatch_destroy_done);
	nemoshow_transition_set_userdata(trans, tap);
	nemoshow_transition_check_one(trans, tap->one0);
	nemoshow_transition_check_one(trans, tap->one1);
	nemoshow_transition_attach_sequence(trans, sequence);
	nemoshow_attach_transition(mini->show, trans);

	return 0;
}
