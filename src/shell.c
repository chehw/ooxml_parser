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




int shell_load_ooxml_file(struct shell_context *shell, const char *filename)
{
	debug_printf("filename: %s", filename);
	
	assert(shell && shell->app);
	struct app_context *app = shell->app;
	struct ooxml_context *ooxml = app_get_ooxml_context(app);
	assert(ooxml);

	const int readonly = 1;
	int rc = ooxml->open(ooxml, filename, readonly);
	assert(0 == rc);
	
	ssize_t num_entries = ooxml->get_num_entries(ooxml);
	debug_printf("num_entries: %ld", (long)num_entries);
	if(num_entries > 0) {
		struct ooxml_zip_file file[1];
		memset(file, 0, sizeof(file));
		
		for(ssize_t i = 0; i < num_entries; ++i) {
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
			ooxml_zip_file_clear(file);
		}
		
		
	}
	
	
	return 0;
}


#if defined(TEST_SHELL_CONTEXT_) && defined(_STAND_ALONE)
int main(int argc, char **argv)
{
	
	return 0;
}
#endif
