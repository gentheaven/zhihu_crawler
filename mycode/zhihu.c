#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "mylib_parse.h"

#include "webui.h"
#include "os.h"


/*
./zhihu url-path : output txt file
./zhihu url-path : output html
*/

static char read_buf[BUF_SIZE];
static char json_data[BUF_SIZE];

static char title_all[2048] = "title";
static char glink_str[2048]; //question url

static char test_link_name[] = 
	"https://www.zhihu.com/question/431824510/answer/1646701379";
static char *g_url = NULL;

static char gauthor_name[256]; //should be utf-8 format
lxb_html_document_t* gdoc = NULL;
static int answer_or_question = 0;

static int total_answer_cnt = 0;
static int current_answer_index = 0;
#define MAX_ANSWERS 24
static lxb_dom_element_t* all_answers[MAX_ANSWERS]; //max 12 answers

static void write_html_content(lxb_dom_element_t* answer);

static void reset_variables(void)
{
	current_answer_index = 0;
	total_answer_cnt = 0;
}

static void free_doc(void)
{
	if (gdoc)
		parse_exit(gdoc);
	gdoc = NULL;
	total_answer_cnt = 0;
	answer_or_question = 0;
}

static void write_html_head(FILE* fp, char* title)
{
	fprintf(fp, "<!DOCTYPE html>\n");
	fprintf(fp, "<html>\n");

	fprintf(fp, "<head>\n");
	fprintf(fp, "<meta charSet=\"utf-8\"/>\n");
	fprintf(fp, "<title> %s </title>\n", title);
	fprintf(fp, "<script id=\"MathJax - script\" async src=\"https://cdn.jsdmirror.com/npm/mathjax@3/es5/tex-mml-chtml.js\"></script>");
	fprintf(fp, "</head>\n\n");

	fprintf(fp, "<body>\n");
	
}

static void write_html_tail(FILE* fp)
{
	fprintf(fp, "</body>\n");
	fprintf(fp, "</html>\n");
}

/*
 * output_dir: e.g. temp
 * file_name: e.g. 1.jpg
 * final output: temp/1.jpg or temp\1.jpg
 * path: url address
 * */
void exec_curl(char* output_dir, char* file_name, char* path)
{
	char cmd[512];

	sprintf(cmd, "%s%s", output_dir, file_name);
	if (FileExists(cmd))
		return;

#if defined(WINDOWS)
	sprintf(cmd, "curl -o %s\\%s %s", output_dir, file_name, path);
#else
	sprintf(cmd, "curl -o %s/%s %s", output_dir, file_name, path);
#endif

	system(cmd);
}


/*
download figure and modify path
<figure>
	<noscript>
		<img src="https://xx.jpg?source=1940ef5c" data-rawwidth="1024"/>
	</noscript>
	<img src="data:image/svg+xml;/>
</figure>

modified to:
<figure> <img src="1.jpg"/> </figure>

*/
static void download_image(FILE* fp, lxb_dom_node_t* node)
{
	char* path;
	char* temp;
	char file_name[256];
	int len;
	lxb_dom_collection_t* sub_collection;

	sub_collection = get_nodes_by_tag(node, "img");
	if (!sub_collection)
		return;

	lxb_dom_element_t* element = lxb_dom_collection_element(sub_collection, 0);
	lxb_dom_collection_destroy(sub_collection, true);

	if (!element)
		return;
	//only care <img src="xxx.jpg">
	size_t value_len;
	path = (char*)lxb_dom_element_get_attribute(element, "src", strlen("src"), &value_len);
	if (!path)
		return;
	if (strncmp(path, "https", strlen("https")))
		return;
	temp = strstr(path, "jpg");
	if (!temp)
		return;
	//https://pic2.zhimg.com/50/v2-cf4f9a768fa23924d207f69994b5dbe9_hd.jpg
	len = 0;
	while (*temp != '/') {
		temp--;
		len++;
	}
	temp++;
	len += 2; //jpg
	strncpy(file_name, temp, len);
	file_name[len] = 0;
	//here: <img src="xxx.jpg">, download this jpg file
	//curl -o 1.jpg path
	temp = strstr(path, "jpg") + 3;
	*temp = 0;
	fprintf(fp, "<figure> <img src = \"%s\" /> </figure>", file_name);
	exec_curl(OUTPUT_PATH, file_name, path);
}

/*
 <ul>
	<li data-pid="sf0I4YBa">操作系统: Linux/Windows (推荐 Ubuntu 20.04+)<br> </li>
	<li data-pid="dAD02Yj9">Python: 3.8+<br> </li>
	<li data-pid="Aug9wbMu"><span><a class
	<li data-pid="4IteDlwe">GPU: 显存 ≥16GB (Janus-Pro-7B需≥24GB)<br> </li>
	<li data-pid="7RgLuVNM">存储空间: ≥30GB 可用空间<br> </li>
</ul>
*/
void handle_ul(FILE* fp, lxb_dom_node_t* node)
{
	lxb_dom_collection_t* collection;
	collection = get_nodes_by_tag(node, "li");
	if (!collection)
		return;

	lxb_char_t* tag_name;
	size_t str_len;
	tag_name = (lxb_char_t*)lxb_dom_node_name(node, &str_len);
	if (!tag_name) {
		lxb_dom_collection_destroy(collection, true);
		return;
	}

	fprintf(fp, "\n<%s>\n", tag_name);

	size_t i;
	size_t len = lxb_dom_collection_length(collection);
	lxb_dom_element_t* element;
	lxb_char_t* str;
	for (i = 0; i < len; i++) {
		element = lxb_dom_collection_element(collection, i);
		str = lxb_dom_node_text_content(lxb_dom_interface_node(element), &str_len);
		if (str)
			fprintf(fp, "<li>%s</li>\n", str);
	}
	fprintf(fp, "</%s>\n", tag_name);

	lxb_dom_collection_destroy(collection, true);
}


//<code class="language-js">
void handle_code(FILE* fp, lxb_dom_node_t* node)
{
	size_t str_len;
	lxb_char_t* str;
	int js_flag = 0;

	str = (lxb_char_t*)lxb_dom_element_get_attribute(lxb_dom_interface_element(node),
		"class", strlen("class"), &str_len);
	if (str && lexbor_str_data_casecmp(str, "language-js")) {
		js_flag = 1;
	}

	str = lxb_dom_node_text_content(node, &str_len);
	if (!str)
		return;
	if (js_flag) {
		char* temp;
		temp = strstr(str, "<script>");
		if (temp)
			memcpy(temp, "<header>", strlen("<header>"));
		temp = strstr(str, "</script>");
		if (temp)
			memcpy(temp, "</header>", strlen("</header>"));
	}

	//CR LF to <br>
	char* end = str;
	char* head = end;
	while(*end) {
		if (*end == '\n') {
			*end = 0;
			fprintf(fp, "%s<br>", head);
			head = end + 1;
		}
		end++;
	}

	//last line
	fprintf(fp, "%s<br>", head);
}

void handle_hx(FILE* fp, lxb_dom_node_t* node)
{
	lxb_char_t* str;
	lxb_char_t* tag_name;
	size_t str_len;

	str = lxb_dom_node_text_content(node, &str_len);
	tag_name = (lxb_char_t*)lxb_dom_node_name(node, &str_len);
	if (str && tag_name) {
		fprintf(fp, "<%s>%s</%s>", tag_name, str, tag_name);
	}
}


/*
* https://www.zhihu.com/question/505806181/answer/2270814119
<p data-pid="d7gDKpxF">xxx</p>

<div>
	<a target="_blank" href="xxx" data-text="xxx" ></a>
</div>

<p data-pid="CvSrNSaH">2021/12/26 xxx </p>
*/
void handle_div_ahref(FILE* fp, lxb_dom_node_t* node)
{
	lxb_dom_node_t* prev;
	prev = lxb_dom_node_prev(node);
	if (!prev)
		return;
	lxb_tag_id_t tag = lxb_dom_node_tag_id(prev);
	switch (tag) {
	case LXB_TAG_P:
	case LXB_TAG_BLOCKQUOTE:
	case LXB_TAG_H1:
	case LXB_TAG_H2:
	case LXB_TAG_H3:
	case LXB_TAG_DIV:
		break;
	default:
		return;
	}

	lxb_dom_collection_t* link;
	link = get_nodes_by_tag(node, "a");
	if (!link)
		return;

	size_t len = lxb_dom_collection_length(link);
	lxb_dom_element_t* ela;

	lxb_char_t* str;
	size_t str_len;
	lxb_char_t* text;
	size_t i;

	for (i = 0; i < len; i++) {
		ela = lxb_dom_collection_element(link, i);
		str = (lxb_char_t*)lxb_dom_element_get_attribute(ela, "href", 4, &str_len);
		if (strstr(str, "https://zhida.zhihu.com/search?"))
				continue;
		text = (lxb_char_t*)lxb_dom_element_get_attribute(ela, "data-text", 9, &str_len);
		if (str) {
			if (text)
				fprintf(fp, "<a href=\"%s\">%s</a>\n", str, text);
			else
				fprintf(fp, "<a href=\"%s\">%s</a>\n", str, str);
		}
	}

	lxb_dom_collection_destroy(link, true);
	fprintf(fp, "<br>\n");
}

/*
<meta itemProp="url" content="url"/>
	...
<div class="RichContent RichContent--unescapable">
	<div class="RichContent-inner">
	...
	<figure> special handle <figure>, download pictures and modify src path
        <img src="1.jpg"/>
    </figure>
	other part, just copy
</div>
</div>

Recursion way to get all content and write to fp
*/
static void write_html_body(FILE* fp, lxb_dom_node_t* node)
{
	lxb_tag_id_t tag = lxb_dom_node_tag_id(node);
	switch (tag) {
	case LXB_TAG_P:
		serialize_node(node, fp);
		break;
	case LXB_TAG_BLOCKQUOTE:
		serialize_node(node, fp);
		break;
	case LXB_TAG_CODE:
		handle_code(fp, node);
		break;
	case LXB_TAG_FIGURE:
		//download figure and modify path
		download_image(fp, node);
		break;

	case LXB_TAG_H1:
	case LXB_TAG_H2:
	case LXB_TAG_H3:
		handle_hx(fp, node);
		break;

	case LXB_TAG_DIV:
		handle_div_ahref(fp, node);
		break;
	case LXB_TAG_TABLE:
		serialize_node(node, fp);
		break;
	case LXB_TAG_UL:
	case LXB_TAG_OL:
		serialize_node(node, fp);
		break;
	case LXB_TAG_HR:
		fprintf(fp, "<hr>\n");
		break;
	default:
		break;
	}
}


/*
 <div class="Question-main">
 ...
<div class="AuthorInfo">
	<meta itemprop="name" content="吾牧之">
*/
int ondiv_author(lxb_dom_element_t* node, FILE* fp)
{
	char* value;
	lxb_dom_element_t* cur = node;

	//1. locate <div class="AuthorInfo">
	cur = find_sub_attr(cur, "div", "class", "AuthorInfo");
	if (!cur)
		return 0;

	//2. found author name
	//<meta itemprop="name">
	cur = find_sub_attr(cur, "meta", "itemprop", "name");
	if (!cur)
		return 0;

	//<meta itemprop="name" content="吾牧之">
	size_t value_len;
	value = (char*)lxb_dom_element_get_attribute(cur, "content", strlen("content"), &value_len);
	if (!value)
		return 0;

	strcpy(gauthor_name, value);
	if(fp)
		fprintf(fp, "<p><b>%s</b></p>\n", value);

	return 0;
}

//<meta itemprop="url" content="https://www.zhihu.com/people/lancelu">
//<meta itemprop="url" content="https://www.zhihu.com/question/5073499722/answer/40533676669">
void get_real_url(lxb_dom_element_t* node, FILE* fp)
{
	char* str;
	size_t str_len;

	//<div class="ContentItem AnswerItem">
	lxb_dom_element_t* url;
	lxb_dom_collection_t* sub_collection;
	sub_collection = get_nodes_by_tag(lxb_dom_interface_node(node), "meta");
	if (!sub_collection)
		return;

	size_t i;
	size_t len = lxb_dom_collection_length(sub_collection);
	for (i = 0; i < len; i++) {
		url = lxb_dom_collection_element(sub_collection, i);
		if (!is_attr_value(url, "itemprop", "url"))
			continue;
		str = (char*)lxb_dom_element_get_attribute(url, "content", strlen("content"), &str_len);
		if (!str)
			continue;
		if(strstr(str, "https://www.zhihu.com/people"))
			continue; //skip it
		strcpy(glink_str, str);
		if (fp)
			fprintf(fp, "<p>%s</p>\n", str);
		break;
	}
	lxb_dom_collection_destroy(sub_collection, true);	
}

/*
<meta itemprop="dateModified" content="2024-11-25T12:07:56.000Z">
href="https://www.zhihu.com/question/5073499722/answer/40533676669">
<span data-tooltip="发布于 2024-11-25 20:07" aria-label="发布于 2024-11-25 20:07">发布于 2024-11-25 20:07</span></a>
・IP 属地山东</div>
*/
int ondiv_release_date(lxb_dom_element_t* node, FILE* fp)
{
	lxb_dom_element_t* cur;
	//1. locate <meta itemprop="dateModified">
	cur = find_sub_attr(node, "meta", "itemprop", "dateModified");
	if (!cur)
		return 0;

	//content="2024-11-25T12:07:56.000Z"
	char* str;
	size_t str_len;
	str = (char*)lxb_dom_element_get_attribute(cur, "content", strlen("content"), &str_len);
	if (!str)
		return 0;
	//convert time from 2024-11-25T12:07 to 2024-11-25 20:07
	char chg_str[256];
	strcpy(chg_str, str);
	char *head, *tail;
	head = chg_str;
	tail = strchr(head, 'T');
	if (!tail)
		return 0;
	*tail = ' ';
	head = tail + 1;

	tail = strchr(head, ':');
	if (!tail)
		return 0;
	*tail = 0;
	int clock = atoi(head);
	clock += 8;
	_itoa(clock, head, 10);
	*tail = ':';

	head = tail + 1;
	tail = strchr(head, ':');
	if (!tail)
		return 0;
	
	*tail = 0;
	if (fp)
		fprintf(fp, "<p>release time: %s</p><hr>\n", chg_str);

	get_real_url(node, fp);
	return 0;
}

//only care <p> txt </p> and image
//value = lxb_dom_node_text_content(question, &val_len);
lexbor_action_t interest_walker(lxb_dom_node_t* node, void* ctx)
{
	switch (node->type) {
	case LXB_DOM_NODE_TYPE_ELEMENT:
	case LXB_DOM_NODE_TYPE_TEXT:
		break;
	default: //skip others
		return LEXBOR_ACTION_OK;
	}

	FILE* fp = (FILE*)ctx;
	write_html_body(fp, node);
	return LEXBOR_ACTION_OK;
}

/*
问答网页 https://www.zhihu.com/question/546101323/answer/124738434968
除了原回答之外，以下是其它回答
 <div class="Card MoreAnswers">
	<div class="List" data-zop-feedlist="true">
		<div class="List-header">
			<h4 class="List-headerText">更多回答</h4>
			<div class="List-headerOptions"></div>
		</div>

		1th extra answer
		<div class="List-item" tabindex="0">
			<div>
				<div class="ContentItem AnswerItem">

		2nd extra answer
		 <div class="List-item" tabindex="0">
			<div>
				<div class="ContentItem AnswerItem">
		...

问题网页 https://www.zhihu.com/question/546101323
第1个回答
<div class="List-item" tabindex="0">
	<div>
		<div class="ContentItem AnswerItem" data-za-index="0" data-zop= >
第2个回答
<div class="List-item" tabindex="0">
	<div>
		<div class="ContentItem AnswerItem" data-za-index="1" data-zop= >
第3个回答
<div class="List-item" tabindex="0">
	<div>
		<div class="ContentItem AnswerItem" data-za-index="2" data-zop= >
*/
void get_more_answers(lxb_html_document_t* doc)
{
	lxb_dom_element_t* question;
	if (answer_or_question) {
		question = find_sub_attr(lxb_dom_interface_element(doc),
			"div", "class", "Card MoreAnswers");
	} else {
		total_answer_cnt = -1;
		question = find_sub_attr(lxb_dom_interface_element(doc),
			"div", "class", "Question-main");
	}
	if (!question) //no more answers
		return;

	//<div class="ContentItem AnswerItem">
	lxb_dom_element_t* answer;
	lxb_dom_collection_t* sub_collection;
	sub_collection = get_nodes_by_tag(lxb_dom_interface_node(question), "div");
	if (!sub_collection)
		return;

	size_t i;
	size_t len = lxb_dom_collection_length(sub_collection);
	for (i = 0; i < len; i++) {
		answer = lxb_dom_collection_element(sub_collection, i);
		if (is_attr_value(answer, "class", "ContentItem AnswerItem")) {
			total_answer_cnt++;
			if (total_answer_cnt >= MAX_ANSWERS) //ignore other answer
				break;
			all_answers[total_answer_cnt] = answer;
		}
	}
	lxb_dom_collection_destroy(sub_collection, true);
}

/*
https://www.zhihu.com/question/546101323/answer/124738434968 answer_or_question = 1
https://www.zhihu.com/question/546101323 answer_or_question = 0

<body aria-status="false" >
	<a id="ariaTipText" role="pagedescription">
		<img src="https://www.zhihu.com/question/546101323">
	</a>

<body aria-status="false" >
	<a id="ariaTipText" role="pagedescription">
		<img src="https://www.zhihu.com/question/546101323/answer/124738434968">
	</a>
*/
void get_html_type(lxb_html_document_t* doc)
{
	answer_or_question = 0; //default is answer
	lxb_html_body_element_t* body;
	if (!doc)
		return;
	body = lxb_html_document_body_element(doc);
	if (!body)
		return;

	lxb_dom_element_t* question;
	question = find_sub_attr(lxb_dom_interface_element(doc), "a", "id", "ariaTipText");
	if (!question)
		return;

	lxb_dom_node_t* img;
	img = lxb_dom_node_first_child(lxb_dom_interface_node(question));
	if (!img)
		return;
	//<img src="https://www.zhihu.com/question/546101323/answer/124738434968">
	lxb_tag_id_t tag = lxb_html_element_tag_id(lxb_html_interface_element(img));
	if (tag != LXB_TAG_IMG)
		return;
	lxb_char_t* value;
	size_t val_len;
	value = (lxb_char_t*)lxb_dom_element_get_attribute(lxb_dom_interface_element(img),
		"src", strlen("src"), &val_len);
	if (!value)
		return;
	if (strstr(value, "answer"))
		answer_or_question = 1; //is answer
	else 
		answer_or_question = 0; //is question
}

void change_html_string_single(char* html_content, char* name)
{
	char* head = html_content;
	char keywords[16];
	char change_to[16];
	sprintf(keywords, "&lt;%s&gt;", name);
	sprintf(change_to, "   \"%s\"   ", name);

	size_t len = strlen(change_to);
	while (head) {
		head = strstr(head, keywords);
		if (!head)
			break;
		memcpy(head, change_to, len);
		head = head + len;
	}
}

/*
<audio> to "audio"
<video> to "video"
canvas> to "canvas"
*/
void change_html_string(char* html_content)
{
	change_html_string_single(html_content, "audio");
	change_html_string_single(html_content, "video");
	change_html_string_single(html_content, "canvas");
}

int parse_zhihu_answer(char* buf, size_t len, FILE* fp)
{
	change_html_string(buf);

	lxb_html_document_t* doc;
	doc = parser_init(buf, len);
	if (!doc)
		return -1;

	//free last document
	free_doc();
	gdoc = doc;
	get_html_type(doc);

	lxb_char_t* value;
	size_t val_len;
	//get title
	value = (lxb_char_t*)lxb_html_document_title(doc, &val_len);
	if (value) {
		strcpy(title_all, value);
	}

	//goto <div class="Question-main">
	lxb_dom_element_t* question;
	question = find_sub_attr(lxb_dom_interface_element(doc), "div", "class", "Question-main");
	if (!question) {
		goto exit;
	}

	//get txt and image info
	//<div class="ContentItem AnswerItem"
	question = find_sub_attr(question, "div", "class", "ContentItem AnswerItem");
	if (!question) {
		goto exit;
	}
	all_answers[0] = question;
	write_html_content(question);

	get_more_answers(doc);

exit:
	//parse_exit(doc);
	return 0;
}

void save_as_html(char* buf, int len, char* name)
{
	strcpy(glink_str, test_link_name);
	g_url = glink_str;

	char cmd[1024];
	if (!DirectoryExists(OUTPUT_PATH)) {
		sprintf(cmd, "mkdir %s", OUTPUT_PATH);
		system(cmd);
	}

	char file_name[1024];
	// temp\out.html
	sprintf(file_name, "%sout.html", OUTPUT_PATH);
	FILE* fp_html = NULL;
	fp_html = fopen(file_name, "w");
	parse_zhihu_answer(buf, len, fp_html);
	fclose(fp_html);
}

// JavaScript: list_chapters_call_c(contents, file_size, title, author, regex, flag)
void parser_zhihu_answers(webui_event_t * e)
{
	char* content = (char*)webui_get_string_at(e, 0);
	int size = (int)webui_get_int_at(e, 1);
	char* title = (char*)webui_get_string_at(e, 2);

	reset_variables();
	save_as_html(content, size, title);

	char file_name[1024];
	//total answer number, current answer index, author name, output path
	sprintf(file_name, "%d,%d,%s,%sout.html",
		total_answer_cnt + 1, current_answer_index + 1,
		gauthor_name, OUTPUT_PATH);
	webui_return_string(e, file_name);
}

void write_html_content(lxb_dom_element_t* answer)
{
	char file_name[1024];
	// temp\out.html
	sprintf(file_name, "%sout.html", OUTPUT_PATH);
	FILE* fp_html = NULL;
	fp_html = fopen(file_name, "w");

	write_html_head(fp_html, title_all);
	fprintf(fp_html, "<h1>%s</h1>\n", title_all);
	ondiv_author(answer, fp_html);
	ondiv_release_date(answer, fp_html);

	change_span_latex_nodes(lxb_dom_interface_node(answer)); //latex code
	lxb_dom_node_simple_walk(lxb_dom_interface_node(answer), interest_walker, (void*)fp_html);
	write_html_tail(fp_html);

	fclose(fp_html);
}
void get_next_answer(webui_event_t* e)
{
	char file_name[1024];
	if (total_answer_cnt == 0)
		goto exit; //only one answer

	current_answer_index++;
	if (current_answer_index > total_answer_cnt)
		current_answer_index = 0;

	lxb_dom_element_t* answer;
	answer = all_answers[current_answer_index];
	write_html_content(answer);

exit:
	sprintf(file_name, "%d,%d,%s,%sout.html",
		total_answer_cnt + 1, current_answer_index + 1,
		gauthor_name, OUTPUT_PATH);
	webui_return_string(e, file_name);
}

int main(int argc, char** argv)
{
	// Create a new window
	size_t MainWindow = webui_new_window();

	// Bind HTML elements with the specified ID to C functions
	webui_bind(MainWindow, "parser_zhihu_answers", parser_zhihu_answers);
	webui_bind(MainWindow, "get_next_answer", get_next_answer);

	// Show the window, preferably in a chromium based browser
	webui_show(MainWindow, "zhihu_answer.html");

	// Wait until all windows get closed
	webui_wait();
	// Free all memory resources (Optional)
	webui_clean();

	free_doc();

	return 0;
}

int WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return main(1, NULL);
}
