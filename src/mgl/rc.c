#include <stdlib.h>
#include <os/device.h>
#include <errno.h>
#include <stdio.h>
#include "mgl.h"
#include "render.h"

mglrc_t *current;

mglrc_t *mglCreateRc(const wchar_t *server)
{
	mglrc_t *rc;

	wprintf(L"mglCreateRc: %s\n", server);
	rc = malloc(sizeof(*rc));
	if (rc == NULL)
		return NULL;

	if (server == NULL)
		server = L"vga4";

	rc->video = devOpen(server, NULL);
	if (rc->video == NULL ||
		FT_Init_FreeType(&rc->ft_library) ||
		FT_New_Face(rc->ft_library,
			"rezn000.ttf",
			0,
			&rc->ft_face))
    {
		mglDeleteRc(rc);
		return NULL;
	}
	 
	rc->surf_width = 640;
	rc->surf_height = 480;
	rc->gl_width = 1280;
	rc->gl_height = (rc->gl_width * rc->surf_height) / rc->surf_width;
	rc->scale_x = rc->gl_width / rc->surf_width;
	rc->scale_y = rc->gl_height / rc->surf_height;
	rc->colour = 0xffffff;
	rc->clear_colour = 0;

	if (current == NULL)
		mglUseRc(rc);

	return rc;
}

bool mglDeleteRc(mglrc_t *rc)
{
	wprintf(L"mglDeleteRc: %p\n", rc);
	if (rc)
	{
		if (rc->video)
			devClose(rc->video);

		free(rc);
		return true;
	}
	else
		return false;
}

bool mglUseRc(mglrc_t *rc)
{
	wprintf(L"mglUseRc: %p\n", rc);

	if (rc == NULL)
	{
		errno = EINVALID;
		return false;
	}

	current = rc;
	return true;
}

bool mgliMapToSurface(MGLreal x, MGLreal y, point_t *pt)
{
	if (current)
	{
		pt->x = (int) (x / current->scale_x);
		pt->y = (int) (y / current->scale_y);
		return true;
	}
	else
	{
		errno = EINVALID;
		return false;
	}
}

MGLcolour glSetColour(MGLcolour clr)
{
	MGLcolour old;

	if (current == NULL)
	{
		errno = EINVALID;
		return false;
	}

	old = current->colour;
	current->colour = clr;
	return old;
}

MGLcolour glSetClearColour(MGLcolour clr)
{
	MGLcolour old;

	if (current == NULL)
	{
		errno = EINVALID;
		return false;
	}

	old = current->colour;
	current->clear_colour = clr;
	return old;
}