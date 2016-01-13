#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <math.h>
#include <wayland-server.h>

#include <minishell.h>
#include <minimisc.h>

int minishell_get_site(struct minishell *mini, double x, double y)
{
	double dt, db, dl, dr;

	dt = sqrtf(y * y);
	db = sqrtf((mini->height - y) * (mini->height - y));
	dl = sqrtf(x * x);
	dr = sqrtf((mini->width - x) * (mini->width - x));

	if (dt < db && dt < dl && dt < dr)
		return MINISHELL_TOP_SITE;
	if (dl < dt && dl < db && dl < dr)
		return MINISHELL_LEFT_SITE;
	if (dr < dt && dr < db && dr < dl)
		return MINISHELL_RIGHT_SITE;

	return MINISHELL_BOTTOM_SITE;
}

double minishell_get_site_rotation(struct minishell *mini, int site)
{
	if (site == MINISHELL_TOP_SITE)
		return 180.0f;
	else if (site == MINISHELL_LEFT_SITE)
		return 90.0f;
	else if (site == MINISHELL_RIGHT_SITE)
		return -90.0f;

	return 0.0f;
}

int minishell_get_edge(struct minishell *mini, double x, double y, uint32_t edgesize)
{
	double dt, db, dl, dr;

	if (edgesize <= 0)
		return MINISHELL_NONE_SITE;

	dt = sqrtf(y * y);
	db = sqrtf((mini->height - y) * (mini->height - y));
	dl = sqrtf(x * x);
	dr = sqrtf((mini->width - x) * (mini->width - x));

	if (dt < edgesize)
		return MINISHELL_TOP_SITE;
	if (db < edgesize)
		return MINISHELL_BOTTOM_SITE;
	if (dl < edgesize)
		return MINISHELL_LEFT_SITE;
	if (dr < edgesize)
		return MINISHELL_RIGHT_SITE;

	return MINISHELL_NONE_SITE;
}
