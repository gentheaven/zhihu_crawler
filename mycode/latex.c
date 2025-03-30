#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "mylib_parse.h"

static char latex_string[2048];


/*
* 明明是一行，中间有 \\
* 所以划分为多行
*/
void latex_multi_lines(char* fp, char* text)
{
	int ignore = 0;
	if (strstr(text, "matrix"))
		ignore = 1;
	else if (strstr(text, "\\begin"))
		ignore = 1;
	else if (strstr(text, "\\left"))
		ignore = 1;
	else if (strstr(text, "\\right"))
		ignore = 1;

	if (ignore) {
		sprintf(fp, "\n\\[\n%s\n\\]\n\n", text);
		return;
	}

	char* next = strstr(text, "\\\\");
	while (next) {
		*next = 0;
		sprintf(fp, "\n\\[\n%s\n\\]\n", text);
		fp = fp + strlen(text) + 8;
		text = next + 2;
		next = strstr(text, "\\\\");
	}
	//last line
	next = text + strlen(text) - 1;
	while (*next == ' ')
		next--;
	if (*next == '\\')
		*next = 0;
	if (text[0])
		sprintf(fp, "\n\\[\n%s\n\\]\n", text);
}

//{\text{Time Invariant & Scaling system }}
void write_latex_special(char* fp, char* text, char* head)
{
	char* ptr;
	ptr = strstr(text, "\\text");
	ptr = strchr(ptr, '&');
	if (ptr) {
		*ptr = 0;
		sprintf(fp, "\n\\[\n%s\\&%s\\%s\n\\]\n\n", text, ptr + 1, head);
	}
	else {
		sprintf(fp, "\n\\[\n%s \\%s\n\\]\n\n", text, head);
	}
}

//<span class="ztext-math" data-eeimg="1" data-tex=
// \[ F(x, y) = \begin {cases} 1 ... \]
static void handle_special(char* fp, char* text)
{
	char* head = strstr(text, "\\[");
	char* tail = strstr(text, "\\]");
	if (!head || !tail) {
		sprintf(fp, "\n\\[\n%s\n\\]\n\n", text);
		return;
	}

	head = head + 2;
	*tail = 0;
	sprintf(fp, "\n\\[\n%s\n\\]\n\n", head);
}

/*
* multi line
\[
	math
\]
inline: \(math\)
*/
static void write_latex(char* fp, char* text, int multiLine)
{
	char* tail, * head;
	size_t len;
	if (multiLine) {
		//\\\tag{1} to \tag{1}
		//\\  \tag{1} to \tag{1}
		tail = strstr(text, "\\tag");
		if (tail) {
			head = strstr(tail + 1, "tag");
			tail = head;
			while (1) {
				tail--;
				if (*tail == ' ' || *tail == '\\')
					continue;
				break;
			}
			*(tail + 1) = 0;
			//{\text{Time Invariant & Scaling system }}
			if (strstr(text, "\\text") && strchr(text, '&')) {
				write_latex_special(fp, text, head);
			}
			else {
				sprintf(fp, "\n\\[\n%s \\%s\n\\]\n\n", text, head);
			}
		}
		else {
			head = strstr(text, "\\\\");
			if (head)
				head = strstr(head + 2, "\\\\");

			if (head) {
				latex_multi_lines(fp, text);
			}
			else {
				head = strstr(text, "\\[");
				if (head)
					handle_special(fp, text);
				else
					sprintf(fp, "\n\\[\n%s\n\\]\n\n", text);
			}
		}
	}
	else {
		len = strlen(text);
		if (len == 1 && *text == ' ')
			return;
		sprintf(fp, " \\(%s\\) ", text);
	}
}

//a<b change to a < b, otherwise mathjax render error
static void change_latex_less_than(char* ori, size_t len)
{
	char* chg = malloc(len);

	char* head = ori;
	char* cur; //current <
	char* next = NULL; //next <
	char* left; //left part
	size_t real_len = 0;
	while (1) {
		cur = strchr(head, '<');
		if (!cur)
			break;
		left = cur + 1;
		*cur = 0;
		next = strchr(cur + 1, '<');
		if (next) {
			sprintf(chg + real_len, "%s < ", head);
			real_len = real_len + strlen(head) + 3; //" < "
			*cur = '<';
			head = left;
		} else { //last
			sprintf(chg + real_len, "%s < %s", head, left);
			*cur = '<';
			break;
		}	
	}

	strcpy(ori, chg);
	free(chg);
}

//\[f\left(x \right) \] to f\left(x \right)
//https://www.zhihu.com/question/509447730/answer/2296395900
static void change_latex_bracket(char* ori, size_t len)
{
	if (strlen(ori) > 60)
		return;

	char* head = ori;
	head = strstr(head, "\\[");
	if (!head)
		return;

	/* multiline, add \[ by code, not original
	* https://www.zhihu.com/question/1888313643034187185/answer/131391283931
	\[
		3\sqrt[3]{\lambda}-1=1-\lambda,\\
	\]
	*/
	if (head[2] == '\n')
		return;
	char* tail = strstr(head + 2, "\\]");
	if (!tail)
		return;

	//replaced with space
	head[0] = ' ';
	head[1] = ' ';
	tail[0] = ' ';
	tail[1] = ' ';
}

void latex_replace_pt(char* buf)
{
	char* tail = buf - 1;
	int cnt = 0;
	while (*tail != '[') {
		if (!isdigit(*tail))
			return;
		tail--;
		cnt++;
		if (cnt > 2)
			return;
	}
	int i;
	//[xxpt] = cnt + 4
	cnt += 4;
	for (i = 0; i < cnt; i++, tail++)
		*tail = ' ';
}

//https://www.zhihu.com/question/633696345/answer/3317192268
//remove [xxpt]
static void change_latex_pt(char* ori, size_t len)
{
	char* head = ori;

	while (head) {
		head = strstr(head, "pt]");
		if (!head)
			break;
		
		latex_replace_pt(head);
		head = head + 3;
	}
}

//\displaystyle{ ... } to { ... }
//https://www.zhihu.com/question/633696345/answer/110999101668
static void change_latex_displaystyle(char* ori, size_t len)
{
	const char display_key[] = "\\displaystyle{";
	const size_t key_len = strlen(display_key);

	char* head = strstr(ori, display_key);
	if(!head)
		return;
	memset(head, ' ', key_len);
	//\displaystyle{ ... }, find last '}'
	head = head + key_len;
	char* pre = head;
	char* last = head;
	while (1) {
		pre = strchr(pre, '{');
		if(pre) {
			last = strchr(pre, '}');
			if (!last)
				break; //???, shouldn't happen
			pre = last + 1;
			last = last + 1;
		} else { //last '}'
			last = strchr(last, '}');
			if (!last) //???, shouldn't happen
				break;
			*last = ' ';
			break;
		}
	}
}

// <span class="ztext-math" data-eeimg="1" data-tex="X(j\omega)">X(j\omega)</span>
char* create_latex_str(lxb_dom_element_t* node, size_t* len)
{
	char* extra;
	size_t val_len;

	extra = (char*)lxb_dom_element_get_attribute(node, "data-tex", strlen("data-tex"), &val_len);
	if (!extra) {
		return NULL;
	}

	int new_line = 0;
	val_len = strlen(extra);
	if (strstr(extra, "{aligned}")) {
		new_line = 1;
	}
	else if (strstr(extra, "\\\\")) {
		new_line = 1;
	}
	else if (strstr(extra, "\\large")) {
		if (val_len > 20)
			new_line = 1;
	}
	else if (val_len > 40) {
		new_line = 1;
	}

	write_latex(latex_string, extra, new_line);
	if (strchr(latex_string, '<')) {
		change_latex_less_than(latex_string, sizeof(latex_string));
	}

	//\(\[f\left(x \right) \]\) to \(f\left(x \right)\)
	change_latex_bracket(latex_string, sizeof(latex_string));

	//[xxpt]
	change_latex_pt(latex_string, sizeof(latex_string));
	//remove displaystyle{}
	change_latex_displaystyle(latex_string, sizeof(latex_string));

	if (len)
		*len = strlen(latex_string);
	return latex_string;
}

void change_node_content(lxb_dom_element_t* el)
{
	lxb_char_t* str;
	size_t str_len;
	lxb_dom_node_t* node = lxb_dom_interface_node(el);
	str = lxb_dom_node_text_content(node, &str_len);

	lxb_dom_node_t* parent;
	parent = lxb_dom_node_parent(node);
	lxb_dom_text_t* text;
	lxb_html_document_t* doc;
	doc = lxb_html_interface_document(node->owner_document);

	char* latex;
	latex = create_latex_str(el, &str_len);
	if (!latex)
		return;
	text = lxb_dom_document_create_text_node(&doc->dom_document, latex, str_len);
	if (!text)
		return;
	lxb_dom_node_insert_before(node, lxb_dom_interface_node(text));
	lxb_dom_node_destroy(lxb_dom_interface_node(el));
}

//find all Latex code and change node
//<span class=ztext-math data-eeimg=1 data-tex = h(t)>
void change_span_latex_nodes(lxb_dom_node_t* node)
{
	lxb_dom_element_t* element = NULL;
	lxb_dom_collection_t* collection;

	collection = get_nodes_by_tag(node, "span");
	if (!collection)
		return;

	size_t i;
	size_t len = lxb_dom_collection_length(collection);
	for (i = 0; i < len; i++) {
		element = lxb_dom_collection_element(collection, i);
		if (is_attr_value(element, "class", "ztext-math")) {
			change_node_content(element);
		}
	}

	lxb_dom_collection_destroy(collection, true);
}

