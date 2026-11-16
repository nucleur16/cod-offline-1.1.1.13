#pragma once
#include <wchar.h>
#include <string.h>

static int LocalMultiArgument(int argc, const wchar_t *const *argv)
{
    int enabled=0;
    for(int i=1;i+1<argc;i++) {
        if(wcscmp(argv[i],L"+set") || wcscmp(argv[i+1],L"k33_local_test_multi")) continue;
        enabled=i+2<argc && wcscmp(argv[i+2],L"1")==0;
    }
    return enabled;
}

static int LocalPort(const char *value)
{
    unsigned port=0;
    if(!*value)return 0;
    for(int i=0;value[i];i++) {
        if(i>=5 || value[i]<'0' || value[i]>'9')return 0;
        port=port*10+(unsigned)(value[i]-'0');
    }
    return port>0 && port<=65535;
}

static int LocalTitle(const char *value)
{
    /* C1/launcher encode title 1002 as hex "3ea", not decimal. */
    unsigned nonzero=0;
    if(!*value)return 0;
    for(unsigned i=0;value[i];i++) {
        unsigned char ch=(unsigned char)value[i];
        if(i>=8 || !((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F')))return 0;
        nonzero|=ch!='0';
    }
    return nonzero!=0;
}

/* dwauth.ini's exact five-line C1 contract. Both hosts must equal the same
 * explicit local-test literal (normal .1 or isolated .3) and valid port.
 * No hostname, wildcard, suffix match or general loopback range is accepted. */
static int LocalMultiRoute(const char *contents, size_t size)
{
    char lines[5][64];unsigned line=0,column=0;
    if(!contents || !size || size>256)return 0;
    memset(lines,0,sizeof(lines));
    for(size_t i=0;i<size;i++) {
        unsigned char ch=(unsigned char)contents[i];
        if(ch=='\r') { if(i+1>=size || contents[i+1]!='\n')return 0;continue; }
        if(ch=='\n') {
            if(!column || line>=5)return 0;
            line++;column=0;
            if(line==5 && i+1!=size)return 0;
        } else {
            if(line>=5 || ch<33 || ch>126 || column>=63)return 0;
            lines[line][column++]=(char)ch;
        }
    }
    if(column)line++;
    return line==5 && (strcmp(lines[0],"127.0.0.1")==0 || strcmp(lines[0],"127.0.0.3")==0) &&
        strcmp(lines[2],lines[0])==0 && LocalPort(lines[1]) &&
        strcmp(lines[1],lines[3])==0 && LocalTitle(lines[4]);
}
