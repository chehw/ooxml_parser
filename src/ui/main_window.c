/*
 * main_window.c
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
#include "shell.h"
#include "shell_private.h"

static void on_file_selected(GtkWidget *file_chooser, struct shell_context *shell)
{
	assert(shell && shell->app && shell->priv);
	struct shell_private *priv = shell->priv;
	
	if(file_chooser) gtk_widget_set_sensitive(file_chooser, FALSE);
	
	char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
	if(filename) {
		gtk_header_bar_set_subtitle(GTK_HEADER_BAR(priv->header_bar), filename);
		
		
		shell_load_ooxml_file(shell, filename);
		free(filename);
	}
	
	if(file_chooser) gtk_widget_set_sensitive(file_chooser, TRUE);
	return;
}

static void on_archive_file_selection_changed(GtkTreeSelection *selection, struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct shell_private *priv = shell->priv;
	
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean ok = gtk_tree_selection_get_selected(selection, &model, &iter);
	if(!ok) return;
	
	
	struct ooxml_zip_file *file = NULL;
	if(NULL == model) return;
	
	gtk_tree_model_get(model, &iter, ARCHIVE_FILES_LIST_COLUMN_data_ptr, &file, -1);
	if(file) {
		GtkTextView *textview = GTK_TEXT_VIEW(priv->textview);
		assert(textview);
		GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
		if(file->data && file->doc) {
			gtk_text_buffer_set_text(buffer, (const char *)file->data, file->cb_data);
		}
		gtk_text_view_set_buffer(textview, buffer);
	}
	
}

static void on_update_cell_column(GtkTreeViewColumn *col, 
	GtkCellRenderer *cr, 
	GtkTreeModel *model,
	GtkTreeIter *iter, 
	gpointer user_data)
{
	int row_type = -1;
	gtk_tree_model_get(model, iter, ARCHIVE_FILES_LIST_COLUMN_row_type, &row_type, -1);
	if(row_type != 0) {
		g_object_set(cr, "text", "", NULL);
	}
	return;
}

static void init_archive_files_list(GtkTreeView *tree, struct shell_context *shell)
{
	GtkTreeViewColumn *col = NULL;
	GtkCellRenderer *cr = NULL;
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("name", cr, "text", 0, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("size", cr, "text", ARCHIVE_FILES_LIST_COLUMN_size, NULL);
	gtk_tree_view_column_set_cell_data_func(col, cr, 
		on_update_cell_column, GINT_TO_POINTER(ARCHIVE_FILES_LIST_COLUMN_size), NULL);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("index", cr, "text", ARCHIVE_FILES_LIST_COLUMN_index, NULL);
	gtk_tree_view_column_set_cell_data_func(col, cr, 
		on_update_cell_column, GINT_TO_POINTER(ARCHIVE_FILES_LIST_COLUMN_index), NULL);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("mtime", cr, "text", ARCHIVE_FILES_LIST_COLUMN_mtime, NULL);
	gtk_tree_view_column_set_cell_data_func(col, cr, 
		on_update_cell_column, GINT_TO_POINTER(ARCHIVE_FILES_LIST_COLUMN_mtime), NULL);
	gtk_tree_view_append_column(tree, col);
	
	gtk_tree_view_set_grid_lines(tree, GTK_TREE_VIEW_GRID_LINES_VERTICAL);
	
	GtkTreeStore *store = gtk_tree_store_new(ARCHIVE_FILES_LIST_COLUMNS_COUNT, 
		G_TYPE_STRING, // file name
		G_TYPE_INT64, // file size
		G_TYPE_UINT64, // index
		G_TYPE_STRING, // modification datetime
		G_TYPE_INT,
		G_TYPE_POINTER
	);
	gtk_tree_view_set_model(tree, GTK_TREE_MODEL(store));
	
	GtkTreeSelection *selection = gtk_tree_view_get_selection(tree);
	g_signal_connect(selection, "changed", G_CALLBACK(on_archive_file_selection_changed), shell);
	return;
}

static int init_windows(struct shell_context *shell)
{
	assert(shell && shell->priv);
	struct app_context *app = shell->app;
	struct shell_private *priv = shell->priv;
	
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *header_bar = gtk_header_bar_new();
	gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);
	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	
	GtkWidget *grid = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(window), grid);
	
	GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_attach(GTK_GRID(grid), hpaned, 0, 0, 1, 1);
	
	GtkWidget *scrolled_win;
	GtkWidget *archive_files_list = gtk_tree_view_new();
	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled_win, 180, -1);
	gtk_widget_set_vexpand(scrolled_win, TRUE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(scrolled_win), archive_files_list);
	
	gtk_paned_add1(GTK_PANED(hpaned), scrolled_win);

	GtkWidget *stack = gtk_stack_new();
	GtkWidget *stack_switcher = gtk_stack_switcher_new();
	gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stack_switcher), GTK_STACK(stack));
	gtk_paned_add2(GTK_PANED(hpaned), stack);
	
	
	gtk_grid_attach(GTK_GRID(grid), stack_switcher, 0, 1, 1, 1);
	gtk_widget_set_hexpand(stack, TRUE);
	gtk_widget_set_vexpand(stack, TRUE);
	
	GtkWidget *textview = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD_CHAR);
	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_hexpand(scrolled_win, TRUE);
	gtk_widget_set_vexpand(scrolled_win, TRUE);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_win), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(scrolled_win), textview);
	gtk_stack_add_titled(GTK_STACK(stack), scrolled_win, "textview", "textview");
	
	
	GtkWidget *file_chooser = gtk_file_chooser_button_new("Open", GTK_FILE_CHOOSER_ACTION_OPEN);
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "ooxml files");
	gtk_file_filter_add_pattern(filter, "*.xlsx");
	gtk_file_filter_add_pattern(filter, "*.docx");
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(file_chooser), filter);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), app->work_dir);
	g_signal_connect(file_chooser, "file-set", G_CALLBACK(on_file_selected), shell);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), file_chooser);
	
	
	init_archive_files_list(GTK_TREE_VIEW(archive_files_list), shell);
	
	g_signal_connect_swapped(window, "destroy", G_CALLBACK(shell->stop), shell);
	
	priv->window = window;
	priv->header_bar = header_bar;
	priv->grid = grid;
	priv->archive_files_list = archive_files_list;
	priv->stack = stack;
	priv->textview = textview;
	return 0;
}
