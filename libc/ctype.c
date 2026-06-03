#include <ctype.h>

int isprint(int c) { return c >= 32 && c < 127; }
int isspace(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

