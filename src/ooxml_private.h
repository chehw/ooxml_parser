#ifndef OOXML_PRIVATE_H_
#define OOXML_PRIVATE_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "app.h"
#include "shell.h"
#include "ooxml_context.h"

#include <zip.h>
struct ooxml_private
{
	struct ooxml_context *ooxml;
	zip_t *archive;
	
	int num_entries;
	struct zip_stat *file_stats;
};


#ifdef __cplusplus
}
#endif
#endif

