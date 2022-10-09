/*
 * shell.c
 * 
 * Copyright 2022 chehw <hongwei.che@gmail.com>
 * 
 * The MIT License (MIT)
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <gtk/gtk.h>
#include <libgen.h>

#include "shell.h"
#include "shell_private.h"
#include "app.h"
#include "ooxml_context.h"

#include "ui/main_window.c"

static int shell_init(struct shell_context *shell, json_object *jconfig);
static int shell_run(struct shell_context *app);
static int shell_stop(struct shell_context *app);

static struct shell_private *shell_private_new(struct shell_context *shell)
{
	struct shell_private *priv = calloc(1, sizeof(*priv));
	assert(priv);
	priv->shell = shell;
	
	return priv;
}
static void shell_private_free(struct shell_private *priv)
{
	if(NULL == priv) return;
	
	///< @todo
	free(priv);
	return;
}
	
struct shell_context *shell_context_init(struct shell_context *shell, struct app_context *app)
{
	if(NULL == shell) shell = calloc(1, sizeof(*shell));
	assert(shell);
	shell->app = app;
	shell->init = shell_init;
	shell->run = shell_run;
	shell->stop = shell_stop;

	
	struct shell_private *priv = shell_private_new(shell);
	assert(priv);
	shell->priv = priv;
	
	return shell;
}
void shell_context_cleanup(struct shell_context *shell)
{
	if(NULL == shell) return;
	shell_private_free(shell->priv);
	///< @todo
}


static int shell_init(struct shell_context *shell, json_object *jconfig)
{
	init_windows(shell);
	return 0;
}
static int shell_run(struct shell_context *shell)
{
	assert(shell && shell->priv);
	gtk_widget_show_all(shell->priv->window);
	gtk_main();
	return 0;
}
static int shell_stop(struct shell_context *shell)
{
	gtk_main_quit();
	return 0;
}

static void ooxml_zip_file_free_array(ssize_t num_entries, struct ooxml_zip_file *files)
{
	if(files) {
		for(ssize_t i = 0; i < num_entries; ++i) {
			ooxml_zip_file_clear(&files[i]);
		}
		free(files);
	}
}

struct dir_info {
	char *path;
	GtkTreeIter iter;
};
void dir_info_free_array(ssize_t num_dirs, struct dir_info *folders)
{
	for(ssize_t i = 0; i < num_dirs; ++i) {
		if(folders[i].path && strcmp(folders[i].path, ".") != 0) free(folders[i].path);
	}
	free(folders);
}

struct dir_info *find_dir_by_name(ssize_t num_dirs, struct dir_info *folders, const char *dir_name)
{
	assert(dir_name);
	for(ssize_t i = 0; i < num_dirs; ++i) {
		if(NULL == folders[i].path) continue;
		if(strcmp(folders[i].path, dir_name) == 0) return &folders[i];
	}
	return NULL;
}

static void shell_update_archive_list(struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	GtkTreeStore *store = gtk_tree_store_new(ARCHIVE_FILES_LIST_COLUMNS_COUNT, 
		G_TYPE_STRING, // file name
		G_TYPE_INT64, // file size
		G_TYPE_UINT64, // index
		G_TYPE_STRING, // modification datetime
		G_TYPE_INT, // row_type
		G_TYPE_POINTER
	);
	
	GtkTreeIter root, item;
	// set root item
	gtk_tree_store_append(store, &root, NULL);
	gtk_tree_store_set(store, &root, 
		ARCHIVE_FILES_LIST_COLUMN_name, priv->archive_name, 
		ARCHIVE_FILES_LIST_COLUMN_row_type, -1,
		-1);
	
	int num_dirs = 0;
	int max_dirs = 4096;
	struct dir_info *folders = calloc(max_dirs, sizeof(*folders));
	assert(folders);
	
	folders[0].path = ".";
	folders[0].iter = root;
	++num_dirs;
	
	for(ssize_t i = 0; i < priv->num_entries; ++i) {
		struct ooxml_zip_file *file = &priv->files[i];
		char path_name[PATH_MAX] = "";
		strncpy(path_name, file->filename, sizeof(path_name));
		
		char *file_name = basename(path_name);
		char *dir_name = dirname(path_name);
		
		struct dir_info *folder = find_dir_by_name(num_dirs, folders, dir_name);
		if(NULL == folder) {
			assert(num_dirs < max_dirs);
			folder = &folders[num_dirs++];
			folder->path = strdup(dir_name);
			gtk_tree_store_append(store, &folder->iter, &root);
			gtk_tree_store_set(store, &folder->iter, 
				ARCHIVE_FILES_LIST_COLUMN_name, dir_name, 
				ARCHIVE_FILES_LIST_COLUMN_row_type, 1,
				-1);
		}
		gtk_tree_store_append(store, &item, &folder->iter);
		
		struct tm t[1];
		char sz_time[100] = "";
		memset(t, 0, sizeof(t));
		localtime_r((time_t *)&file->mtime, t);
		strftime(sz_time, sizeof(sz_time), "%Y/%m/%d %H:%M:%S", t);
		
		gtk_tree_store_set(store, &item, 
			ARCHIVE_FILES_LIST_COLUMN_name, file_name,
			ARCHIVE_FILES_LIST_COLUMN_size, file->file_length,
			ARCHIVE_FILES_LIST_COLUMN_index, file->index,
			ARCHIVE_FILES_LIST_COLUMN_mtime, sz_time,
			ARCHIVE_FILES_LIST_COLUMN_row_type, 0,
			ARCHIVE_FILES_LIST_COLUMN_data_ptr, file,
			-1);
	}
	dir_info_free_array(num_dirs, folders);
	gtk_tree_view_set_model(GTK_TREE_VIEW(priv->archive_files_list), GTK_TREE_MODEL(store));
	
	gtk_tree_view_expand_all(GTK_TREE_VIEW(priv->archive_files_list));
	return;
	
}

int shell_load_ooxml_file(struct shell_context *shell, const char *filename)
{
	static GdkCursor *cursor_wait = NULL;;
	static GdkCursor *cursor_default = NULL;
	
	debug_printf("filename: %s", filename);
	assert(shell && shell->priv && shell->app);
	struct shell_private *priv = shell->priv;
	struct app_context *app = shell->app;
	struct ooxml_context *ooxml = app_get_ooxml_context(app);
	assert(ooxml);
	
	GdkDisplay *display = gtk_widget_get_display(priv->window);
	GdkWindow *window = gtk_widget_get_window(priv->window);
	
	if(NULL == cursor_wait) cursor_wait = gdk_cursor_new_from_name(display, "wait");
	if(NULL == cursor_default) cursor_default = gdk_cursor_new_from_name(display, "default");
	
	
	gdk_window_set_cursor(window, cursor_wait);
	// reset data
	if(priv->files)
	{
		ooxml_zip_file_free_array(priv->num_entries, priv->files);
		priv->num_entries = 0;
		priv->files = NULL;
	}
	
	const int readonly = 1;
	int rc = ooxml->open(ooxml, filename, readonly);
	assert(0 == rc);
	
	strncpy(priv->archive_name, filename, sizeof(priv->archive_name));
	ssize_t num_entries = ooxml->get_num_entries(ooxml);
	debug_printf("num_entries: %ld", (long)num_entries);
	
	priv->num_entries = num_entries;
	if(num_entries > 0) {
		struct ooxml_zip_file *files = calloc(num_entries, sizeof(*files));
		assert(files);
		priv->files = files;
		
		for(ssize_t i = 0; i < num_entries; ++i) {
			struct ooxml_zip_file *file = &files[i];
			int rc = ooxml->get_file(ooxml, i, file, 1);
			
			printf("==== %s(cb=%ld) ====\n", file->filename, (long)file->file_length);
			if(0 == rc && file->cb_data > 0) {
				if(file->doc) {
					printf("xml: \n");
					xmlDocDump(stdout, file->doc);
				}else {
					printf("raw_data: \n");
					fwrite(file->data, 1, file->cb_data, stdout);
				}
			}
		}
	}
	
	// update ui
	shell_update_archive_list(shell);
	
	gdk_window_set_cursor(window, cursor_default);
	return 0;
}


#if defined(TEST_SHELL_CONTEXT_) && defined(_STAND_ALONE)
int main(int argc, char **argv)
{
	
	return 0;
}
#endif
