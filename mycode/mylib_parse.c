/*
 Copyright (C) 2019 Albert Chen
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 
 Author: gentheaven@hotmail.com (Albert Chen)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mylib_parse.h"

/////////////////////////////////////////////////////////////////////////
//parse html content, return DOM tree
lxb_html_document_t* parser_init(char* buf, size_t len)
{
	lxb_status_t status;
	lxb_html_document_t* document;

	document = lxb_html_document_create();
	if (document == NULL) {
		return NULL;
	}

	status = lxb_html_document_parse(document, buf, len);
	if (status != LXB_STATUS_OK) {
		return NULL;
	}

	return document;
}

void parse_exit(lxb_html_document_t* document)
{
	if(document)
		lxb_html_document_destroy(document);
}


//<a target="_blank" href = "https://www.zhihu.com/question/5073499722/answer/40533676669"
//save url to result
int get_a_href(lxb_dom_node_t* node, char* result)
{
	lxb_dom_attr_t* attr;
	attr = lxb_dom_element_attr_by_id(lxb_dom_interface_element(node), LXB_DOM_ATTR_HREF);
	if (!attr)
		return 0;

	const char* str = lxb_dom_attr_value(attr, NULL);
	if (!str)
		return 0;
	strcpy(result, str);
	return 0;
}

int is_attr_value(lxb_dom_element_t* node, char* attr_name, char* value)
{
	lxb_dom_attr_t* attr;
	attr = lxb_dom_element_attr_by_name(node, attr_name, strlen(attr_name));
	if (!attr)
		return 0;
	const lxb_char_t* str = lxb_dom_attr_value(attr, NULL);
	if (!str)
		return 0;
	if (lexbor_str_data_casecmp(value, str))
		return 1;
	return 0;
}

/*
<div class="AuthorInfo">
    <meta itemprop="name" content="体制老司机">

	cur = find_sub_attr(node, "meta", "itemprop", "name");
	match  <meta itemprop="name" >, return this <meta> node

 return node if found, 
 return NULL if not found
*/
lxb_dom_element_t* find_sub_attr(lxb_dom_element_t* node, lxb_char_t* name, char* attr, char* value)
{
	lxb_dom_element_t* element = NULL;
	lxb_status_t status;
	lxb_dom_collection_t* collection;

	lxb_html_document_t* doc = lxb_html_element_document(lxb_html_interface_element(node));
	collection = lxb_dom_collection_make(&doc->dom_document, 16);

	//e.g. find_sub_attr(node, "meta", "itemprop", "name");
	//find all <meta> nodes
	status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(node), collection,
		(const lxb_char_t*)name, strlen(name));
	if (status != LXB_STATUS_OK) {
		lxb_dom_collection_destroy(collection, true);
		return NULL;
	}

	size_t i;
	size_t len = lxb_dom_collection_length(collection);
	int found = 0;
	for (i = 0; i < len; i++) {
		element = lxb_dom_collection_element(collection, i);
		//e.g. find_sub_attr(node, "meta", "itemprop", "name");
		//match itemprop = name
		if(is_attr_value(element, attr, value)) {
			found = 1;
			break;
		}
	}

	lxb_dom_collection_destroy(collection, true);
	if(found)
		return element;
	return NULL;
}

static lxb_status_t serializer_callback(const lxb_char_t* data, size_t len, void* ctx)
{
	FILE* fp = (FILE*)ctx;
	fwrite(data, 1, len, fp);
	return 0;
}

void serialize_node(lxb_dom_node_t* node, FILE* fp)
{
	lxb_html_serialize_tree_cb(node, serializer_callback, (void*)fp);
}


struct serial_buf_str {
	char* buf;
	int len;
	int cur_len;
};

static lxb_status_t serializer_buf_callback(const lxb_char_t* data, size_t len, void* ctx)
{
	struct serial_buf_str *sbuf = (struct serial_buf_str *)ctx;
	char* head = sbuf->buf + sbuf->cur_len;
	if ((sbuf->cur_len + len) >= sbuf->len) {
		printf("html content is too big, current buf size is %d\n", sbuf->len);
		return 1;
	}

	memcpy(head, data, len);
	head[len] = 0;
	sbuf->cur_len = sbuf->cur_len + (int)len;
	return 0;
}

void serialize_node_buf(lxb_dom_node_t* node, char* buf, int buf_len)
{
	struct serial_buf_str sbuf;
	sbuf.buf = buf;
	sbuf.len = buf_len;
	sbuf.cur_len = 0;
	lxb_html_serialize_tree_cb(node, serializer_buf_callback, (void*)&sbuf);
}

//call lxb_dom_collection_destroy(collection, true); after use
lxb_dom_collection_t* get_nodes_by_tag(lxb_dom_node_t* node, char* tag_name)
{
	lxb_status_t status;
	lxb_dom_collection_t* collection;

	lxb_html_document_t* doc;
	doc = lxb_html_interface_document(node->owner_document);
	collection = lxb_dom_collection_make(&doc->dom_document, 16);

	status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(node), collection,
		(const lxb_char_t*)tag_name, strlen(tag_name));
	if (status != LXB_STATUS_OK) {
		lxb_dom_collection_destroy(collection, true);
		return NULL;
	}
	return collection;
}

void remove_dom_element(lxb_html_document_t* doc, lxb_tag_id_t tag)
{
	char* label = NULL;
	switch (tag) {
	case LXB_TAG_SCRIPT:
	case LXB_TAG_AUDIO:
	case LXB_TAG_CANVAS:
	case LXB_TAG_VIDEO:
		label = (char*)lxb_tag_data_by_id(tag);
		break;
	default:
		return;
	}

	lxb_dom_collection_t* collection;
	collection = get_nodes_by_tag(lxb_dom_interface_node(doc), label);
	if (!collection)
		return;

	size_t i;
	size_t len = lxb_dom_collection_length(collection);
	lxb_dom_element_t* element;
	for (i = 0; i < len; i++) {
		element = lxb_dom_collection_element(collection, i);
		lxb_dom_node_remove(lxb_dom_interface_node(element));
	}
	lxb_dom_collection_destroy(collection, true);
}

