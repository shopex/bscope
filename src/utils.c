#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <execinfo.h>


void die(const char *fmt, ...);
extern void die_on_error(int x, char const *context);

void die(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}

void die_on_error(int x, char const *context)
{
  if (x < 0) {
    fprintf(stderr, "%s: %s\n", context, amqp_error_string2(x));
    exit(1);
  }
}

void print_trace (void) {
	void *array[10];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace (array, 10);
	strings = backtrace_symbols (array, size);

	printf ("Obtained %zd stack frames.\n", size);

	for (i = 0; i < size; i++)
		printf ("%s\n", strings[i]);

	free (strings);
}



int is_in_list(char *test, int strlen, char *list[], int size){
	int i = 0;
	for(;i<size;i++){
		if(list[i][0]=='*' && list[i][1]==0){
			return 1;
		}else if(strncasecmp(test, list[i], strlen)==0){
			return 1;
		}
	}
	return 0;
}

char * urldecode(char * p){
	if(p){
		int n=0, i=0, j=0, l = strlen(p);
		char buf[] = "0x00";
		for(i=0; i<l; i++){
			if(*(p+i)=='%'){
				if( i < ( l - 1 ) && *(p+i+1)=='%'){
					p[j] = '%';
					i+=1;
				}else if( i < ( l - 2 ) ){
					buf[2] = *(p+i+1);
					buf[3] = *(p+i+2);
					int a = strtoul(buf, 0 , 16);
					p[j] = a;
					i+=2;
				}
			}else if(*(p+i)=='+'){
				p[j] = ' ';
			}else{
				p[j] = p[i];
			}
			j++;
		}
		p[j] = 0;
		return p;
	}
}

char * memstr(char *haystack, char *needle, int size){
	char *p;
	char needlesize = strlen(needle);

	for (p = haystack; p <= (haystack-needlesize+size); p++)
	{
		if (memcmp(p, needle, needlesize) == 0)
			return p; /* found */
	}
	return NULL;
}

void print_data_std(const char *data, unsigned int len){
	int i=0, j=0, k=0;
	for(i=0, j=0; i<len; i++){
		if(isprint(data[i])){
			printf("%c", data[i]);
			j++;
		}else{
			k = printf("[%x]", data[i]);
			j+=k;
		}

		if( (j>77 && j!=0) || i==len-1){
			j=0;
			printf("\n");
		}
	}
	printf("\n");
}

char * strtolower(char *string){
	char *cp = string;
	while(*cp){
		*cp = tolower(*cp);
		cp++;
	}
	return(string);
}

//vim: ts=4
