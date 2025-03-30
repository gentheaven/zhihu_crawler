#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "os.h"

#if defined(WINDOWS)
#include "dirent_win.h"
#else
#include <dirent.h>
#endif

int test_read_file(char* url, char* buf, size_t* count)
{
	FILE* fd;
	size_t len;
	fd = pipe_open(url, "r"); 
	if (!fd) {
		return -1;
	}
	len = fread(buf, 1, *count, fd);
	pipe_close(fd);

	if (len == *count) {
		printf("file %s size is larger than %zu\n", url, *count);
		return -1;
	}
	*count = len;
	buf[len] = 0;
	//printf("%s\n",buf);
	return 0;
}


int get_website_from_url(char* url, char* web)
{
	int len;
	char* head;

	printf("url is %s\n", url);
	/* get website from url */
	head = strstr(url, "://");
	if (!head) {
		return -1;
	}
	head = head + strlen("://");
	head = strchr(head, '/');
	if (!head)
		return -1;
	len = (int)(head - url);
	strncpy( web, url, len);
	*(web + len) = 0;

	return 0;
}

/* read url content to file 
in:
	url: point to a link
	count: in, out, buffer length
	buf: user alloc 'count' bytes buffer
0 if read OK;
*/
int get_html_content(char* url, char* buf, size_t* count)
{
	char cmd[1024];
	size_t len = *count;

	sprintf(cmd, "curl %s", url);

#if defined(WINDOWS)
	strcat(cmd, " 2> nul");
#elif defined(WIN32) //msys2
	//do nothing
#else
	strcat(cmd, " 2> /dev/null");
#endif
	//printf("%s\n", cmd);
	if (test_read_file(cmd, buf, &len)) {
		return -1;
	}

	*count = len;
	return 0;
}

#if defined(WINDOWS)
//max convert 33 words
int u8_to_gb2312(char* in, char* out)
{
	return 0;
}

//windows: GB2312 to UTF8
int gb2312_to_u8(char* in, char* out)
{
	int len;
	char mid[100];
	len = (int)strlen(in);

	//in to mid
	//Maps a character string to a UTF-16 (wide character) string
	len = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)in, -1, NULL, 0);
	MultiByteToWideChar(CP_ACP, 0, (LPCSTR)in, -1, (LPWSTR)mid, len);

	//mid to out
	//Maps a UTF-16 (wide character) string to a new character string. 
	len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)mid, -1, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)mid, -1, (LPSTR)out, len, NULL, NULL);
	return len;
}

/* if local is UTF-8, return 0
	else return 1(GB2312)
*/
unsigned int get_local_charset(void)
{
	//CPINFOEX info;
	//GetCPInfoEx (CP_ACP, 0, &info);
	if (GetACP() == 936) { //CP936 is GB2312
		return 1;
	}
	return 0;
}
/* gb2312 to utf-8 */
//u8_to_gb2312(pstr->cn_num, mid);

#else
//mac
#include <locale.h>
/* if local is UTF-8, return 0
	else return 1(GB2312)
*/
unsigned int get_local_charset(void)
{
	char* charset;
	//zh_CN.UTF-8
	charset = setlocale(LC_CTYPE, "");
	if (!strncasecmp(charset, "zh_CN.UTF-8", strlen("zh_CN.UTF-8")))
		return 0;
	return 1;
}
int code_convert(char* from_charset, char* to_charset,
	char* inbuf, size_t inlen, char* outbuf, size_t outlen)
{
	iconv_t cd;
	char** pin = &inbuf;
	char** pout = &outbuf;

	cd = iconv_open(to_charset, from_charset);
	if (cd < 0)
		return -1;
	memset(outbuf, 0, outlen);
	if (iconv(cd, pin, &inlen, pout, &outlen) == -1)
		return -1;

	iconv_close(cd);
	return 0;
}

int u8_to_gb2312(char* in, char* out)
{
	int ret;
	int len;
	len = strlen(in) + 1;
	ret = code_convert("UTF-8", "GB2312", in, len, out, len);
	return ret;
}

int gb2312_to_u8(char* in, char* out)
{
	int ret;
	int len;
	len = strlen(in) / 2 * 3 + 1;
	ret = code_convert("GB2312", "UTF-8", in, len, out, len);
	return ret;
}
#endif

void handle_argv(char* argv, char* author)
{
	if (!get_local_charset()) {
		printf("system charset is UTF-8\n");
		printf("author(utf-8) is %s\n\n", argv);
		strcpy(author, argv);
		return;
	}
	printf("system charset is CP936/GB2312\n");
	gb2312_to_u8(argv, author);
}

/*
* curl command line:
* windows: & to "&"
* mac: = ? & to \= \? \&
*/
void convert_exec_cmd(char* in, char* out)
{
	char* head, * tail;
	size_t bytes = 0;

	head = in;
	tail = strchr(head, '?');
	if (tail) {
		*tail = 0;
#if defined(WINDOWS)
		sprintf(out, "%s?", head);
#else
		sprintf(out, "%s\?", head);
#endif
		bytes = strlen(out);
	}

	*tail = '?';
	head = tail + 1;
	tail = strchr(head, '=');
	while (tail) {
		*tail = 0;

#if defined(WINDOWS)
		sprintf(out + bytes, "%s=", head);
		bytes = bytes + strlen(head) + 1;
#else
		sprintf(out + bytes, "%s\\=", head);
		bytes = bytes + strlen(head) + 2;
#endif

		*tail = '=';
		head = tail + 1;
		tail = strchr(head, '&');
		if (!tail) {
			//till end
			sprintf(out + bytes, "%s", head);
			break;
		}
		*tail = 0;
#if defined(WINDOWS)
		sprintf(out + bytes, "%s\"&\"", head); //"&"
		bytes = bytes + strlen(head) + 3;
#else
		sprintf(out + bytes, "%s\\&", head); //\&
		bytes = bytes + strlen(head) + 2;
#endif
		* tail = '&';
		head = tail + 1;
		tail = strchr(head, '=');
	}
}

// Check if the file exists
int FileExists(const char* fileName)
{
	int result = 0;

#if defined(WINDOWS)
	if (_access(fileName, 0) != -1) result = 1;
#else
	if (access(fileName, F_OK) != -1) result = 1;
#endif

	// NOTE: Alternatively, stat() can be used instead of access()
	//#include <sys/stat.h>
	//struct stat statbuf;
	//if (stat(filename, &statbuf) == 0) result = 1;

	return result;
}

// Check if a directory path exists
int DirectoryExists(const char* dirPath)
{
	int result = 0;
	DIR* dir = opendir(dirPath);

	if (dir != NULL)
	{
		result = 1;
		closedir(dir);
	}

	return result;
}

