#ifndef _MYLIB_PARSE_H
#define _MYLIB_PARSE_H



#define BUF_SIZE (1024*1024)

#ifdef __cplusplus
extern "C" {
#endif

#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>

//parse html content, return DOM tree
extern lxb_html_document_t* parser_init(char* buf, size_t len);
extern void parse_exit(lxb_html_document_t* document);

//tools

extern void serialize_node(lxb_dom_node_t* node, FILE* fp);

/*
	<script id = "js-initialData" type = "text/json">
	is_attr_value(node, "id", "js-initialData");
	return: 1 if node has this "attr" and value is "value"
*/
extern int is_attr_value(lxb_dom_element_t* node, char* attr_name, char* value);

//save url to result
extern int get_a_href(lxb_dom_node_t* node, char* result);

/*
<div class="AuthorInfo">
	<meta itemprop="name" content="体制老司机">
cur = find_sub_attr(node, "meta", "itemprop", "name");

 return node if found,
 return NULL if not found
*/
extern lxb_dom_element_t* find_sub_attr(lxb_dom_element_t* node, lxb_char_t* name, char* attr, char* value);

//call lxb_dom_collection_destroy(collection, true); after use
extern lxb_dom_collection_t* get_nodes_by_tag(lxb_dom_node_t* node, char* tag_name);

//find all Latex code and change node
//<span class=ztext-math data-eeimg=1 data-tex = h(t)>
extern void change_span_latex_nodes(lxb_dom_node_t* node);

extern void remove_dom_element(lxb_html_document_t* doc, lxb_tag_id_t tag);

#ifdef __cplusplus
}
#endif
#endif
