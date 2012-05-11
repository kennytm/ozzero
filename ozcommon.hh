/*
    Copyright (c) 2012, Kenny Chan <kennytm@gmail.com>
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

// ozcommon.hh -- Defines some common functions and types used for Oz of wrapper
//                types in Ozzero.

#ifndef OZCOMMON_HH
#define OZCOMMON_HH 1

#define __STDC_FORMAT_MACROS

#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <climits>
#include <inttypes.h>
#include <mozart.h>

namespace Ozzero
{

    /** Raise an Oz error. Use it like this inside an OZ_BI_define:

        if (some bad condition)
            return raise_error();
        else
            OZ_RETURN(etc);
    */
    static inline OZ_Return raise_error()
    {
        int error_number = errno;
        const char* error_message = strerror(error_number);
        return OZ_raiseErrorC("zmqError", 2,
                              OZ_int(error_number),
                              OZ_atom(error_message));
    }

    /** Raise an Oz error, or call 'continue' if the errno is EINTR. Use it like
    this inside an OZ_BI_define:

        while (some function)
            RETURN_RAISE_ERROR_OR_RETRY;
        OZ_RETURN(etc);
    */
    #define RETURN_RAISE_ERROR_OR_RETRY \
        if (errno != EINTR) \
            return raise_error()


    static inline OZ_Return checked(int v)
    {
        return v ? raise_error() : OZ_ENTAILED;
    }

    template <typename Self, typename WrappedType, const int& g_id_ref>
    class ExtensionBase : public OZ_Extension
    {
    protected:
        /** Access the wrapped object. */
        WrappedType _obj;

    public:
        virtual OZ_Extension* sCloneV() { Assert(0); return NULL; }
        virtual void gCollectRecurseV() {}
        virtual void sCloneRecurseV() {}

        WrappedType* get()
        {
            return static_cast<Self*>(this)->is_valid() ? &_obj : NULL;
        }

        virtual int getIdV() { return g_id_ref; }

        /** Check whether the given Oz term is an instance of this type. */
        static bool is(OZ_Term term)
        {
            term = OZ_deref(term);
            return OZ_isExtension(term) && OZ_getExtension(term)->getIdV() == g_id_ref;
        }

        /** Coerce the Oz term into a pointer of this type. */
        static Self* get(OZ_Term term)
        {
            term = OZ_deref(term);
            Assert(OZ_isExtension(term));
            OZ_Extension* extension = OZ_getExtension(term);
            Assert(extension->getIdV() == g_id_ref);
            return static_cast<Self*>(extension);
        }
    };

    /** This is an abstract base class of a type to be extended. Use it like
    this:

        int g_id_File;
        class File : public Ozzero::Extension<File, FILE*, g_id_File>
        {
        public:
            explicit File(FILE* f) : Ozzero::Extension(f) {}
            int getc() { return fgetc(_obj); }
        };

    The type by default is not clonable. Override sCloneV() to enable cloning.
    */
    template <typename Self, typename WrappedType, const int& g_id_ref>
    class Extension : public ExtensionBase<Self, WrappedType, g_id_ref>
    {
    protected:
        /** Construct the new extension object from a wrapped object. This
        constructor is used only in gCollectV. Override that function if it does
        not make sense for a subclass to use the same constructor. */
        explicit Extension(WrappedType obj) { this->_obj = obj; }

    public:
        virtual OZ_Extension* gCollectV() { return new Self(this->_obj); }
    };

    /** Declare an input argument as the C++ type. Use it like this:

        OZ_declare(File, 0, file);
        // Now we have 'Ozzero::File* file' pointing to the 0th argument.

    */
    #define OZ_declare(Suffix, argNum, varName) \
        OZ_declareType(argNum, varName, Suffix*, #Suffix, Suffix::is, Suffix::get)

    /** Convert an Oz-term to an int64_t */
    static inline int64_t OZ_intToCint64(OZ_Term term)
    {
        int64_t value = OZ_intToCL(term);
        if (LONG_MAX < 0x7fffffffffffffffLL || LONG_MIN > -0x8000000000000000LL)
        {
            if (value == LONG_MAX || value == LONG_MIN)
            {
                char* string = OZ_toC(term, 0, 0);
                sscanf(string, "%" SCNd64, &value);
            }
        }
        return value;
    }

    /** Convert an Oz-term to a uint64_t */
    static inline uint64_t OZ_intToCuint64(OZ_Term term)
    {
        uint64_t value = OZ_intToCulong(term);
        if (ULONG_MAX < 0xffffffffffffffffULL)
        {
            if (value == ULONG_MAX)
            {
                char* string = OZ_toC(term, 0, 0);
                sscanf(string, "%" SCNu64, &value);
            }
        }
        return value;
    }

    /** Convert an int64_t to an Oz-term */
    static inline OZ_Term OZ_int64(int64_t value)
    {
        if (LONG_MIN <= value && value <= LONG_MAX)
            return OZ_long(static_cast<long>(value));

        char buffer[24];
        snprintf(buffer, sizeof(buffer), "%" PRId64, value);
        if (buffer[0] == '-')
            buffer[0] = '~';
        return OZ_CStringToInt(buffer);
    }

    /** Convert a uint64_t to an Oz-term */
    static inline OZ_Term OZ_uint64(uint64_t value)
    {
        if (value <= ULONG_MAX)
            return OZ_unsignedLong(static_cast<unsigned long>(value));

        char buffer[24];
        snprintf(buffer, sizeof(buffer), "%" PRIu64, value);
        return OZ_CStringToInt(buffer);
    }

    /** Ensure 'inst' is valid (not closed). */
    #define ENSURE_VALID(Type, inst) \
        if (!inst->is_valid()) \
            return OZ_raiseErrorC("zmqError", 2, OZ_int(-1), OZ_atom(#Type " is already closed."))
}

#endif
