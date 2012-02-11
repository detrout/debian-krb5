/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/asn.1/asn1_encode.h */
/*
 * Copyright 1994, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __ASN1_ENCODE_H__
#define __ASN1_ENCODE_H__

#include "k5-int.h"
#include "krbasn1.h"
#include "asn1buf.h"
#include "asn1_get.h"
#include <time.h>

/* These functions are referenced by encoder structures.  They handle the
 * encoding of primitive ASN.1 types. */
asn1_error_code k5_asn1_encode_bool(asn1buf *buf, asn1_intmax val,
                                    size_t *len_out);
asn1_error_code k5_asn1_encode_int(asn1buf *buf, asn1_intmax val,
                                   size_t *len_out);
asn1_error_code k5_asn1_encode_uint(asn1buf *buf, asn1_uintmax val,
                                    size_t *len_out);
asn1_error_code k5_asn1_encode_bytestring(asn1buf *buf,
                                          unsigned char *const *val,
                                          size_t len, size_t *len_out);
asn1_error_code k5_asn1_encode_bitstring(asn1buf *buf,
                                         unsigned char *const *val,
                                         size_t len, size_t *len_out);
asn1_error_code k5_asn1_encode_generaltime(asn1buf *buf, time_t val,
                                           size_t *len_out);

/*
 * An atype_info structure specifies how to encode a pointer to a C
 * object as an ASN.1 type.
 *
 * We wind up with a lot of load-time relocations being done, which is
 * a bit annoying.  Be careful about "fixing" that at the cost of too
 * much run-time performance.  It might work to have a master "module"
 * descriptor with pointers to various arrays (type descriptors,
 * strings, field descriptors, functions) most of which don't need
 * relocation themselves, and replace most of the pointers with table
 * indices.
 *
 * It's a work in progress.
 */

enum atype_type {
    /* For bounds checking only.  By starting with 2, we guarantee that
     * zero-initialized storage will be recognized as invalid. */
    atype_min = 1,
    /* Encoder function to be called with address of <thing>.  tinfo is a
     * struct fn_info *. */
    atype_fn,
    /* Pointer to actual thing to be encoded.  tinfo is a struct ptr_info *. */
    atype_ptr,
    /* Actual thing to be encoded is at an offset from the original pointer.
     * tinfo is a struct offset_info *. */
    atype_offset,
    /*
     * Indicates a sequence field which may or may not be present in an object.
     * tinfo is a struct optional_info *.  Must be used within a sequence,
     * although the optional type may be nested within offset, ptr, and/or tag
     * types.
     */
    atype_optional,
    /*
     * Actual thing to be encoded is an object at an offset from the original
     * pointer, combined with an integer at a different offset, in a manner
     * specified by a cntype_info structure.  tinfo is a struct counted_info *.
     */
    atype_counted,
    /* Sequence.  tinfo is a struct seq_info *. */
    atype_sequence,
    /*
     * Sequence-of, with pointer to base type descriptor, represented as a
     * null-terminated array of pointers (and thus the "base" type descriptor
     * is actually an atype_ptr node).  tinfo is a struct atype_info * giving
     * the base type.
     */
    atype_nullterm_sequence_of,
    atype_nonempty_nullterm_sequence_of,
    /* Tagged version of another type.  tinfo is a struct tagged_info *. */
    atype_tagged_thing,
    /* Signed or unsigned integer.  tinfo is NULL (the atype_info size field is
     * used to determine the width). */
    atype_int,
    atype_uint,
    /* Integer value taken from the type info, not from the object being
     * encoded.  tinfo is an int *. */
    atype_int_immediate,
    /* Unused except for bounds checking.  */
    atype_max
};

struct atype_info {
    enum atype_type type;
    size_t size;                /* Used for sequence-of processing */
    const void *tinfo;          /* Points to type-specific structure */
};

struct fn_info {
    asn1_error_code (*enc)(asn1buf *, const void *, taginfo *);
};

struct ptr_info {
    const void *(*loadptr)(const void *);
    const struct atype_info *basetype;
};

struct offset_info {
    unsigned int dataoff : 9;
    const struct atype_info *basetype;
};

struct optional_info {
    int (*is_present)(const void *);
    const struct atype_info *basetype;
};

struct counted_info {
    unsigned int dataoff : 9;
    unsigned int lenoff : 9;
    unsigned int lensigned : 1;
    unsigned int lensize : 5;
    const struct cntype_info *basetype;
};

struct tagged_info {
    unsigned int tagval : 16, tagtype : 8, construction : 6, implicit : 1;
    const struct atype_info *basetype;
};

/* A cntype_info structure specifies how to encode a pointer to a C object and
 * count (length or union distinguisher) as an ASN.1 object. */

enum cntype_type {
    cntype_min = 1,

    /*
     * Apply an encoder function (contents only) and wrap it in a universal
     * primitive tag.  The object must be a char * or unsigned char *.  tinfo
     * is a struct string_info *.
     */
    cntype_string,

    /* Insert a pre-made DER encoding contained at the pointer and length.  The
     * object must be a char * or unsigned char *.  tinfo is NULL. */
    cntype_der,

    /* Encode a counted sequence of a given base type.  tinfo is a struct
     * atype_info * giving the base type, which must be of type atype_ptr. */
    cntype_seqof,

    /* Encode one of several alternatives from a union object, using the count
     * as a distinguisher.  tinfo is a struct choice_info *. */
    cntype_choice,

    cntype_max
};

struct cntype_info {
    enum cntype_type type;
    const void *tinfo;
};

struct string_info {
    asn1_error_code (*enc)(asn1buf *, unsigned char *const *, size_t,
                           size_t *);
    unsigned int tagval : 5;
};

struct choice_info {
    const struct atype_info **options;
    size_t n_options;
};

struct seq_info {
    const struct atype_info **fields;
    size_t n_fields;
    /* Currently all sequences are assumed to be extensible. */
};

/*
 * The various DEF*TYPE macros must:
 *
 * + Define a type named aux_type_##DESCNAME, for use in any types derived from
 *   the type being defined.
 *
 * + Define an atype_info struct named k5_atype_##DESCNAME
 *
 * + Define a type-specific structure, referenced by the tinfo field
 *   of the atype_info structure.
 *
 * + Define any extra stuff needed in the type descriptor, like
 *   pointer-load functions.
 *
 * + Accept a following semicolon syntactically, to keep Emacs parsing
 *   (and indentation calculating) code happy.
 *
 * Nothing else should directly define the atype_info structures.
 */

/* Define a type using an encoder function. */
#define DEFFNTYPE(DESCNAME, CTYPENAME, ENCFN)                   \
    typedef CTYPENAME aux_type_##DESCNAME;                      \
    static const struct fn_info aux_info_##DESCNAME = {         \
        ENCFN                                                   \
    };                                                          \
    const struct atype_info k5_atype_##DESCNAME = {             \
        atype_fn, sizeof(CTYPENAME), &aux_info_##DESCNAME       \
    }
/* A sequence, defined by the indicated series of types, and an optional
 * function indicating which fields are not present. */
#define DEFSEQTYPE(DESCNAME, CTYPENAME, FIELDS)                         \
    typedef CTYPENAME aux_type_##DESCNAME;                              \
    static const struct seq_info aux_seqinfo_##DESCNAME = {             \
        FIELDS, sizeof(FIELDS)/sizeof(FIELDS[0])                        \
    };                                                                  \
    const struct atype_info k5_atype_##DESCNAME = {                     \
        atype_sequence, sizeof(CTYPENAME), &aux_seqinfo_##DESCNAME      \
    }
/* Integer types.  */
#define DEFINTTYPE(DESCNAME, CTYPENAME)                         \
    typedef CTYPENAME aux_type_##DESCNAME;                      \
    const struct atype_info k5_atype_##DESCNAME = {             \
        atype_int, sizeof(CTYPENAME), NULL                      \
    }
#define DEFUINTTYPE(DESCNAME, CTYPENAME)                        \
    typedef CTYPENAME aux_type_##DESCNAME;                      \
    const struct atype_info k5_atype_##DESCNAME = {             \
        atype_uint, sizeof(CTYPENAME), NULL                     \
    }
#define DEFINT_IMMEDIATE(DESCNAME, VAL)                 \
    typedef int aux_type_##DESCNAME;                    \
    static const int aux_int_##DESCNAME = VAL;          \
    const struct atype_info k5_atype_##DESCNAME = {     \
        atype_int_immediate, 0, &aux_int_##DESCNAME     \
    }

/* Pointers to other types, to be encoded as those other types.  */
#ifdef POINTERS_ARE_ALL_THE_SAME
#define DEFPTRTYPE(DESCNAME,BASEDESCNAME)                       \
    typedef aux_type_##BASEDESCNAME *aux_type_##DESCNAME;       \
    static const struct ptr_info aux_info_##DESCNAME = {        \
        NULL, &k5_atype_##BASEDESCNAME                          \
    };                                                          \
    const struct atype_info k5_atype_##DESCNAME = {             \
        atype_ptr, sizeof(aux_type_##DESCNAME),                 \
        &aux_info_##DESCNAME                                    \
    }
#else
#define DEFPTRTYPE(DESCNAME,BASEDESCNAME)                       \
    typedef aux_type_##BASEDESCNAME *aux_type_##DESCNAME;       \
    static const void *                                         \
    aux_loadptr_##DESCNAME(const void *p)                       \
    {                                                           \
        return *(aux_type_##DESCNAME *)p;                       \
    }                                                           \
    static const struct ptr_info aux_info_##DESCNAME = {        \
        aux_loadptr_##DESCNAME, &k5_atype_##BASEDESCNAME        \
    };                                                          \
    const struct atype_info k5_atype_##DESCNAME = {             \
        atype_ptr, sizeof(aux_type_##DESCNAME),                 \
        &aux_info_##DESCNAME                                    \
    }
#endif
#define DEFOFFSETTYPE(DESCNAME, STYPE, FIELDNAME, BASEDESC)     \
    typedef STYPE aux_type_##DESCNAME;                          \
    static const struct offset_info aux_info_##DESCNAME = {     \
        OFFOF(STYPE, FIELDNAME, aux_type_##BASEDESC),           \
        &k5_atype_##BASEDESC                                    \
    };                                                          \
    const struct atype_info k5_atype_##DESCNAME = {             \
        atype_offset, sizeof(aux_type_##DESCNAME),              \
        &aux_info_##DESCNAME                                    \
    }
#define DEFCOUNTEDTYPE_base(DESCNAME, STYPE, DATAFIELD, COUNTFIELD, SIGNED, \
                            CDESC)                                      \
    typedef STYPE aux_type_##DESCNAME;                                  \
    const struct counted_info aux_info_##DESCNAME = {                   \
        OFFOF(STYPE, DATAFIELD, aux_ptrtype_##CDESC),                   \
        OFFOF(STYPE, COUNTFIELD, aux_counttype_##CDESC),                \
        SIGNED, sizeof(((STYPE*)0)->COUNTFIELD),                        \
        &k5_cntype_##CDESC                                              \
    };                                                                  \
    const struct atype_info k5_atype_##DESCNAME = {                     \
        atype_counted, sizeof(STYPE),                                   \
        &aux_info_##DESCNAME                                            \
    }
#define DEFCOUNTEDTYPE(DESCNAME, STYPE, DATAFIELD, COUNTFIELD, CDESC) \
    DEFCOUNTEDTYPE_base(DESCNAME, STYPE, DATAFIELD, COUNTFIELD, 0, CDESC)
#define DEFCOUNTEDTYPE_SIGNED(DESCNAME, STYPE, DATAFIELD, COUNTFIELD, CDESC) \
    DEFCOUNTEDTYPE_base(DESCNAME, STYPE, DATAFIELD, COUNTFIELD, 1, CDESC)

/* Optional sequence fields.  The basic form allows arbitrary test and
 * initializer functions to be used. */
#define DEFOPTIONALTYPE(DESCNAME, PRESENT, BASEDESC)            \
    typedef aux_type_##BASEDESC aux_type_##DESCNAME;            \
    static const struct optional_info aux_info_##DESCNAME = {   \
        PRESENT, &k5_atype_##BASEDESC                           \
    };                                                          \
    const struct atype_info k5_atype_##DESCNAME = {             \
        atype_optional, sizeof(aux_type_##DESCNAME),            \
        &aux_info_##DESCNAME                                    \
    }
/* This form defines an is_present function for a zero-valued integer or null
 * pointer of the base type's C type. */
#define DEFOPTIONALZEROTYPE(DESCNAME, BASEDESC)                 \
    static int                                                  \
    aux_present_##DESCNAME(const void *p)                       \
    {                                                           \
        return *(aux_type_##BASEDESC *)p != 0;                  \
    }                                                           \
    DEFOPTIONALTYPE(DESCNAME, aux_present_##DESCNAME, BASEDESC)
/* This form defines an is_present function for a null or empty null-terminated
 * array of the base type's C type. */
#define DEFOPTIONALEMPTYTYPE(DESCNAME, BASEDESC)                \
    static int                                                  \
    aux_present_##DESCNAME(const void *p)                       \
    {                                                           \
        const aux_type_##BASEDESC *val = p;                     \
        return (*val != NULL && **val != NULL);                 \
    }                                                           \
    DEFOPTIONALTYPE(DESCNAME, aux_present_##DESCNAME, BASEDESC)

/*
 * This encodes a pointer-to-pointer-to-thing where the passed-in
 * value points to a null-terminated list of pointers to objects to be
 * encoded, and encodes a (possibly empty) SEQUENCE OF these objects.
 *
 * BASEDESCNAME is a descriptor name for the pointer-to-thing
 * type.
 *
 * When dealing with a structure containing a
 * pointer-to-pointer-to-thing field, make a DEFPTRTYPE of this type,
 * and use that type for the structure field.
 */
#define DEFNULLTERMSEQOFTYPE(DESCNAME,BASEDESCNAME)                     \
    typedef aux_type_##BASEDESCNAME aux_type_##DESCNAME;                \
    const struct atype_info k5_atype_##DESCNAME = {                     \
        atype_nullterm_sequence_of, sizeof(aux_type_##DESCNAME),        \
        &k5_atype_##BASEDESCNAME                                        \
    }
#define DEFNONEMPTYNULLTERMSEQOFTYPE(DESCNAME,BASEDESCNAME)     \
    typedef aux_type_##BASEDESCNAME aux_type_##DESCNAME;        \
    const struct atype_info k5_atype_##DESCNAME = {             \
        atype_nonempty_nullterm_sequence_of,                    \
        sizeof(aux_type_##DESCNAME),                            \
        &k5_atype_##BASEDESCNAME                                \
    }

/* Objects with an explicit or implicit tag.  (Implicit tags will ignore the
 * construction field.) */
#define DEFTAGGEDTYPE(DESCNAME, CLASS, CONSTRUCTION, TAG, IMPLICIT, BASEDESC) \
    typedef aux_type_##BASEDESC aux_type_##DESCNAME;                    \
    static const struct tagged_info aux_info_##DESCNAME = {             \
        TAG, CLASS, CONSTRUCTION, IMPLICIT, &k5_atype_##BASEDESC        \
    };                                                                  \
    const struct atype_info k5_atype_##DESCNAME = {                     \
        atype_tagged_thing, sizeof(aux_type_##DESCNAME),                \
        &aux_info_##DESCNAME                                            \
    }
/* Objects with an explicit APPLICATION tag added.  */
#define DEFAPPTAGGEDTYPE(DESCNAME, TAG, BASEDESC)                       \
    DEFTAGGEDTYPE(DESCNAME, APPLICATION, CONSTRUCTED, TAG, 0, BASEDESC)
/* Object with a context-specific tag added */
#define DEFCTAGGEDTYPE(DESCNAME, TAG, BASEDESC)                         \
    DEFTAGGEDTYPE(DESCNAME, CONTEXT_SPECIFIC, CONSTRUCTED, TAG, 0, BASEDESC)
#define DEFCTAGGEDTYPE_IMPLICIT(DESCNAME, TAG, BASEDESC)                \
    DEFTAGGEDTYPE(DESCNAME, CONTEXT_SPECIFIC, CONSTRUCTED, TAG, 1, BASEDESC)

/* Define an offset type with an explicit context tag wrapper (the usual case
 * for an RFC 4120 sequence field). */
#define DEFFIELD(NAME, STYPE, FIELDNAME, TAG, DESC)                     \
    DEFOFFSETTYPE(NAME##_untagged, STYPE, FIELDNAME, DESC);             \
    DEFCTAGGEDTYPE(NAME, TAG, NAME##_untagged)
/* Define a counted type with an explicit context tag wrapper. */
#define DEFCNFIELD(NAME, STYPE, DATAFIELD, LENFIELD, TAG, CDESC)        \
    DEFCOUNTEDTYPE(NAME##_untagged, STYPE, DATAFIELD, LENFIELD, CDESC); \
    DEFCTAGGEDTYPE(NAME, TAG, NAME##_untagged)
/* Like DEFFIELD but with an implicit context tag. */
#define DEFFIELD_IMPLICIT(NAME, STYPE, FIELDNAME, TAG, DESC)            \
    DEFOFFSETTYPE(NAME##_untagged, STYPE, FIELDNAME, DESC);             \
    DEFCTAGGEDTYPE_IMPLICIT(NAME, TAG, NAME##_untagged)

/*
 * DEFCOUNTED*TYPE macros must:
 *
 * + Define types named aux_ptrtype_##DESCNAME and aux_counttype_##DESCNAME, to
 *   allow type checking when the counted type is referenced with structure
 *   field offsets in DEFCOUNTEDTYPE.
 *
 * + Define a cntype_info struct named k5_cntype_##DESCNAME
 *
 * + Define a type-specific structure, referenced by the tinfo field of the
 *   cntype_info structure.
 *
 * + Accept a following semicolon syntactically.
 */

#define DEFCOUNTEDSTRINGTYPE(DESCNAME, DTYPE, LTYPE, ENCFN, TAGVAL)     \
    typedef DTYPE aux_ptrtype_##DESCNAME;                               \
    typedef LTYPE aux_counttype_##DESCNAME;                             \
    static const struct string_info aux_info_##DESCNAME = {             \
        ENCFN, TAGVAL                                                   \
    };                                                                  \
    const struct cntype_info k5_cntype_##DESCNAME = {                   \
        cntype_string, &aux_info_##DESCNAME                             \
    }

#define DEFCOUNTEDDERTYPE(DESCNAME, DTYPE, LTYPE)               \
    typedef DTYPE aux_ptrtype_##DESCNAME;                       \
    typedef LTYPE aux_counttype_##DESCNAME;                     \
    const struct cntype_info k5_cntype_##DESCNAME = {           \
        cntype_der, NULL                                        \
    }

#define DEFCOUNTEDSEQOFTYPE(DESCNAME, LTYPE, BASEDESC)          \
    typedef aux_type_##BASEDESC aux_ptrtype_##DESCNAME;         \
    typedef LTYPE aux_counttype_##DESCNAME;                     \
    const struct cntype_info k5_cntype_##DESCNAME = {           \
        cntype_seqof, &k5_atype_##BASEDESC                      \
    }

#define DEFCHOICETYPE(DESCNAME, UTYPE, DTYPE, FIELDS)           \
    typedef UTYPE aux_ptrtype_##DESCNAME;                       \
    typedef DTYPE aux_counttype_##DESCNAME;                     \
    static const struct choice_info aux_info_##DESCNAME = {     \
        FIELDS, sizeof(FIELDS) / sizeof(FIELDS[0])              \
    };                                                          \
    const struct cntype_info k5_cntype_##DESCNAME = {           \
        cntype_choice, &aux_info_##DESCNAME                     \
    }

/*
 * Declare an externally-defined type.  This is a hack we should do
 * away with once we move to generating code from a script.  For now,
 * this macro is unfortunately not compatible with the defining macros
 * above, since you can't do the typedefs twice and we need the
 * declarations to produce typedefs.  (We could eliminate the typedefs
 * from the DEF* macros, but then every DEF* macro use, even the ones
 * for internal type nodes we only use to build other types, would
 * need an accompanying declaration which explicitly lists the
 * type.)
 */
#define IMPORT_TYPE(DESCNAME, CTYPENAME)                        \
    typedef CTYPENAME aux_type_##DESCNAME;                      \
    extern const struct atype_info k5_atype_##DESCNAME

/* Partially encode the contents of a type and return its tag information.
 * Used only by kdc_req_body. */
asn1_error_code
k5_asn1_encode_atype(asn1buf *buf, const void *val, const struct atype_info *a,
                     taginfo *tag_out);

/* Returns a completed encoding, with tag and in the correct byte order, in an
 * allocated krb5_data. */
extern krb5_error_code
k5_asn1_full_encode(const void *rep, const struct atype_info *a,
                    krb5_data **code_out);

#define MAKE_ENCODER(FNAME, DESC)                                       \
    krb5_error_code                                                     \
    FNAME(const aux_type_##DESC *rep, krb5_data **code_out)             \
    {                                                                   \
        return k5_asn1_full_encode(rep, &k5_atype_##DESC, code_out);    \
    }                                                                   \
    extern int dummy /* gobble semicolon */

#include <stddef.h>
/*
 * Ugly hack!
 * Like "offsetof", but with type checking.
 */
#define WARN_IF_TYPE_MISMATCH(LVALUE, TYPE)     \
    (sizeof(0 ? (TYPE *) 0 : &(LVALUE)))
#define OFFOF(TYPE,FIELD,FTYPE)                                 \
    (offsetof(TYPE, FIELD)                                      \
     + 0 * WARN_IF_TYPE_MISMATCH(((TYPE*)0)->FIELD, FTYPE))

#endif
