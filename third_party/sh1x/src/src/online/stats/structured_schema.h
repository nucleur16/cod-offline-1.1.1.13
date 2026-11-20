#ifndef STRUCTURED_SCHEMA_H
#define STRUCTURED_SCHEMA_H
#include "assets.h"
#include "structured_data.h"
#pragma pack(push, 8)
typedef struct {uint32_t kind,index;} structured_type_t;
typedef struct {const char *name; uint32_t value;} structured_enum_member_t;
typedef struct {uint32_t count; structured_enum_member_t *members;} structured_enum_t;
typedef struct {
    const char *name; structured_type_t type;
    uint32_t logical_index,extra[2];
} structured_property_t;
typedef struct {
    uint32_t count,size,bit_offset,logical_count;
    structured_property_t *properties;
} structured_struct_t;
typedef struct {uint32_t count_or_enum; structured_type_t element; uint32_t stride;} structured_array_t;
typedef struct {
    uint32_t version,checksum,enum_count,struct_count,indexed_count,enum_array_count;
    structured_type_t root;
    structured_enum_t *enums;
    structured_struct_t *structs;
    structured_array_t *indexed_arrays,*enum_arrays;
    structured_layout_t layout;
} structured_definition_t;
typedef struct {const char *name; uint32_t count; structured_definition_t *definitions;} structured_schema_t;
/* A non-NULL name selects a field/enum; otherwise index selects an array item. */
typedef struct {const char *name; int32_t index;} structured_path_t;
typedef struct {
    structured_type_t type;
    uint32_t logical_index,mapped_address;
} structured_field_t;
/* Resolve a complete scalar path with C1's lookup arithmetic. Validates every
 * visited type and final storage extent; preserves out on failure. Enum-array
 * indices follow 0x61f270 directly. Nested scoreboard semantics remain under
 * investigation; no inferred multiplication by serialized byte stride. */
int ResolveStructuredField(const structured_definition_t *,
    const structured_path_t *,size_t count,structured_field_t *out);
/* C1's structured-data result codes, numbered as its message table at
 * 0xccfcd8. Anything this resolver rejects that C1 does not classify (an
 * invalid definition, or a path ending on a struct/array rather than a value)
 * reports the type mismatch code. The change/set and value-range codes belong
 * to the write path and are never produced by resolution. */
enum {
    STRUCTURED_OK = 0,
    STRUCTURED_TYPE = 1,
    STRUCTURED_BOUNDS = 2,
    STRUCTURED_NAME = 3,
    STRUCTURED_ENUM = 4,
    STRUCTURED_CHANGED = 5,
    STRUCTURED_UNCHANGED = 6,
    STRUCTURED_BYTE_RANGE = 8,
    STRUCTURED_STRING_LENGTH = 9
};
const char *StructuredStatusText(int status);
/* Same resolution, plus the code above for the first failing path element.
 * status may be NULL; it is written on success and failure alike. */
int ResolveStructuredFieldStatus(const structured_definition_t *,
    const structured_path_t *,size_t count,structured_field_t *out,int *status);
/* String (3) and named enum (4) storage. Read borrows record/schema text.
 * Write rejects unknown enum names and strings without room for a terminator;
 * a shorter string preserves bytes after its terminator, as C1 does. */
int StructuredReadText(const structured_definition_t *,
    const structured_path_t *,size_t,const uint8_t *,size_t,const char **out);
int StructuredWriteText(const structured_definition_t *,
    const structured_path_t *,size_t,uint8_t *,size_t,const char *value);
/* Owns record arrays; strings and offset maps borrow asset/resolver lifetimes.
 * Requires inline record arrays; packed string references use resolver.
 * Exact stream boundary required. This validates stream structure, not every
 * type graph or field storage range. out must be zero/prior; failure atomic. */
int ReadStructuredSchema(const asset_t *,const char *(*resolver)(void *,uint32_t),
    void *context,structured_schema_t *out);
void FreeStructuredSchema(structured_schema_t *);
int LoadMountedStructuredSchema(const char *,structured_schema_t *);
#pragma pack(pop)
#endif
