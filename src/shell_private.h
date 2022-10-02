#ifndef SHELL_PRIVATE_H_
#define SHELL_PRIVATE_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>

#include "app.h"
#include "shell.h"
struct shell_private
{
	struct shell_context *shell;
	GtkWidget *window;
	GtkWidget *header_bar;
	GtkWidget *grid;
	
	GtkWidget *archive_files_list;
	GtkWidget *stack;
	GtkWidget *textview;
};


#ifdef __cplusplus
}
#endif
#endif
