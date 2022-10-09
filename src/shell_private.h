#ifndef SHELL_PRIVATE_H_
#define SHELL_PRIVATE_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>
#include <limits.h>

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
	
	char archive_name[PATH_MAX];
	ssize_t num_entries;
	struct ooxml_zip_file *files;
};

enum ARCHIVE_FILES_LIST_COLUMN
{
	ARCHIVE_FILES_LIST_COLUMN_name,
	ARCHIVE_FILES_LIST_COLUMN_size,
	ARCHIVE_FILES_LIST_COLUMN_index,
	ARCHIVE_FILES_LIST_COLUMN_mtime,
	ARCHIVE_FILES_LIST_COLUMN_row_type,	// 0: file, 1:dir, -1: root
	ARCHIVE_FILES_LIST_COLUMN_data_ptr,	// pointer to struct ooxml_zip_file
	ARCHIVE_FILES_LIST_COLUMNS_COUNT
};

#ifdef __cplusplus
}
#endif
#endif
