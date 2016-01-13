#define __USE_GNU
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <getopt.h>
#include <linux/input.h>
#include <wayland-server.h>

#include <shell.h>
#include <compz.h>
#include <screen.h>
#include <view.h>
#include <content.h>
#include <actor.h>
#include <canvas.h>
#include <subcanvas.h>
#include <seat.h>
#include <keyboard.h>
#include <pointer.h>
#include <touch.h>
#include <virtuio.h>
#include <datadevice.h>
#include <session.h>
#include <binding.h>
#include <plugin.h>
#include <xserver.h>
#include <timer.h>
#include <animation.h>
#include <grab.h>
#include <move.h>
#include <pick.h>
#include <sound.h>
#include <waylandhelper.h>
#include <nemoxml.h>
#include <nemolog.h>
#include <nemoitem.h>
#include <nemobox.h>
#include <nemomisc.h>

#include <minishell.h>
#include <minitap.h>
#include <miniedge.h>
#include <minimisc.h>
#include <nemograb.h>
#include <nemopad.h>
#include <nemopack.h>
#include <nemocursor.h>
#include <showhelper.h>

static void minishell_load_background(struct minishell *mini)
{
	struct nemoshell *shell = mini->shell;
	struct nemocompz *compz = mini->compz;
	int index;

	nemoitem_for_each(shell->configs, index, "//nemoshell/background", 0) {
		pid_t pid;
		char *argv[32];
		int argc = 0;
		char *attr;
		int iattr;
		int32_t x, y;
		int32_t width, height;

		x = nemoitem_get_iattr(shell->configs, index, "x", 0);
		y = nemoitem_get_iattr(shell->configs, index, "y", 0);
		width = nemoitem_get_iattr(shell->configs, index, "width", nemocompz_get_scene_width(compz));
		height = nemoitem_get_iattr(shell->configs, index, "height", nemocompz_get_scene_height(compz));

		argv[argc++] = nemoitem_get_attr(shell->configs, index, "path");
		argv[argc++] = strdup("-w");
		asprintf(&argv[argc++], "%d", width);
		argv[argc++] = strdup("-h");
		asprintf(&argv[argc++], "%d", height);

		nemoitem_for_vattr(shell->configs, index, "arg%d", iattr, 0, attr)
			argv[argc++] = attr;

		argv[argc++] = NULL;

		pid = wayland_execute_path(argv[0], argv, NULL);
		if (pid > 0) {
			struct clientstate *state;

			state = nemoshell_create_client_state(shell, pid);
			if (state != NULL) {
				state->x = x;
				state->y = y;
				state->dx = 0.0f;
				state->dy = 0.0f;
				state->flags = NEMO_SHELL_SURFACE_ALL_FLAGS;
			}
		}

		free(argv[1]);
		free(argv[2]);
		free(argv[3]);
		free(argv[4]);
	}
}

static void minishell_load_menu(struct minishell *mini)
{
	struct nemoshell *shell = mini->shell;
	struct minigroup *group;
	struct miniitem *item;
	char *path;
	char *icon;
	char *ring;
	char *type;
	char *input;
	char *resize;
	char *width;
	char *height;
	char *attr;
	int argc;
	int igroup;
	int index;
	int i;

	nemoitem_for_each(shell->configs, index, "//nemoshell/edge/group", 0) {
		icon = nemoitem_get_attr(shell->configs, index, "icon");
		ring = nemoitem_get_attr(shell->configs, index, "ring");
		if (icon == NULL || ring == NULL)
			continue;

		group = (struct minigroup *)malloc(sizeof(struct minigroup));
		group->icon = strdup(icon);
		group->ring = strdup(ring);
		group->nitems = 0;

		mini->groups[mini->ngroups++] = group;
	}

	nemoitem_for_each(shell->configs, index, "//nemoshell/edge/item", 0) {
		igroup = nemoitem_get_iattr(shell->configs, index, "group", 0);
		path = nemoitem_get_attr(shell->configs, index, "path");
		icon = nemoitem_get_attr(shell->configs, index, "icon");
		ring = nemoitem_get_attr(shell->configs, index, "ring");
		type = nemoitem_get_attr(shell->configs, index, "type");
		input = nemoitem_get_attr(shell->configs, index, "input");
		resize = nemoitem_get_attr(shell->configs, index, "resize");
		if (igroup < 0 || igroup >= mini->ngroups || icon == NULL || ring == NULL)
			continue;

		group = mini->groups[igroup];

		item = (struct miniitem *)malloc(sizeof(struct miniitem));
		item->icon = strdup(icon);
		item->ring = strdup(ring);

		item->type = MINISHELL_APP_NORMAL_TYPE;
		if (type != NULL) {
			if (strcmp(type, "keypad") == 0)
				item->type = MINISHELL_APP_KEYPAD_TYPE;
			else if (strcmp(type, "speaker") == 0)
				item->type = MINISHELL_APP_SPEAKER_TYPE;
		}

		item->input = NEMO_VIEW_INPUT_NORMAL;
		if (input != NULL) {
			if (strcmp(input, "touch") == 0)
				item->input = NEMO_VIEW_INPUT_TOUCH;
		}

		item->flags = NEMO_SHELL_SURFACE_MOVABLE_FLAG | NEMO_SHELL_SURFACE_PICKABLE_FLAG | NEMO_SHELL_SURFACE_MAXIMIZABLE_FLAG | NEMO_SHELL_SURFACE_MINIMIZABLE_FLAG;
		if (resize != NULL) {
			if (strcmp(resize, "on") == 0)
				item->flags = NEMO_SHELL_SURFACE_MOVABLE_FLAG | NEMO_SHELL_SURFACE_RESIZABLE_FLAG | NEMO_SHELL_SURFACE_PICKABLE_FLAG | NEMO_SHELL_SURFACE_MAXIMIZABLE_FLAG | NEMO_SHELL_SURFACE_MINIMIZABLE_FLAG;
			else if (strcmp(resize, "scale") == 0)
				item->flags = NEMO_SHELL_SURFACE_MOVABLE_FLAG | NEMO_SHELL_SURFACE_SCALABLE_FLAG | NEMO_SHELL_SURFACE_PICKABLE_FLAG | NEMO_SHELL_SURFACE_MAXIMIZABLE_FLAG | NEMO_SHELL_SURFACE_MINIMIZABLE_FLAG;
		}

		width = nemoitem_get_attr(shell->configs, index, "max_width");
		height = nemoitem_get_attr(shell->configs, index, "max_height");
		if (width != NULL && height != NULL) {
			item->max_width = strtoul(width, NULL, 10);
			item->max_height = strtoul(height, NULL, 10);
			item->has_max_size = 1;
		} else {
			item->has_max_size = 0;
		}

		width = nemoitem_get_attr(shell->configs, index, "min_width");
		height = nemoitem_get_attr(shell->configs, index, "min_height");
		if (width != NULL && height != NULL) {
			item->min_width = strtoul(width, NULL, 10);
			item->min_height = strtoul(height, NULL, 10);
			item->has_min_size = 1;
		} else {
			item->has_min_size = 0;
		}

		argc = 0;

		if (item->type == MINISHELL_APP_NORMAL_TYPE) {
			item->path = strdup(path);
			item->args[argc++] = item->path;
		}

		nemoitem_for_vattr(shell->configs, index, "arg%d", i, 0, attr)
			item->args[argc++] = attr;

		item->args[argc++] = NULL;

		group->items[group->nitems++] = item;
	}
}

static void minishell_handle_escape_key(struct nemocompz *compz, struct nemokeyboard *keyboard, uint32_t time, uint32_t key, enum wl_keyboard_key_state state, void *data)
{
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		nemolog_message("SHELL", "exit nemoshell by escape key\n");

		nemocompz_destroy_clients(compz);
		nemocompz_exit(compz);
	}
}

static void minishell_handle_tab_key(struct nemocompz *compz, struct nemokeyboard *keyboard, uint32_t time, uint32_t key, enum wl_keyboard_key_state state, void *data)
{
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		struct nemoscreen *screen;

		nemolog_message("SHELL", "capture screen by tab key\n");

		screen = nemocompz_get_main_screen(compz);
		if (screen != NULL) {
			pixman_image_t *image;

			image = pixman_image_create_bits(PIXMAN_a8b8g8r8, screen->width, screen->height, NULL, screen->width * 4);

			nemoscreen_read_pixels(screen, PIXMAN_a8b8g8r8,
					pixman_image_get_data(image),
					screen->x, screen->y,
					screen->width, screen->height);

			pixman_image_unref(image);
		}
	}
}

static void minishell_handle_left_button(struct nemocompz *compz, struct nemopointer *pointer, uint32_t time, uint32_t button, enum wl_pointer_button_state state, void *data)
{
	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (pointer->focus != NULL && pointer->focus->canvas != NULL) {
			struct nemocanvas *parent = nemosubcanvas_get_main_canvas(pointer->focus->canvas);
			struct shellbin *bin = nemoshell_get_bin(pointer->focus->canvas);

			nemoview_update_layer(pointer->focus);

			if (bin != NULL) {
				nemopointer_set_keyboard_focus(pointer, pointer->focus);
				datadevice_set_focus(pointer->seat, pointer->focus);
			}
		} else if (pointer->focus != NULL && pointer->focus->actor != NULL) {
			nemoview_update_layer(pointer->focus);
			nemopointer_set_keyboard_focus(pointer, pointer->focus);
		}

		wl_signal_emit(&compz->activate_signal, pointer->focus);
	}
}

static void minishell_handle_key_binding(struct nemocompz *compz, struct nemokeyboard *keyboard, uint32_t time, uint32_t key, enum wl_keyboard_key_state state, void *data)
{
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		struct nemopointer *pointer = (struct nemopointer *)data;

		nemopointer_set_keyboard(pointer, keyboard);

		nemolog_message("SHELL", "bind keyboard(%s) to pointer(%s)\n", keyboard->node->devnode, pointer->node->devnode);
	}
}

static void minishell_handle_touch_binding(struct nemocompz *compz, struct touchpoint *tp, uint32_t time, void *data)
{
	struct nemopointer *pointer = (struct nemopointer *)data;
	struct nemoscreen *screen;

	screen = nemocompz_get_screen_on(compz, pointer->x, pointer->y);

	nemoinput_set_screen(tp->touch->node, screen);

	nemolog_message("SHELL", "bind touch(%s) to screen(%d:%d)\n", tp->touch->node->devnode, screen->node->nodeid, screen->screenid);
}

static void minishell_handle_right_button(struct nemocompz *compz, struct nemopointer *pointer, uint32_t time, uint32_t button, enum wl_pointer_button_state state, void *data)
{
	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		pointer->bindings[0] = nemocompz_add_key_binding(compz, KEY_ENTER, 0, minishell_handle_key_binding, (void *)pointer);
	} else if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
		if (pointer->bindings[0] != NULL) {
			nemobinding_destroy(pointer->bindings[0]);

			pointer->bindings[0] = NULL;
		}
	}
}

static void minishell_handle_touch_down(struct nemocompz *compz, struct touchpoint *tp, uint32_t time, void *data)
{
	if (tp->focus != NULL && tp->focus->canvas != NULL) {
		struct minishell *mini = (struct minishell *)data;
		struct nemocanvas *parent = nemosubcanvas_get_main_canvas(tp->focus->canvas);
		struct shellbin *bin = nemoshell_get_bin(tp->focus->canvas);

		nemoview_update_layer(tp->focus);

		if (bin != NULL) {
			datadevice_set_focus(tp->touch->seat, tp->focus);

			if ((bin->flags & NEMO_SHELL_SURFACE_BINDABLE_FLAG) && (bin->grabbed == 0) && (bin->fixed == 0)) {
				struct touchpoint *tps[10];
				int tapcount;

				tapcount = nemoseat_get_touchpoint_by_view(compz->seat, tp->focus, tps, 5);
				if (tapcount >= 3) {
					struct nemopack *pack;

					pack = nemopack_create(mini->shell, bin->view, mini->packtimeout);
					if (pack != NULL)
						nemoseat_bypass_touchpoint_by_view(compz->seat, bin->view);
				}
			}
		}
	} else if (tp->focus != NULL && tp->focus->actor != NULL) {
		nemoview_update_layer(tp->focus);
	}

	wl_signal_emit(&compz->activate_signal, tp->focus);
}

static void minishell_handle_child_signal(struct wl_listener *listener, void *data)
{
	struct minishell *mini = (struct minishell *)container_of(listener, struct minishell, child_signal_listener);
}

static int minishell_dispatch_tap_grab(struct talegrab *base, uint32_t type, struct taleevent *event)
{
	struct nemograb *grab = (struct nemograb *)container_of(base, struct nemograb, base);
	struct minitap *tap = (struct minitap *)nemograb_get_userdata(grab);
	struct minishell *mini = tap->mini;

	if (type & NEMOTALE_DOWN_EVENT) {
		minishell_tap_down(mini, tap, event->x, event->y);

		nemoactor_dispatch_frame(mini->actor);
	} else if (type & NEMOTALE_MOTION_EVENT) {
		minishell_tap_motion(mini, tap, event->x, event->y);

		nemoactor_dispatch_frame(mini->actor);
	} else if (type & NEMOTALE_UP_EVENT) {
		minishell_tap_up(mini, tap, event->x, event->y);

		nemoactor_dispatch_frame(mini->actor);

		nemograb_destroy(grab);

		return 0;
	}

	return 1;
}

static int minishell_dispatch_edge_grab(struct talegrab *base, uint32_t type, struct taleevent *event)
{
	struct nemograb *grab = (struct nemograb *)container_of(base, struct nemograb, base);
	struct miniedge *edge = (struct miniedge *)nemograb_get_userdata(grab);
	struct minishell *mini = edge->mini;

	if (type & NEMOTALE_DOWN_EVENT) {
		minishell_edge_down(mini, edge, event->x, event->y);

		nemoactor_dispatch_frame(mini->actor);
	} else if (type & NEMOTALE_MOTION_EVENT) {
		minishell_edge_motion(mini, edge, event->x, event->y);

		nemoactor_dispatch_frame(mini->actor);
	} else if (type & NEMOTALE_UP_EVENT) {
		minishell_edge_up(mini, edge, event->x, event->y);

		nemoactor_dispatch_frame(mini->actor);

		nemograb_destroy(grab);

		return 0;
	}

	return 1;
}

static int minishell_dispatch_edge_group_grab(struct talegrab *base, uint32_t type, struct taleevent *event)
{
	struct nemograb *grab = (struct nemograb *)container_of(base, struct nemograb, base);
	struct miniedge *edge = (struct miniedge *)nemograb_get_userdata(grab);
	struct minishell *mini = edge->mini;
	uint32_t tag = nemograb_get_tag(grab);

	if (type & NEMOTALE_DOWN_EVENT) {
		minishell_edge_activate_group(mini, edge, tag - 1000);
	} else if (type & NEMOTALE_MOTION_EVENT) {
		uint32_t tag;

		tag = nemoshow_canvas_pick_tag(mini->canvas, event->x, event->y);
		if (1000 <= tag && tag < 2000) {
			if (edge->groupidx != tag - 1000) {
				minishell_edge_deactivate_group(mini, edge);
				minishell_edge_activate_group(mini, edge, tag - 1000);
			} else {
				minishell_edge_deactivate_item(mini, edge);
			}
		} else if (2000 <= tag && tag < 3000) {
			if (edge->itemidx < 0) {
				minishell_edge_activate_item(mini, edge, tag - 2000);
			} else if (edge->itemidx != tag - 2000) {
				minishell_edge_deactivate_item(mini, edge);
				minishell_edge_activate_item(mini, edge, tag - 2000);
			}
		} else {
			minishell_edge_deactivate_item(mini, edge);
		}
	} else if (type & NEMOTALE_UP_EVENT) {
		if (edge->itemidx >= 0) {
			struct miniitem *item = mini->groups[edge->groupidx]->items[edge->itemidx];
			struct clientstate *state = NULL;

			if (item->type == MINISHELL_APP_NORMAL_TYPE) {
				pid_t pid;

				pid = wayland_execute_path(item->path, item->args, NULL);
				if (pid > 0) {
					state = nemoshell_create_client_state(mini->shell, pid);
				}
			} else if (item->type == MINISHELL_APP_KEYPAD_TYPE) {
				struct nemopad *pad;

				pad = nemopad_create(mini->shell, mini->padwidth, mini->padheight, mini->padminwidth, mini->padminheight, mini->padtimeout);
				if (pad != NULL)
					nemopad_activate(pad, event->x, event->y, edge->r);
			}

			if (state != NULL) {
				state->x = event->x;
				state->y = event->y;
				state->dx = 0.5f;
				state->dy = 0.5f;
				state->r = edge->r * M_PI / 180.0f;
				state->flags = item->flags;
				state->input_type = item->input;

				if (item->has_max_size != 0) {
					state->max_width = item->max_width;
					state->max_height = item->max_height;
					state->has_max_size = 1;
				}

				if (item->has_min_size != 0) {
					state->min_width = item->min_width;
					state->min_height = item->min_height;
					state->has_min_size = 1;
				}
			}

			minishell_edge_shutdown(mini, edge);
		} else {
			minishell_edge_deactivate_group(mini, edge);
		}

		nemograb_destroy(grab);

		return 0;
	}

	return 1;
}

static void minishell_dispatch_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
	uint32_t id = nemotale_node_get_id(node);

	if (nemotale_is_pointer_enter(tale, event, type)) {
		struct nemoshow *show = (struct nemoshow *)nemotale_get_userdata(tale);
		struct nemoshell *shell = NEMOSHOW_AT(show, shell);
		struct nemocompz *compz = shell->compz;
		struct nemoseat *seat = compz->seat;
		struct nemopointer *pointer;
		struct nemoactor *cursor;
		int32_t dx, dy;

		pointer = nemoseat_get_pointer_by_id(seat, event->device);
		if (pointer != NULL) {
			int32_t width = 32;
			int32_t height = 32;

			cursor = nemoactor_create_pixman(shell->compz, width, height);
			if (cursor != NULL) {
				nemocursor_make_circle(
						cursor->data, width, height,
						width / 2.0f, height / 2.0f, width / 3.0f,
						SkPaint::kFill_Style,
						SkColorSetARGB(255, 0, 255, 255),
						5.0f);

				nemopointer_set_cursor_actor(pointer, cursor, width / 2, height / 2);
			}
		}
	} else if (nemotale_is_pointer_leave(tale, event, type)) {
	}

	if (id == 1) {
		if (nemotale_dispatch_grab(tale, event->device, type, event) == 0) {
			struct nemoshow *show = (struct nemoshow *)nemotale_get_userdata(tale);
			struct nemoshell *shell = NEMOSHOW_AT(show, shell);
			struct nemoactor *actor = NEMOSHOW_AT(show, actor);
			struct minishell *mini = (struct minishell *)nemoshow_get_userdata(show);
			struct showone *canvas = mini->canvas;
			struct nemograb *grab;

			if (nemotale_is_touch_down(tale, event, type)) {
				struct showone *one;
				uint32_t tag;

				one = nemoshow_canvas_pick_one(canvas, event->x, event->y);

				tag = nemoshow_one_get_tag(one);
				if (1000 <= tag && tag < 2000) {
					struct miniedge *edge = (struct miniedge *)nemoshow_one_get_userdata(one);

					grab = nemograb_create(shell, tale, event, minishell_dispatch_edge_group_grab);
					nemograb_set_userdata(grab, edge);
					nemograb_set_tag(grab, tag);
					nemograb_check_signal(grab, &edge->destroy_signal);
					nemotale_dispatch_grab(tale, event->device, type, event);
				} else {
					int site = minishell_get_edge(mini, event->x, event->y, mini->edgesize);

					if (site == MINISHELL_NONE_SITE) {
						struct minitap *tap;

						tap = minishell_tap_create(mini);

						grab = nemograb_create(shell, tale, event, minishell_dispatch_tap_grab);
						nemograb_set_userdata(grab, tap);
						nemograb_check_signal(grab, &tap->destroy_signal);
						nemotale_dispatch_grab(tale, event->device, type, event);
					} else {
						struct miniedge *edge;

						edge = minishell_edge_create(mini, site);

						grab = nemograb_create(shell, tale, event, minishell_dispatch_edge_grab);
						nemograb_set_userdata(grab, edge);
						nemograb_check_signal(grab, &edge->destroy_signal);
						nemotale_dispatch_grab(tale, event->device, type, event);
					}
				}
			}
		}
	}
}

int main(int argc, char *argv[])
{
	struct option options[] = {
		{ "node",								required_argument,	NULL,		'n' },
		{ "seat",								required_argument,	NULL,		's' },
		{ "tty",								required_argument,	NULL,		't' },
		{ "config",							required_argument,	NULL,		'c' },
		{ "log",								required_argument,	NULL,		'l' },
		{ "help",								no_argument,				NULL,		'h' },
		{ 0 }
	};

	struct minishell *mini;
	struct nemoshell *shell;
	struct nemocompz *compz;
	struct nemoactor *actor;
	struct nemoshow *show;
	struct showone *scene;
	struct showone *canvas;
	struct showone *blur;
	struct showone *ease;
	char *rendernode = NULL;
	char *configpath = NULL;
	char *seat = NULL;
	char *env;
	int tty = 0;
	int opt;

	nemolog_set_file(2);

	while (opt = getopt_long(argc, argv, "n:s:t:c:l:h", options, NULL)) {
		if (opt == -1)
			break;

		switch (opt) {
			case 'n':
				rendernode = strdup(optarg);
				break;

			case 's':
				seat = strdup(optarg);
				break;

			case 't':
				tty = strtoul(optarg, NULL, 10);
				break;

			case 'c':
				configpath = strdup(optarg);
				break;

			case 'l':
				nemolog_open_socket(optarg);
				break;

			case 'h':
				fprintf(stderr, "usage: minishell --node [rendernode] --seat [name] --tty [number] --config [filepath] --log [logfile]\n");
				return 0;

			default:
				break;
		}
	}

	nemolog_message("SHELL", "start nemoshell...\n");

	if (configpath == NULL)
		asprintf(&configpath, "%s/.config/minishell.xml", getenv("HOME"));

	mini = (struct minishell *)malloc(sizeof(struct minishell));
	if (mini == NULL)
		return -1;
	memset(mini, 0, sizeof(struct minishell));

	nemolist_init(&mini->tap_list);

	compz = nemocompz_create();
	if (compz == NULL)
		goto out1;
	nemocompz_load_configs(compz, configpath);

	shell = nemoshell_create(compz);
	if (shell == NULL)
		goto out2;

	mini->compz = compz;
	mini->shell = shell;

	mini->xserver = nemoxserver_create(shell,
			nemoitem_get_attr_named(shell->configs, "//nemoshell/xserver", "path"));

	if (seat != NULL)
		nemosession_connect(compz->session, seat, tty);

	nemocompz_load_backends(compz);
	nemocompz_load_scenes(compz);
	nemocompz_load_virtuios(compz);
	nemocompz_load_plugins(compz);

	nemocompz_add_key_binding(compz, KEY_ESC, MODIFIER_CTRL, minishell_handle_escape_key, (void *)mini);
	nemocompz_add_key_binding(compz, KEY_TAB, MODIFIER_CTRL, minishell_handle_tab_key, (void *)mini);
	nemocompz_add_button_binding(compz, BTN_LEFT, minishell_handle_left_button, (void *)mini);
	nemocompz_add_button_binding(compz, BTN_RIGHT, minishell_handle_right_button, (void *)mini);
	nemocompz_add_touch_binding(compz, minishell_handle_touch_down, (void *)mini);

	nemoshell_load_fullscreens(shell);
	nemoshell_load_gestures(shell);

	wl_list_init(&mini->child_signal_listener.link);
	mini->child_signal_listener.notify = minishell_handle_child_signal;
	wl_signal_add(&compz->child_signal, &mini->child_signal_listener);

	nemocompz_make_current(compz);

	mini->width = nemocompz_get_scene_width(compz);
	mini->height = nemocompz_get_scene_height(compz);

	asprintf(&env, "%d", mini->width);
	setenv("NEMOTOOL_FULLSCREEN_WIDTH", env, 1);
	free(env);

	asprintf(&env, "%d", mini->height);
	setenv("NEMOTOOL_FULLSCREEN_HEIGHT", env, 1);
	free(env);

	nemoshow_initialize();

	mini->show = show = nemoshow_create_actor(shell,
			mini->width, mini->height,
			minishell_dispatch_tale_event);
	if (show == NULL)
		goto out3;
	nemoshow_set_userdata(show, mini);

	mini->scene = scene = nemoshow_scene_create();
	nemoshow_scene_set_width(scene, mini->width);
	nemoshow_scene_set_height(scene, mini->height);
	nemoshow_attach_one(show, scene);

	mini->back = canvas = nemoshow_canvas_create();
	nemoshow_canvas_set_width(canvas, mini->width);
	nemoshow_canvas_set_height(canvas, mini->height);
	nemoshow_canvas_set_type(canvas, NEMOSHOW_CANVAS_BACK_TYPE);
	nemoshow_canvas_set_fill_color(canvas, 0.0f, 0.0f, 0.0f, 0.0f);
	nemoshow_canvas_set_alpha(canvas, 0.0f);
	nemoshow_attach_one(show, canvas);
	nemoshow_one_attach(scene, canvas);

	mini->canvas = canvas = nemoshow_canvas_create();
	nemoshow_canvas_set_width(canvas, mini->width);
	nemoshow_canvas_set_height(canvas, mini->height);
	nemoshow_canvas_set_type(canvas, NEMOSHOW_CANVAS_VECTOR_TYPE);
	nemoshow_canvas_set_event(canvas, 1);
	nemoshow_attach_one(show, canvas);
	nemoshow_one_attach(scene, canvas);

	nemoshow_set_scene(show, scene);
	nemoshow_set_size(show, mini->width, mini->height);

	mini->ease0 = ease = nemoshow_ease_create();
	nemoshow_ease_set_type(ease, NEMOEASE_CUBIC_INOUT_TYPE);

	mini->ease1 = ease = nemoshow_ease_create();
	nemoshow_ease_set_type(ease, NEMOEASE_CUBIC_OUT_TYPE);

	mini->ease2 = ease = nemoshow_ease_create();
	nemoshow_ease_set_type(ease, NEMOEASE_LINEAR_TYPE);

	mini->inner = blur = nemoshow_filter_create(NEMOSHOW_BLUR_FILTER);
	nemoshow_attach_one(show, blur);
	nemoshow_filter_set_blur(blur, "high", "inner", 5.0f);

	mini->outer = blur = nemoshow_filter_create(NEMOSHOW_BLUR_FILTER);
	nemoshow_attach_one(show, blur);
	nemoshow_filter_set_blur(blur, "high", "outer", 5.0f);

	mini->solid = blur = nemoshow_filter_create(NEMOSHOW_BLUR_FILTER);
	nemoshow_attach_one(show, blur);
	nemoshow_filter_set_blur(blur, "high", "solid", 15.0f);

	mini->solidsmall = blur = nemoshow_filter_create(NEMOSHOW_BLUR_FILTER);
	nemoshow_attach_one(show, blur);
	nemoshow_filter_set_blur(blur, "high", "solid", 3.0f);

	nemoshow_render_one(show);

	mini->actor = actor = NEMOSHOW_AT(show, actor);
	nemoview_attach_layer(actor->view, &shell->underlay_layer);
	nemoview_set_state(actor->view, NEMO_VIEW_MAPPED_STATE);
	nemoview_set_position(actor->view, 0.0f, 0.0f);
	nemoview_update_transform(actor->view);

	mini->edgesize = nemoitem_get_fattr_named(shell->configs, "//nemoshell/edge", "size", 100.0f);
	mini->edgetimeout = nemoitem_get_iattr_named(shell->configs, "//nemoshell/edge", "timeout", 1500);

	mini->packtimeout = nemoitem_get_iattr_named(shell->configs, "//nemoshell/pack", "timeout", NEMOPACK_TIMEOUT);

	mini->padwidth = nemoitem_get_iattr_named(shell->configs, "//nemoshell/pad", "width", NEMOPAD_WIDTH);
	mini->padheight = nemoitem_get_iattr_named(shell->configs, "//nemoshell/pad", "height", NEMOPAD_HEIGHT);
	mini->padminwidth = nemoitem_get_iattr_named(shell->configs, "//nemoshell/pad", "min_width", NEMOPAD_MINIMUM_WIDTH);
	mini->padminheight = nemoitem_get_iattr_named(shell->configs, "//nemoshell/pad", "min_height", NEMOPAD_MINIMUM_HEIGHT);
	mini->padtimeout = nemoitem_get_iattr_named(shell->configs, "//nemoshell/pad", "timeout", NEMOPAD_TIMEOUT);

	mini->tapsize = nemoitem_get_fattr_named(shell->configs, "//nemoshell/tap", "size", 70.0f);

	minishell_load_background(mini);
	minishell_load_menu(mini);

	nemopack_prepare_envs();
	nemopad_prepare_envs();

	nemoactor_dispatch_frame(actor);

	nemocompz_run(compz);

	nemopack_finish_envs();
	nemopad_finish_envs();

	nemoshow_destroy_actor(show);

out3:
	nemoshow_finalize();

	nemoshell_destroy(shell);

out2:
	nemocompz_destroy(compz);

out1:
	free(mini);

	nemolog_message("SHELL", "end nemoshell...\n");

	nemolog_close_file();

	return 0;
}
