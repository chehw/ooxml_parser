/*
 * ooxml_context.c
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

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>

#include <libgen.h>
#include <zip.h>
#include <glib.h>


#include "ooxml_context.h"
#include "ooxml_private.h"

static int ooxml_open(struct ooxml_context *ooxml, const char *filename, int readonly);
static void ooxml_close(struct ooxml_context *ooxml);
static ssize_t ooxml_get_num_entries(struct ooxml_context *ooxml);
static int ooxml_get_file(struct ooxml_context *ooxml, int index, struct ooxml_zip_file *file, int fetch_data);

struct ooxml_private *ooxml_private_new(struct ooxml_context *ooxml)
{
	struct ooxml_private *priv = calloc(1, sizeof(*priv));
	assert(priv);
	priv->ooxml = ooxml;
	priv->num_entries = -1;
	
	if(ooxml) ooxml->priv = priv;
	return priv;
}
void ooxml_private_free(struct ooxml_private *priv)
{
	///< @todo
}



struct ooxml_context *ooxml_context_init(struct ooxml_context *ooxml, void *user_data)
{
	if(NULL == ooxml) ooxml = calloc(1, sizeof(*ooxml));
	assert(ooxml);
	
	ooxml->user_data = user_data;
	ooxml->open = ooxml_open;
	ooxml->close = ooxml_close;
	ooxml->get_num_entries = ooxml_get_num_entries;
	ooxml->get_file = ooxml_get_file;
	
	ooxml->priv = ooxml_private_new(ooxml);
	assert(ooxml->priv);
	
	return ooxml;
}

void ooxml_context_cleanup(struct ooxml_context *ooxml)
{
	if(NULL == ooxml) return;
	ooxml_private_free(ooxml->priv);
	ooxml->priv = NULL;
	return;
}

int parse_zip_xml_file(zip_t *zip, const char *filename, xmlDocPtr *p_doc)
{
	char buffer[4096] = "";
	ssize_t cb_data = 0;
	xmlParserCtxtPtr parser = NULL;
	
	zip_file_t *zfp = zip_fopen(zip, filename, ZIP_FL_UNCHANGED);
	if(!zfp) {
		fprintf(stderr, "zip_fopen(%s) failed: \n%s\n", 
			filename, 
			zip_strerror(zip));
		return -1;
	}
	
	// read a few bytes to check the format */
	cb_data = zip_fread(zfp, buffer, 8);
	parser = xmlCreatePushParserCtxt(NULL, NULL, buffer, cb_data, filename);
	if(NULL == parser) {
		fprintf(stderr, "xmlCreatePushParserCtxt() failed\n");
		zip_fclose(zfp);
		return -1;
	}
	
	xmlParserErrors err_code = XML_ERR_OK;
	while((cb_data = zip_fread(zfp, buffer, sizeof(buffer) - 1))  > 0)
	{
		err_code = xmlParseChunk(parser, buffer, cb_data, 0);
		if(err_code != XML_ERR_OK) {
			fprintf(stderr, "xmlParseChunk() failed.\n");
			break;
		}
	}
	zip_fclose(zfp);
	
	if(err_code == XML_ERR_OK) {
		err_code = xmlParseChunk(parser, buffer, 0, 1);
	}
	
	xmlDocPtr doc = parser->myDoc;
	int wellFormed = parser->wellFormed;
	xmlFreeParserCtxt(parser);
	
	if(wellFormed) {
		*p_doc = doc;
		return 0;
	}
	
	xmlFreeDoc(doc);
	return -1;
}


static int ooxml_open(struct ooxml_context *ooxml, const char *filename, int readonly)
{
	struct ooxml_private *priv = ooxml->priv;
	assert(priv);
	
	if(priv->archive) {
		ooxml->close(ooxml);
	}
	
	int err_code = 0;
	zip_t *zip = zip_open(filename, readonly?ZIP_RDONLY:0, &err_code);
	if(NULL == zip || err_code) {
		fprintf(stderr, "zip_open(%s) failed, err_code=%d\n", 
			filename,
			err_code);
		if(zip) zip_close(zip);
		return -1;
	}
	
	priv->archive = zip;
	return 0;
}
static void ooxml_close(struct ooxml_context *ooxml)
{
	struct ooxml_private *priv = ooxml->priv;
	assert(priv);
	if(priv->archive) {
		zip_close(priv->archive);
		priv->archive = NULL;
	}
	
	if(priv->file_stats) free(priv->file_stats);
	priv->num_entries = -1;
	priv->file_stats = NULL;
	
	return;
}
static ssize_t ooxml_get_num_entries(struct ooxml_context *ooxml)
{
	struct ooxml_private *priv = ooxml->priv;
	assert(priv);
	zip_t *zip = priv->archive;
	if(NULL == zip) return -1;
	
	if(priv->num_entries < 0) {
		ssize_t num_entries = zip_get_num_entries(zip, ZIP_FL_UNCHANGED);
		if(num_entries > 0) {
			struct zip_stat *stats = calloc(num_entries, sizeof(*stats));
			assert(stats);
			priv->file_stats = stats;
			
			for(ssize_t i = 0; i < num_entries; ++i) {
				zip_stat_index(zip, i, ZIP_FL_UNCHANGED, &stats[i]);	
			}
		}
		priv->num_entries = num_entries;
	}
	return priv->num_entries;
}

void ooxml_zip_file_clear(struct ooxml_zip_file *file)
{
	if(NULL == file) return;
	if(file->filename) free(file->filename);
	if(file->data) free(file->data);
	if(file->doc) {
		xmlFreeDoc(file->doc);
	}
	memset(file, 0, sizeof(*file));
}

static int ooxml_get_file(struct ooxml_context *ooxml, int index, struct ooxml_zip_file *p_file, int fetch_data)
{
	struct ooxml_private *priv = ooxml->priv;
	assert(priv);
	zip_t *zip = priv->archive;
	if(NULL == zip) return -1;
	if(priv->num_entries <= 0) return -1;
	if(index < 0 || index >= priv->num_entries) return -1;
	
	struct ooxml_zip_file file;
	memset(&file, 0, sizeof(file));
	
	zip_stat_t *stats = &priv->file_stats[index];
	if((stats->valid & ZIP_STAT_NAME) != ZIP_STAT_NAME) {
		fprintf(stderr, "error::ooxml_get_file_data(index=%ld): invalid data(no filename).\n", (long)index);
		return -1;
	}
	if((stats->valid & ZIP_STAT_SIZE) != ZIP_STAT_SIZE) {
		fprintf(stderr, "error::ooxml_get_file_data(%s): invalid data(no size).\n", stats->name);
		return -1;
	}
	
	file.filename = strdup(stats->name);
	file.file_length = stats->size;
	
	if(stats->valid & ZIP_STAT_MTIME) file.mtime = stats->mtime;
	if(stats->valid & ZIP_STAT_INDEX) file.index = stats->index;
	
	if(fetch_data && (file.file_length > 0)) {
		unsigned char *data = malloc(file.file_length + 1);
		assert(data);
		
		zip_file_t *zfp = zip_fopen_index(zip, index, ZIP_FL_UNCHANGED);
		assert(zfp);
		
		ssize_t cb_data = zip_fread(zfp, data, file.file_length);
		assert(cb_data == file.file_length);
		data[cb_data] = '\0';
		
		file.data = data;
		file.cb_data = cb_data;
		
		file.doc = xmlReadMemory((const char *)data, cb_data, file.filename, "utf-8", XML_PARSE_NONET);
		zip_fclose(zfp);
	}
	
	if(p_file) *p_file = file;
	else ooxml_zip_file_clear(&file);

	return 0;
}

#if defined(TEST_OOXML_CONTEXT_) && defined(_STAND_ALONE)
int main(int argc, char **argv)
{
	
	return 0;
}
#endif
