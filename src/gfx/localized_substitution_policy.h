#ifndef K33_LOCALIZED_SUBSTITUTION_POLICY_H
#define K33_LOCALIZED_SUBSTITUTION_POLICY_H
#include <stddef.h>
#include <string.h>

#define K33LOC_CAPACITY 1024u
#define K33LOC_MAX_ARGS 9u
#define K33LOC_INPUT_LIMIT 65536u
typedef struct { const char *data; size_t length; } K33LocView;
typedef enum { K33LOC_INVALID=0,K33LOC_OK=1,K33LOC_TRUNCATED=2 } K33LocResult;

/* Views are established by the native memory reader, never by unbounded
 * strlen. Count includes values only, not the count word in native Args.
 * Native ordinary syntax is &&1..&&9; &&10 means substitution1 then literal0.
 * Unknown nonnumeric markers are literal. Invalid references clear output.
 */
static K33LocResult K33LocFormat(K33LocView format,const K33LocView *values,int count,
                                char *output,size_t capacity)
{
    if (!output || !capacity) return K33LOC_INVALID;
    memset(output,0,capacity);
    if (!format.data || format.length>K33LOC_INPUT_LIMIT || count<0 || count>(int)K33LOC_MAX_ARGS ||
        (count && !values)) return K33LOC_INVALID;
    /* Validate all references before writing, including ones past the visible
     * truncation point. A missing/invalid argument never leaves partial text. */
    for (size_t i=0;i<format.length;i++) {
        if (format.data[i]=='&' && i+2<format.length && format.data[i+1]=='&' &&
            format.data[i+2]>='0' && format.data[i+2]<='9') {
            unsigned index=(unsigned)(format.data[i+2]-'1');
            if (index>=(unsigned)count || !values[index].data || values[index].length>K33LOC_INPUT_LIMIT)
                return K33LOC_INVALID;
            i+=2;
        }
    }
    size_t used=0;int truncated=0;
    for (size_t i=0;i<format.length;i++) {
        const char *source=format.data+i;size_t length=1;
        if (format.data[i]=='&' && i+2<format.length && format.data[i+1]=='&' &&
            format.data[i+2]>='0' && format.data[i+2]<='9') {
            unsigned index=(unsigned)(format.data[i+2]-'1');
            source=values[index].data;length=values[index].length;i+=2;
        }
        size_t available=capacity-1-used,copy=length<available ? length:available;
        if (copy) memcpy(output+used,source,copy);
        used+=copy;
        if (copy!=length) truncated=1;
    }
    output[used]=0;
    return truncated ? K33LOC_TRUNCATED:K33LOC_OK;
}
#endif
