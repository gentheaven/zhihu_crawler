#ifndef _OS_H
#define _OS_H

#ifdef WINDOWS
#include <Windows.h>

#define pipe_open _popen
#define pipe_close _pclose
#define strncasecmp _strnicmp
#define strcasecmp stricmp
#define snprintf _snprintf

#define DEBUG_HTML_PATH "question.html"
#define DEBUG_JSON_PATH "question.json"

#define OUTPUT_PATH "temp\\"

#else //mac
#include <iconv.h>
#include <sys/types.h>

#define OUTPUT_PATH "temp/"

#ifndef WIN32 //msys2 no file wait.h
#include <sys/wait.h>
#endif
#include <pthread.h>

#define pipe_open popen
#define pipe_close pclose

#define DEBUG_HTML_PATH "./zhihu/question.html"
#define DEBUG_JSON_PATH "./zhihu/question.json"
#endif

//#define DEBUG
#define SINGLE_FILE //single file download html file

#ifdef __cplusplus
extern "C" {
#endif

/*
in: url, https://www.fanfiction.net/s/12555707/1/One-Hundred-Green-Leaves
out: website, https://www.fanfiction.net
*/
extern int get_website_from_url(char* url, char* web);

/* read url content to file 
in:
	url: point to a link
	count: in, out, buffer length
	buf: user alloc 'count' bytes buffer
out:
	new_head: point to html header
0 if read OK;
*/
extern int get_html_content(char* url, char* buf, size_t* count);

extern int test_read_file(char* url, char* buf, size_t* count);

//max convert 33 words
extern int u8_to_gb2312(char* in, char* out);
extern int gb2312_to_u8(char* in, char* out);

extern unsigned int get_local_charset(void);

extern void handle_argv(char* argv, char* author);

/*
* curl command line:
* windows: & to "&"
* mac: = ? & to \= \? \&
*/
extern void convert_exec_cmd(char* in, char* out);

extern int FileExists(const char* fileName);

extern int DirectoryExists(const char* dirPath);

#ifdef __cplusplus
}
#endif
#endif
