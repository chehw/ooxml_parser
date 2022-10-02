#ifndef OOXML_CONTEXT_H_
#define OOXML_CONTEXT_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <libxml/parser.h>
#include <zip.h>

enum ooxml_file_type
{
	ooxml_file_document,
	ooxml_file_spreadsheet,
};


struct ooxml_zip_file
{
	char *filename;
	size_t file_length;
	
	unsigned char *data;
	size_t cb_data;
	
	xmlDocPtr doc;
};
void ooxml_zip_file_clear(struct ooxml_zip_file *file);


struct ooxml_private;
struct ooxml_context
{
	void *user_data;
	struct ooxml_private *priv;
	enum ooxml_file_type type;
	
	int (* open)(struct ooxml_context *ooxml, const char *filename, int readonly);
	void (*close)(struct ooxml_context *ooxml);
	
	ssize_t (*get_num_entries)(struct ooxml_context *ooxml);
	int (*get_file)(struct ooxml_context *ooxml, int index, struct ooxml_zip_file *file, int fetch_data);
};

struct ooxml_context *ooxml_context_init(struct ooxml_context *ooxml, void *user_data);
void ooxml_context_cleanup(struct ooxml_context *ooxml);


int parse_zip_xml_file(zip_t *zip, const char *filename, xmlDocPtr *p_doc);
#ifdef __cplusplus
}
#endif
#endif

