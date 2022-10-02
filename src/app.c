/*
 * app.c
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

#include <unistd.h>
#include <limits.h>
#include <getopt.h>
#include <libgen.h>
#include <json-c/json.h>

#include <zip.h>
#include <gtk/gtk.h>

#include "app.h"
#include "shell.h"
#include "ooxml_context.h"

static int app_init(struct app_context *app, const char *conf_file);
static int app_run(struct app_context *app);
static int app_stop(struct app_context *app);

/******************************************************************************
 * app_private
******************************************************************************/
struct app_private
{
	struct app_context *app;
	char *conf_file;
	
	int argc;		// num_unparsed_args
	char **argv;	// unparsed_args
	
	char app_path[PATH_MAX];
	char work_dir[PATH_MAX];
	
	struct ooxml_context *ooxml;
};
struct ooxml_context *app_get_ooxml_context(struct app_context *app)
{
	assert(app && app->priv);
	struct app_private *priv = app->priv;
	return priv->ooxml;
}
static void print_usuages(struct app_context *app)
{
	fprintf(stderr, "Usuage: %s [--conf=<conf/app.json>]\n", app->app_name);
}
static int app_private_parse_args(struct app_private *priv, int argc, char **argv)
{
	static struct option options[] = {
		{"conf", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{NULL},
	};
	
	const char *conf_file = NULL;
	while(1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "c:h", options, &option_index);
		if(c == -1) break;
		
		switch(c) {
		case 'c': 
			conf_file = optarg; 
			break;
		
		case 'h':
		default:
			print_usuages(priv->app);
			if(c != 'h') {
				fprintf(stderr, "unknown args '%c'(%.2x)\n", c, (unsigned char)c);
				exit(1);
			}
		}
		if(c == -1) break;	
	}
	if(NULL == conf_file) conf_file = "conf/app.json";
	priv->conf_file = strdup(conf_file);
	
	if(optind < argc)
	{
		priv->argc = argc - optind;
		priv->argv = &argv[optind];
	}
	
	// debug print
	for(int i = 0; i < priv->argc; ++i)
	{
		fprintf(stderr, "argv[%d]: %s\n", i, priv->argv[i]);
	}
	return 0;
}
static struct app_private *app_private_new(struct app_context *app, int argc, char **argv)
{
	struct app_private *priv = calloc(1, sizeof(*priv));
	assert(priv);
	priv->app = app;
	
	// get real path
	char *app_path = realpath("/proc/self/exe", priv->app_path);
	if(NULL == app_path) {
		perror("realpath()");
		exit(1);
	}else {
		printf("real path: %s\n", app_path);
	}
	
	app->app_path = app_path;
	app->app_name = basename(priv->app_path);
	
	char *work_dir = dirname(priv->app_path);
	assert(work_dir);
	strncpy(priv->work_dir, work_dir, sizeof(priv->work_dir));
	app->work_dir = priv->work_dir;
	
	int rc = app_private_parse_args(priv, argc, argv);
	assert(0 == rc);
	return priv;
}
static void app_private_free(struct app_private *priv)
{
	///< @todo
}

/******************************************************************************
 * app context
******************************************************************************/
struct app_context *app_context_init(struct app_context *app, int argc, char **argv, void *user_data)
{
	if(NULL == app) app = calloc(1, sizeof(*app));
	assert(app);
	
	app->user_data = user_data;
	app->init = app_init;
	app->run = app_run;
	app->stop = app_stop;
	
	struct app_private *priv = app_private_new(app, argc, argv);
	assert(priv);
	app->priv = priv;
	
	gtk_init(&priv->argc, &priv->argv);
	
	priv->ooxml = ooxml_context_init(NULL, app);
	assert(priv->ooxml);
	
	
	
	struct shell_context *shell = shell_context_init(NULL, app);
	assert(shell);
	app->shell = shell;
	
	return app;
	
}
void app_context_cleanup(struct app_context *app)
{
	if(NULL == app) return;
	app_private_free(app->priv);
	///< @todo
}


/******************************************************************************
 * app_context::virtual functions
******************************************************************************/
static int app_init(struct app_context *app, const char *conf_file)
{
	assert(app && app->priv);
	struct app_private *priv = app->priv;
	if(NULL == conf_file) conf_file = priv->conf_file;
	json_object *jconfig = json_object_from_file(conf_file);
	if(jconfig) {
		app->jconfig = jconfig;
	}
	
	struct shell_context *shell = app->shell;
	
	if(shell) {
		return shell->init(shell, jconfig);
	}
	
	fprintf(stderr, "no shell\n");
	return 0;
}
static int app_run(struct app_context *app)
{
	int rc = -1;
	struct shell_context *shell = app->shell;
	if(shell) rc = shell->run(shell);
	return rc;
}

static int app_stop(struct app_context *app)
{
	int rc = -1;
	struct shell_context *shell = app->shell;
	if(shell) {
		rc = shell->stop(shell);
	}
	return rc;
}

/******************************************************************************
 * main
******************************************************************************/
#include <signal.h>
static struct app_context g_app[1];
static void on_signal(int sig)
{
	switch(sig) {
	case SIGINT: case SIGUSR1:
		fprintf(stderr, "stopped by signal (%d)\n", sig);
		g_app->stop(g_app);
		return;
	default:
		break;
	}
	exit(sig);
}

int main(int argc, char **argv)
{
	signal(SIGINT, on_signal);
	signal(SIGUSR1, on_signal);
	
	pid_t pid = getpid();
	struct app_context *app = app_context_init(g_app, argc, argv, NULL);
	assert(app);
	
	fprintf(stderr,
		"== app.name: %s\n"
		"== work_dir: %s\n"
		"== process id: %ld\n", 
		app->app_name, app->work_dir, (long)pid);
	int rc;
	rc = app->init(app, NULL);
	if(0 == rc) rc = app->run(app);
	
	app_context_cleanup(app);
	return rc;
}

