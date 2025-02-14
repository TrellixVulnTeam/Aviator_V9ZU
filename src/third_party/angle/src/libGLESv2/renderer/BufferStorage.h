//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferStorage.h Defines the abstract BufferStorage class.

#ifndef LIBGLESV2_RENDERER_BUFFERSTORAGE_H_
#define LIBGLESV2_RENDERER_BUFFERSTORAGE_H_

#include "common/angleutils.h"

namespace rx
{

class BufferStorage
{
  public:
    BufferStorage();
    virtual ~BufferStorage();

    // The data returned is only guaranteed valid until next non-const method.
    virtual void *getData() = 0;
    virtual void setData(const void* data, size_t size, size_t offset) = 0;
    virtual void copyData(BufferStorage* sourceStorage, size_t size, size_t sourceOffset, size_t destOffset) = 0;
    virtual void clear() = 0;
    virtual void markTransformFeedbackUsage() = 0;
    virtual size_t getSize() const = 0;
    virtual bool supportsDirectBinding() const = 0;
    unsigned int getSerial() const;

    virtual bool isMapped() const = 0;
    virtual void *map(GLbitfield access) = 0;
    virtual void unmap() = 0;

  protected:
    void updateSerial();

  private:
    DISALLOW_COPY_AND_ASSIGN(BufferStorage);

    unsigned int mSerial;
    static unsigned int mNextSerial;
};

}

#endif // LIBGLESV2_RENDERER_BUFFERSTORAGE_H_
