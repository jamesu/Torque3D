//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
// Portions Copyright (c) 2013-2014 Mode 7 Limited
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(disable: 4996) 
#endif

#include "gfx/D3D9/gfxD3D9Device.h"
#include "gfx/D3D9/gfxD3D9EnumTranslate.h"
#include "gfx/bitmap/bitmapUtils.h"
#include "gfx/gfxCardProfile.h"
#include "core/strings/unicode.h"
#include "core/util/swizzle.h"
#include "core/util/safeDelete.h"
#include "console/console.h"
#include "core/resourceManager.h"

//-----------------------------------------------------------------------------
// Utility function, valid only in this file
#ifdef D3D_TEXTURE_SPEW
U32 GFXD3D9TextureObject::mTexCount = 0;
#endif

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
GFXD3D9TextureManager::GFXD3D9TextureManager( LPDIRECT3DDEVICE9 d3ddevice, U32 adapterIndex ) 
{
   mD3DDevice = d3ddevice;
   mAdapterIndex = adapterIndex;
   dMemset( mCurTexSet, 0, sizeof( mCurTexSet ) );   
   mD3DDevice->GetDeviceCaps(&mDeviceCaps);
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
GFXD3D9TextureManager::~GFXD3D9TextureManager()
{
   // Destroy texture table now so just in case some texture objects
   // are still left, we don't crash on a pure virtual method call.
   SAFE_DELETE_ARRAY( mHashTable );
}

//-----------------------------------------------------------------------------
// _innerCreateTexture
//-----------------------------------------------------------------------------
void GFXD3D9TextureManager::_innerCreateTexture( GFXD3D9TextureObject *retTex, 
                                               U32 height, 
                                               U32 width, 
                                               U32 depth,
                                               GFXFormat format, 
                                               GFXTextureProfile *profile, 
                                               U32 numMipLevels,
                                               bool forceMips,
                                               S32 antialiasLevel)
{
  PROFILE_SCOPE(GFXD3D9TextureManager__innerCreateTexture);

   GFXD3D9Device* d3d = static_cast<GFXD3D9Device*>(GFX);

   // Some relevant helper information...
   bool supportsAutoMips = GFX->getCardProfiler()->queryProfile("autoMipMapLevel", true);
   
   DWORD usage = 0;   // 0, D3DUSAGE_RENDERTARGET, or D3DUSAGE_DYNAMIC
   D3DPOOL pool = D3DPOOL_DEFAULT;

   retTex->mProfile = profile;

   D3DFORMAT d3dTextureFormat = GFXD3D9TextureFormat[format];

#ifndef TORQUE_OS_XENON
   if( retTex->mProfile->isDynamic() )
   {
      usage = D3DUSAGE_DYNAMIC;
   }
   else
   {
      usage = 0;
      pool = D3DPOOL_MANAGED;
   }

   if( retTex->mProfile->isRenderTarget() )
   {
      pool = D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_RENDERTARGET;
   }

   if(retTex->mProfile->isZTarget())
   {
      usage |= D3DUSAGE_DEPTHSTENCIL;
      pool = D3DPOOL_DEFAULT;
   }

   if( retTex->mProfile->isSystemMemory() )
   {
      pool = D3DPOOL_SYSTEMMEM;
   }

   if( supportsAutoMips && 
       !forceMips &&
       !retTex->mProfile->isSystemMemory() &&
       numMipLevels == 0 &&
       !(depth > 0) )
   {
      usage |= D3DUSAGE_AUTOGENMIPMAP;
   }
#else
   if(retTex->mProfile->isRenderTarget())
   {
      d3dTextureFormat = (D3DFORMAT)MAKELEFMT(d3dTextureFormat);
   }
#endif

   // Set the managed flag...
   retTex->isManaged = (pool == D3DPOOL_MANAGED);
   
   if( depth > 0 )
   {
      D3D9Assert( mD3DDevice->CreateVolumeTexture( width, height, depth, numMipLevels, usage, 
         d3dTextureFormat, pool, retTex->get3DTexPtr(), NULL), "Failed to create volume texture" );

      retTex->mTextureSize.set( width, height, depth );
      retTex->mMipLevels = retTex->get3DTex()->GetLevelCount();
      // required for 3D texture support - John Kabus
	  retTex->mFormat = format;
   }
   else
   {
#ifdef TORQUE_OS_XENON
      D3D9Assert( mD3DDevice->CreateTexture(width, height, numMipLevels, usage, d3dTextureFormat, pool, retTex->get2DTexPtr(), NULL), "Failed to create texture" );
      retTex->mMipLevels = retTex->get2DTex()->GetLevelCount();
#else
      // Figure out AA settings for depth and render targets
      D3DMULTISAMPLE_TYPE mstype;
      DWORD mslevel;

      switch (antialiasLevel)
      {
         case 0 :
            mstype = D3DMULTISAMPLE_NONE;
            mslevel = 0;
            break;
         case AA_MATCH_BACKBUFFER :
            mstype = d3d->getMultisampleType();
            mslevel = d3d->getMultisampleLevel();
            break;
         default :
            {
               mstype = D3DMULTISAMPLE_NONMASKABLE;
               mslevel = antialiasLevel;
#ifdef TORQUE_DEBUG
               DWORD MaxSampleQualities;      
               d3d->getD3D()->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dTextureFormat, FALSE, D3DMULTISAMPLE_NONMASKABLE, &MaxSampleQualities);
               AssertFatal(mslevel < MaxSampleQualities, "Invalid AA level!");
#endif
            }
            break;
      }

      if(retTex->mProfile->isZTarget())
      {
         D3D9Assert(mD3DDevice->CreateDepthStencilSurface(width, height, d3dTextureFormat,
            mstype, mslevel, retTex->mProfile->canDiscard(), retTex->getSurfacePtr(), NULL), "Failed to create Z surface" );

         retTex->mFormat = format; // Assigning format like this should be fine.
      }
      else
      {
         // Try to create the texture directly - should gain us a bit in high
         // performance cases where we know we're creating good stuff and we
         // don't want to bother with D3DX - slow function.
         HRESULT res = mD3DDevice->CreateTexture(width, height, numMipLevels, usage, d3dTextureFormat, pool, retTex->get2DTexPtr(), NULL);
		 D3D9Assert(res, "GFXD3D9TextureManager::_createTexture - failed to create texture!");

         // If this is a render target, and it wants AA or wants to match the backbuffer (for example, to share the z)
         // Check the caps though, if we can't stretchrect between textures, use the old RT method.  (Which hopefully means
         // that they can't force AA on us as well.)
         if (retTex->mProfile->isRenderTarget() && mslevel != 0 && (mDeviceCaps.Caps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES))
         {
            D3D9Assert(mD3DDevice->CreateRenderTarget(width, height, d3dTextureFormat, 
               mstype, mslevel, false, retTex->getSurfacePtr(), NULL),
               "GFXD3D9TextureManager::_createTexture - unable to create render target");
         }

         // All done!
		 IDirect3DTexture9 * t2dTex = retTex->get2DTex();
         retTex->mMipLevels = t2dTex->GetLevelCount();
      }
#endif

      // Get the actual size of the texture...
      D3DSURFACE_DESC probeDesc;
      ZeroMemory(&probeDesc, sizeof probeDesc);

      if( retTex->get2DTex() != NULL )
         D3D9Assert( retTex->get2DTex()->GetLevelDesc( 0, &probeDesc ), "Failed to get surface description");
      else if( retTex->getSurface() != NULL )
         D3D9Assert( retTex->getSurface()->GetDesc( &probeDesc ), "Failed to get surface description");

      retTex->mTextureSize.set(probeDesc.Width, probeDesc.Height, 0);
      
      S32 fmt = probeDesc.Format;

#if !defined(TORQUE_OS_XENON)
      GFXREVERSE_LOOKUP( GFXD3D9TextureFormat, GFXFormat, fmt );
      retTex->mFormat = (GFXFormat)fmt;
#else
      retTex->mFormat = format;
#endif
   }
}

//-----------------------------------------------------------------------------
// createTexture
//-----------------------------------------------------------------------------
GFXTextureObject *GFXD3D9TextureManager::_createTextureObject( U32 height, 
                                                               U32 width,
                                                               U32 depth,
                                                               GFXFormat format, 
                                                               GFXTextureProfile *profile, 
                                                               U32 numMipLevels,
                                                               bool forceMips, 
                                                               S32 antialiasLevel,
                                                               GFXTextureObject *inTex )
{
   GFXD3D9TextureObject *retTex;
   if ( inTex )
   {
      AssertFatal( dynamic_cast<GFXD3D9TextureObject*>( inTex ), "GFXD3D9TextureManager::_createTexture() - Bad inTex type!" );
      retTex = static_cast<GFXD3D9TextureObject*>( inTex );
      retTex->release();
   }      
   else
   {
      retTex = new GFXD3D9TextureObject( GFX, profile );
      retTex->registerResourceWithDevice( GFX );
   }

   _innerCreateTexture(retTex, height, width, depth, format, profile, numMipLevels, forceMips, antialiasLevel);

   return retTex;
}

//-----------------------------------------------------------------------------
// loadTexture - GBitmap
//-----------------------------------------------------------------------------
bool GFXD3D9TextureManager::_loadTexture(GFXTextureObject *aTexture, GBitmap *pDL)
{
   PROFILE_SCOPE(GFXD3D9TextureManager_loadTexture);

   GFXD3D9TextureObject *texture = static_cast<GFXD3D9TextureObject*>(aTexture);

#ifdef TORQUE_OS_XENON
   // If the texture is currently bound, it needs to be unbound before modifying it
   if( texture->getTex() && texture->getTex()->IsSet( mD3DDevice ) )
   {
      mD3DDevice->SetTexture( 0, NULL );
      mD3DDevice->SetTexture( 1, NULL );
      mD3DDevice->SetTexture( 2, NULL );
      mD3DDevice->SetTexture( 3, NULL );
      mD3DDevice->SetTexture( 4, NULL );
      mD3DDevice->SetTexture( 5, NULL );
      mD3DDevice->SetTexture( 6, NULL );
      mD3DDevice->SetTexture( 7, NULL );
   }
#endif

   // Check with profiler to see if we can do automatic mipmap generation.
   const bool supportsAutoMips = GFX->getCardProfiler()->queryProfile("autoMipMapLevel", true);

   // Helper bool
   const bool isCompressedTexFmt = aTexture->getFormat() >= GFXFormatDXT1 && aTexture->getFormat() <= GFXFormatDXT5;

   GFXD3D9Device* dev = static_cast<GFXD3D9Device *>(GFX);

   if (isCompressedTexFmt)
   {
      return _loadTextureDDS(texture, pDL, aTexture->getMipLevels());
   }

   // Settings for mipmap generation
   U32 maxDownloadMip = pDL->getNumMipLevels();
   U32 nbMipMapLevel  = pDL->getNumMipLevels();

   if( supportsAutoMips && !isCompressedTexFmt )
   {
      maxDownloadMip = 1;
      nbMipMapLevel  = aTexture->getMipLevels();
   }

   // Fill the texture...
   for( int i = 0; i < maxDownloadMip; i++ )
   {
      LPDIRECT3DSURFACE9 surf = NULL;
      D3D9Assert(texture->get2DTex()->GetSurfaceLevel( i, &surf ), "Failed to get surface");

      D3DLOCKED_RECT lockedRect;

#ifdef TORQUE_OS_XENON
      // On the 360, doing a LockRect doesn't work like it does with untiled memory
      // so instead swizzle into some temporary memory, and then later use D3DX
      // to do the upload properly.
      FrameTemp<U8> swizzleMem(pDL->getWidth(i) * pDL->getHeight(i) * pDL->getBytesPerPixel());
      lockedRect.pBits = (void*)~swizzleMem;
#else
      U32 waterMark = 0;
      surf->LockRect( &lockedRect, NULL, 0 );
#endif
      
      switch( texture->mFormat )
      {
      case GFXFormatR8G8B8:
         {
            PROFILE_SCOPE(Swizzle24_Upload);
            AssertFatal( pDL->getFormat() == GFXFormatR8G8B8, "Assumption failed" );
            GFX->getDeviceSwizzle24()->ToBuffer( lockedRect.pBits, pDL->getBits(i), 
               pDL->getWidth(i) * pDL->getHeight(i) * pDL->getBytesPerPixel() );
         }
         break;

      case GFXFormatR8G8B8A8:
      case GFXFormatR8G8B8X8:
         {
            PROFILE_SCOPE(Swizzle32_Upload);
            GFX->getDeviceSwizzle32()->ToBuffer( lockedRect.pBits, pDL->getBits(i), 
               pDL->getWidth(i) * pDL->getHeight(i) * pDL->getBytesPerPixel() );
         }
         break;

      default:
         {
            // Just copy the bits in no swizzle or padding
            PROFILE_SCOPE(SwizzleNull_Upload);
            AssertFatal( pDL->getFormat() == texture->mFormat, "Format mismatch" );
            dMemcpy( lockedRect.pBits, pDL->getBits(i), 
               pDL->getWidth(i) * pDL->getHeight(i) * pDL->getBytesPerPixel() );
         }
      }

#ifdef TORQUE_OS_XENON
      RECT srcRect;
      srcRect.bottom = pDL->getHeight(i);
      srcRect.top = 0;
      srcRect.left = 0;
      srcRect.right = pDL->getWidth(i);

      D3DXLoadSurfaceFromMemory(surf, NULL, NULL, ~swizzleMem, (D3DFORMAT)MAKELINFMT(GFXD3D9TextureFormat[pDL->getFormat()]),
         pDL->getWidth(i) * pDL->getBytesPerPixel(), NULL, &srcRect, false, 0, 0, D3DX_FILTER_NONE, 0);
#else
	  surf->UnlockRect();
#endif
      
      surf->Release();
   }

   return true;          
}

//-----------------------------------------------------------------------------
// loadTexture - raw
//-----------------------------------------------------------------------------
bool GFXD3D9TextureManager::_loadTexture( GFXTextureObject *inTex, void *raw )
{
   PROFILE_SCOPE(GFXD3D9TextureManager_loadTextureRaw);

   GFXD3D9TextureObject *texture = (GFXD3D9TextureObject *) inTex;

   // currently only for volume textures...
   if( texture->getDepth() < 1 ) return false;

   D3DBOX box;
   box.Left    = 0;
   box.Right   = texture->getWidth();
   box.Front   = 0;
   box.Back    = texture->getDepth();
   box.Top     = 0;
   box.Bottom  = texture->getHeight();

   LPDIRECT3DVOLUME9 volume = NULL;
   D3D9Assert( texture->get3DTex()->GetVolumeLevel( 0, &volume ), "Failed to load volume" );

   D3DLOCKED_BOX lockedBox;
   volume->LockBox( &lockedBox, &box, 0 );

   dMemcpy( lockedBox.pBits, raw, lockedBox.SlicePitch * texture->getDepth() );

   volume->UnlockBox();

   volume->Release();

   return true;
}

//-----------------------------------------------------------------------------
// refreshTexture
//-----------------------------------------------------------------------------
bool GFXD3D9TextureManager::_refreshTexture(GFXTextureObject *texture)
{
   U32 usedStrategies = 0;
   GFXD3D9TextureObject *realTex = static_cast<GFXD3D9TextureObject *>( texture );

   if(texture->getProfile()->doStoreBitmap())
   {
//      SAFE_RELEASE(realTex->mD3DTexture);
//      _innerCreateTexture(realTex, texture->mTextureSize.x, texture->mTextureSize.y, texture->mFormat, texture->mProfile, texture->mMipLevels);

      if(texture->getBitmap())
         _loadTexture(texture, texture->getBitmap());

      if(texture->getDDS())
         _loadTexture(texture, texture->getDDS());

      usedStrategies++;
   }

   if(texture->getProfile()->isRenderTarget() || texture->getProfile()->isDynamic() ||
     texture->getProfile()->isZTarget())
   {
      realTex->release();
      _innerCreateTexture(realTex, texture->getHeight(), texture->getWidth(), texture->getDepth(), texture->getFormat(), 

         texture->getProfile(), texture->getMipLevels(), false, texture->getAntialiasLevel());
      usedStrategies++;
   }

   AssertFatal(usedStrategies < 2, "GFXD3D9TextureManager::_refreshTexture - Inconsistent profile flags!");

   return true;
}


//-----------------------------------------------------------------------------
// freeTexture
//-----------------------------------------------------------------------------
bool GFXD3D9TextureManager::_freeTexture(GFXTextureObject *texture, bool zombify)
{
   AssertFatal(dynamic_cast<GFXD3D9TextureObject *>(texture),"Not an actual d3d texture object!");
   GFXD3D9TextureObject *tex = static_cast<GFXD3D9TextureObject *>( texture );

   // If it's a managed texture and we're zombifying, don't blast it, D3D allows
   // us to keep it.
   if(zombify && tex->isManaged)
      return true;

   tex->release();

   return true;
}

bool GFXD3D9TextureManager::_loadTextureDDS(GFXD3D9TextureObject *texture, GBitmap *dds, const U32 texMips)
{
   PROFILE_SCOPE(GFXD3D9TextureManager_loadTextureDDSGBM);

   // Fill the texture...
   int mipCount = texMips;
   for( int i = 0; i < mipCount; i++ )
   {
      PROFILE_SCOPE(GFXD3DTexMan_loadSurface);

      LPDIRECT3DSURFACE9 surf = NULL;
      D3D9Assert(texture->get2DTex()->GetSurfaceLevel( i, &surf ), "Failed to get surface");

#if defined(TORQUE_OS_XENON)
      XGTEXTURE_DESC surfDesc;
      dMemset(&surfDesc, 0, sizeof(XGTEXTURE_DESC));
      XGGetSurfaceDesc(surf, &surfDesc);

      RECT srcRect;
      srcRect.top = srcRect.left = 0;
      srcRect.bottom = dds->getHeight(i);
      srcRect.right = dds->getWidth(i);

      D3DXLoadSurfaceFromMemory(surf, NULL, NULL, dds->getAddress(0, 0, i),
         (D3DFORMAT)MAKELINFMT(GFXD3D9TextureFormat[dds->mFormat]), dds->getPitch(i), 
         NULL, &srcRect, false, 0, 0, D3DX_FILTER_NONE, 0);
#else

      GFXD3D9Device* dev = static_cast<GFXD3D9Device *>(GFX);

	  D3DLOCKED_RECT lockedRect;
	  D3D9Assert( surf->LockRect( &lockedRect, NULL, 0 ), "Failed to lock surface level for load" );

	  if ( dds->getPitch( i ) != lockedRect.Pitch )
	  {
		  // Do a row-by-row copy.
		  U32 srcPitch = dds->getPitch( i );
		  U32 srcHeight = dds->getHeight();
		  U8* srcBytes = dds->getAddress(0, 0, i);
		  U8* dstBytes = (U8*)lockedRect.pBits;
		  
		  U32 numRows = 0;
		  U32 rowSize = 0;
		  GFX_getTextureMetrics(dds->getFormat(), dds->getWidth(i), dds->getHeight(i), &numRows, &rowSize);

		  for (U32 i = 0; i<numRows; i++)          
		  {
			  dMemcpy( dstBytes, srcBytes, rowSize );
			  dstBytes += lockedRect.Pitch;
			  srcBytes += rowSize;
		  }

		  surf->UnlockRect();
		  surf->Release();

		  continue;
	  }

	  dMemcpy( lockedRect.pBits, dds->getAddress(0,0,i), dds->getSurfaceSize(i) );

	  surf->UnlockRect();
#endif
      surf->Release();
   }

   return true;
}

/// Load a texture from a proper DDSFile instance.
bool GFXD3D9TextureManager::_loadTexture(GFXTextureObject *aTexture, DDSFile *dds)
{
   PROFILE_SCOPE(GFXD3D9TextureManager_loadTextureDDS);

   GFXD3D9TextureObject *texture = static_cast<GFXD3D9TextureObject*>(aTexture);

   // Fill the texture...
   int mipCount = aTexture->getMipLevels();
   for( int i = 0; i < mipCount; i++ )
   {
      PROFILE_SCOPE(GFXD3DTexMan_loadSurface);

      LPDIRECT3DSURFACE9 surf = NULL;
      D3D9Assert(texture->get2DTex()->GetSurfaceLevel( i, &surf ), "Failed to get surface");

#if defined(TORQUE_OS_XENON)
      XGTEXTURE_DESC surfDesc;
      dMemset(&surfDesc, 0, sizeof(XGTEXTURE_DESC));
      XGGetSurfaceDesc(surf, &surfDesc);

      RECT srcRect;
      srcRect.top = srcRect.left = 0;
      srcRect.bottom = dds->getHeight(i);
      srcRect.right = dds->getWidth(i);

      D3DXLoadSurfaceFromMemory(surf, NULL, NULL, dds->mSurfaces[0]->mMips[i],
         (D3DFORMAT)MAKELINFMT(GFXD3D9TextureFormat[dds->mFormat]), dds->getSurfacePitch(i), 
         NULL, &srcRect, false, 0, 0, D3DX_FILTER_NONE, 0);
#else

      GFXD3D9Device* dev = static_cast<GFXD3D9Device *>(GFX);

	  D3DLOCKED_RECT lockedRect;
	  D3D9Assert( surf->LockRect( &lockedRect, NULL, 0 ), "Failed to lock surface level for load" );

	  AssertFatal( dds->mSurfaces.size() > 0, "Assumption failed. DDSFile has no surfaces." );

	  if ( dds->getSurfacePitch( i ) != lockedRect.Pitch )
	  {
		  // Do a row-by-row copy.
		  U32 srcPitch = dds->getSurfacePitch( i );
		  U32 srcHeight = dds->getHeight();
		  U8* srcBytes = dds->mSurfaces[0]->mMips[i];
		  U8* dstBytes = (U8*)lockedRect.pBits;

		  U32 numRows = 0;
		  U32 rowSize = 0;
		  GFX_getTextureMetrics(dds->getFormat(), dds->getWidth(i), dds->getHeight(i), &numRows, &rowSize);

		  for (U32 i = 0; i<numRows; i++)          
		  {
			  dMemcpy( dstBytes, srcBytes, rowSize );
			  dstBytes += lockedRect.Pitch;
			  srcBytes += rowSize;
		  }

		  surf->UnlockRect();
		  surf->Release();

		  continue;
	  }

	  dMemcpy( lockedRect.pBits, dds->mSurfaces[0]->mMips[i], dds->getSurfaceSize(i) );

	  surf->UnlockRect();
#endif

      surf->Release();
   }

   return true;
}