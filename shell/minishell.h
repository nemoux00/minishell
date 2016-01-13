#ifndef __MINISHELL_H__
#define __MINISHELL_H__

#include <nemoconfig.h>

#ifdef __cplusplus
NEMO_BEGIN_EXTERN_C
#endif

#include <nemolist.h>
#include <nemolistener.h>
#include <nemobook.h>

#include <showhelper.h>

#define	MINISHELL_GROUP_MAX			(8)
#define	MINISHELL_ITEM_MAX			(16)

typedef enum {
	MINISHELL_APP_NORMAL_TYPE = 0,
	MINISHELL_APP_KEYPAD_TYPE = 1,
	MINISHELL_APP_SPEAKER_TYPE = 2,
	MINISHELL_APP_LAST_TYPE
} MiniShellAppType;

struct nemoxserver;

struct miniitem {
	char *path;
	char *icon;
	char *ring;
	char *args[16];

	uint32_t flags;

	int type;
	int input;

	uint32_t max_width, max_height;
	uint32_t min_width, min_height;
	int has_min_size, has_max_size;

	uint32_t time;
};

struct minigroup {
	struct miniitem *items[MINISHELL_ITEM_MAX];
	int nitems;

	char *icon;
	char *ring;
};

struct minishell {
	struct nemocompz *compz;
	struct nemoshell *shell;
	struct nemoxserver *xserver;

	uint32_t width, height;

	struct nemolist tap_list;

	struct wl_listener child_signal_listener;

	struct nemoshow *show;

	struct showone *scene;

	struct showone *back;
	struct showone *canvas;

	struct showone *ease0;
	struct showone *ease1;
	struct showone *ease2;

	struct showone *inner;
	struct showone *outer;
	struct showone *solid;
	struct showone *solidsmall;

	struct nemoactor *actor;

	float edgesize;
	uint32_t edgetimeout;

	uint32_t packtimeout;

	uint32_t padwidth;
	uint32_t padheight;
	uint32_t padminwidth;
	uint32_t padminheight;
	uint32_t padtimeout;

	float tapsize;

	struct minigroup *groups[MINISHELL_GROUP_MAX];
	int ngroups;
};

#ifdef __cplusplus
NEMO_END_EXTERN_C
#endif

#endif
