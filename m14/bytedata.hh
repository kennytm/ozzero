/*
 *  Authors:
 *    Denys Duchier (duchier@ps.uni-sb.de)
 *
 *  Copyright:
 *    Denys Duchier (1998)
 *
 *  Last change:
 *    $Date$ by $Author$
 *    $Revision$
 *
 *  This file is part of Mozart, an implementation
 *  of Oz 3:
 *     http://www.mozart-oz.org
 *
 *  See the file "LICENSE" or
 *     http://www.mozart-oz.org/LICENSE.html
 *  for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL
 *  WARRANTIES.
 *
 */

#ifndef __BYTEDATAH
#define __BYTEDATAH

typedef unsigned char BYTE;

class BytePtr
{
protected:
    BYTE *data;
public:
    virtual int getSize();
    virtual void sCloneRecurseV();
    virtual void gCollectRecurseV();
    BYTE* getData() { return data; }
};

class ByteData: public BytePtr {
protected:
    int width;
public:
    virtual int getSize();
};

class ByteString : public OZ_Extension, public ByteData
{
public:
};

inline ByteString* tagged2ByteString(OZ_Term term)
{
    term = OZ_deref(term);
    Assert(OZ_isExtension(term));
    OZ_Extension* extension = OZ_getExtension(term);
    Assert(extension->getIdV() == OZ_E_BYTESTRING);
    return static_cast<ByteString*>(extension);
}

#endif
