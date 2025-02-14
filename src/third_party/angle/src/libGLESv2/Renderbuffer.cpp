#include "precompiled.h"
//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderbuffer.cpp: the gl::Renderbuffer class and its derived classes
// Colorbuffer, Depthbuffer and Stencilbuffer. Implements GL renderbuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4.3 page 108.

#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/renderer/RenderTarget.h"

#include "libGLESv2/Texture.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/TextureStorage.h"
#include "common/utilities.h"
#include "libGLESv2/formatutils.h"

namespace gl
{
unsigned int RenderbufferStorage::mCurrentSerial = 1;

RenderbufferStorage::RenderbufferStorage() : mSerial(issueSerials(1))
{
    mWidth = 0;
    mHeight = 0;
    mInternalFormat = GL_RGBA4;
    mActualFormat = GL_RGBA8_OES;
    mSamples = 0;
}

RenderbufferStorage::~RenderbufferStorage()
{
}

rx::RenderTarget *RenderbufferStorage::getRenderTarget()
{
    return NULL;
}

rx::RenderTarget *RenderbufferStorage::getDepthStencil()
{
    return NULL;
}

rx::TextureStorage *RenderbufferStorage::getTextureStorage()
{
    return NULL;
}

GLsizei RenderbufferStorage::getWidth() const
{
    return mWidth;
}

GLsizei RenderbufferStorage::getHeight() const
{
    return mHeight;
}

GLenum RenderbufferStorage::getInternalFormat() const
{
    return mInternalFormat;
}

GLenum RenderbufferStorage::getActualFormat() const
{
    return mActualFormat;
}

GLsizei RenderbufferStorage::getSamples() const
{
    return mSamples;
}

unsigned int RenderbufferStorage::getSerial() const
{
    return mSerial;
}

unsigned int RenderbufferStorage::issueSerials(GLuint count)
{
    unsigned int firstSerial = mCurrentSerial;
    mCurrentSerial += count;
    return firstSerial;
}

bool RenderbufferStorage::isTexture() const
{
    return false;
}

unsigned int RenderbufferStorage::getTextureSerial() const
{
    return -1;
}

Colorbuffer::Colorbuffer(rx::Renderer *renderer, rx::SwapChain *swapChain)
{
    mRenderTarget = renderer->createRenderTarget(swapChain, false); 

    if (mRenderTarget)
    {
        mWidth = mRenderTarget->getWidth();
        mHeight = mRenderTarget->getHeight();
        mInternalFormat = mRenderTarget->getInternalFormat();
        mActualFormat = mRenderTarget->getActualFormat();
        mSamples = mRenderTarget->getSamples();
    }
}

Colorbuffer::Colorbuffer(rx::Renderer *renderer, int width, int height, GLenum format, GLsizei samples) : mRenderTarget(NULL)
{
    mRenderTarget = renderer->createRenderTarget(width, height, format, samples);

    if (mRenderTarget)
    {
        mWidth = width;
        mHeight = height;
        mInternalFormat = format;
        mActualFormat = mRenderTarget->getActualFormat();
        mSamples = mRenderTarget->getSamples();
    }
}

Colorbuffer::~Colorbuffer()
{
    if (mRenderTarget)
    {
        delete mRenderTarget;
    }
}

rx::RenderTarget *Colorbuffer::getRenderTarget()
{
    return mRenderTarget;
}

DepthStencilbuffer::DepthStencilbuffer(rx::Renderer *renderer, rx::SwapChain *swapChain)
{
    mDepthStencil = renderer->createRenderTarget(swapChain, true);
    if (mDepthStencil)
    {
        mWidth = mDepthStencil->getWidth();
        mHeight = mDepthStencil->getHeight();
        mInternalFormat = mDepthStencil->getInternalFormat();
        mSamples = mDepthStencil->getSamples();
        mActualFormat = mDepthStencil->getActualFormat();
    }
}

DepthStencilbuffer::DepthStencilbuffer(rx::Renderer *renderer, int width, int height, GLsizei samples)
{

    mDepthStencil = renderer->createRenderTarget(width, height, GL_DEPTH24_STENCIL8_OES, samples);

    mWidth = mDepthStencil->getWidth();
    mHeight = mDepthStencil->getHeight();
    mInternalFormat = GL_DEPTH24_STENCIL8_OES;
    mActualFormat = mDepthStencil->getActualFormat();
    mSamples = mDepthStencil->getSamples();
}

DepthStencilbuffer::~DepthStencilbuffer()
{
    if (mDepthStencil)
    {
        delete mDepthStencil;
    }
}

rx::RenderTarget *DepthStencilbuffer::getDepthStencil()
{
    return mDepthStencil;
}

Depthbuffer::Depthbuffer(rx::Renderer *renderer, int width, int height, GLsizei samples) : DepthStencilbuffer(renderer, width, height, samples)
{
    if (mDepthStencil)
    {
        mInternalFormat = GL_DEPTH_COMPONENT16;   // If the renderbuffer parameters are queried, the calling function
                                                  // will expect one of the valid renderbuffer formats for use in 
                                                  // glRenderbufferStorage
    }
}

Depthbuffer::~Depthbuffer()
{
}

Stencilbuffer::Stencilbuffer(rx::Renderer *renderer, int width, int height, GLsizei samples) : DepthStencilbuffer(renderer, width, height, samples)
{
    if (mDepthStencil)
    {
        mInternalFormat = GL_STENCIL_INDEX8;   // If the renderbuffer parameters are queried, the calling function
                                               // will expect one of the valid renderbuffer formats for use in 
                                               // glRenderbufferStorage
    }
}

Stencilbuffer::~Stencilbuffer()
{
}

}
