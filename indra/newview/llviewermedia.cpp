/** 
 * @file llviewermedia.cpp
 * @author Callum Prentice & James Cook
 * @brief Client interface to the media engine
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */
#include "llviewerprecompiledheaders.h"

#include "llviewermedia.h"

#include "llmimetypes.h"
#include "llviewercontrol.h"
#include "llviewerimage.h"
#include "llviewerwindow.h"
#include "llversionviewer.h"
#include "llviewerimagelist.h"

#include "llevent.h"		// LLSimpleListener
#include "llmediamanager.h"
#include "lluuid.h"

// don't want to include llappviewer.h
extern std::string gChannelName;

// Implementation functions not exported into header file
class LLViewerMediaImpl
	:	public LLMediaObserver
{
	public:
		LLViewerMediaImpl()
		:	mMediaSource( NULL ),
			mMovieImageID(),
			mMovieImageHasMips(false)
		{ }

		void initControlListeners();

		void destroyMediaSource();

	    void play(const std::string& media_url,
				  const std::string& mime_type,
				  const LLUUID& placeholder_texture_id,
				  S32 media_width, S32 media_height, U8 media_auto_scale,
				  U8 media_loop);
	
		void stop();
		void pause();
		void start();
    	void seek(F32 time);
    	void setVolume(F32 volume);
		LLMediaBase::EStatus getStatus();

		/*virtual*/ void onMediaSizeChange(const EventType& event_in);
		/*virtual*/ void onMediaContentsChange(const EventType& event_in);

		void updateMovieImage(const LLUUID& image_id, BOOL active);
		void updateImagesMediaStreams();
		LLUUID getMediaTextureID();

	public:

		// a single media url with some data and an impl.
		LLMediaBase* mMediaSource;	
		LLUUID mMovieImageID;
		bool  mMovieImageHasMips;
		std::string mMediaURL;		
		std::string mMimeType;
    private:
	    void initializePlaceholderImage(LLViewerImage *placeholder_image, LLMediaBase *media_source);
};

static LLViewerMediaImpl sViewerMediaImpl;

void LLViewerMediaImpl::destroyMediaSource()
{
	LLMediaManager* mgr = LLMediaManager::getInstance();
	if ( mMediaSource )
	{
		bool was_playing = LLViewerMedia::isMediaPlaying();
		mMediaSource->remObserver(this);
		mgr->destroySource( mMediaSource );

		// Restore the texture
		updateMovieImage(LLUUID::null, was_playing);

	}
	mMediaSource = NULL;
}

void LLViewerMediaImpl::play(const std::string& media_url,
							 const std::string& mime_type,
							 const LLUUID& placeholder_texture_id,
							 S32 media_width, S32 media_height, U8 media_auto_scale,
							 U8 media_loop)
{
	// first stop any previously playing media
	stop();
	
	// Save this first, as init/load below may fire events
	mMovieImageID = placeholder_texture_id;

	// If the mime_type passed in is different than the cached one, and 
	// Auto-discovery is turned OFF, replace the cached mime_type with the new one.
	if(mime_type != mMimeType && 
		! gSavedSettings.getBOOL("AutoMimeDiscovery"))
	{
		mMimeType = mime_type;
	}
	LLURI url(media_url);
	std::string scheme = url.scheme() != "" ? url.scheme() : "http";
	
	LLMediaManager* mgr = LLMediaManager::getInstance();
	mMediaSource = mgr->createSourceFromMimeType(scheme, mMimeType );
	if ( !mMediaSource )
	{
		llwarns << "media source create failed " << media_url
			<< " type " << mMimeType
			<< llendl;
		return;
	}
	
	if ((media_width != 0) && (media_height != 0))
	{
		mMediaSource->setRequestedMediaSize(media_width, media_height);
	}
	
	mMediaSource->setLooping(media_loop);
	mMediaSource->setAutoScaled(media_auto_scale);
	mMediaSource->addObserver( this );
	mMediaSource->navigateTo( media_url );
	mMediaSource->addCommand(LLMediaBase::COMMAND_START);

	// Store the URL and Mime Type
	mMediaURL = media_url;

}

void LLViewerMediaImpl::stop()
{
	destroyMediaSource();
}

void LLViewerMediaImpl::pause()
{
	if(mMediaSource)
	{
		mMediaSource->addCommand(LLMediaBase::COMMAND_PAUSE);
	}
}

void LLViewerMediaImpl::start()
{
	if(mMediaSource)
	{
		mMediaSource->addCommand(LLMediaBase::COMMAND_START);
	}
}

void LLViewerMediaImpl::seek(F32 time)
{
	if(mMediaSource)
	{
		mMediaSource->seek(time);
	}
}

void LLViewerMediaImpl::setVolume(F32 volume)
{
	if(mMediaSource)
	{
		mMediaSource->setVolume( volume);
	}
}

LLMediaBase::EStatus LLViewerMediaImpl::getStatus()
{
	if (mMediaSource)
	{
		return mMediaSource->getStatus();
	}
	else
	{
		return LLMediaBase::STATUS_UNKNOWN;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMediaImpl::updateMovieImage(const LLUUID& uuid, BOOL active)
{
	// IF the media image hasn't changed, do nothing
	if (mMovieImageID == uuid)
	{
		return;
	}
	// If we have changed media uuid, restore the old one
	if (!mMovieImageID.isNull())
	{
		LLViewerImage* oldImage = LLViewerImage::getImage( mMovieImageID );
		if (oldImage)
		{
			oldImage->reinit(mMovieImageHasMips);
			oldImage->mIsMediaTexture = FALSE;
		}
		mMovieImageID.setNull();
	}
	// If the movie is playing, set the new media image
	if (active && !uuid.isNull())
	{
		LLViewerImage* viewerImage = LLViewerImage::getImage( uuid );
		if( viewerImage )
		{
			mMovieImageID = uuid;
			// Can't use mipmaps for movies because they don't update the full image
			mMovieImageHasMips = viewerImage->getUseMipMaps();
			viewerImage->reinit(FALSE);
			viewerImage->mIsMediaTexture = TRUE;
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMediaImpl::updateImagesMediaStreams()
{
	LLMediaManager::updateClass();
}

void LLViewerMediaImpl::initializePlaceholderImage(LLViewerImage *placeholder_image, LLMediaBase *media_source)
{
	int media_width = media_source->getMediaWidth();
	int media_height = media_source->getMediaHeight();
	//int media_rowspan = media_source->getMediaRowSpan();

	// if width & height are invalid, don't bother doing anything
	if ( media_width < 1 || media_height < 1 ) 
		return;

	llinfos << "initializing media placeholder" << llendl;
	llinfos << "movie image id " << mMovieImageID << llendl;

	int texture_width = LLMediaManager::textureWidthFromMediaWidth( media_width );
	int texture_height = LLMediaManager::textureHeightFromMediaHeight( media_height );
	int texture_depth = media_source->getMediaDepth();

	// MEDIAOPT: check to see if size actually changed before doing work
	placeholder_image->destroyGLTexture();
	// MEDIAOPT: apparently just calling setUseMipMaps(FALSE) doesn't work?
	placeholder_image->reinit(FALSE);	// probably not needed

	// MEDIAOPT: seems insane that we actually have to make an imageraw then
	// immediately discard it
	LLPointer<LLImageRaw> raw = new LLImageRaw(texture_width, texture_height, texture_depth);
	raw->clear(0x0f, 0x0f, 0x0f, 0xff);	
	int discard_level = 0;

	// ask media source for correct GL image format constants
	placeholder_image->setExplicitFormat(media_source->getTextureFormatInternal(), 
										 media_source->getTextureFormatPrimary(), 
										 media_source->getTextureFormatType());

	placeholder_image->createGLTexture(discard_level, raw);

	// placeholder_image->setExplicitFormat()
	placeholder_image->setUseMipMaps(FALSE);

	// MEDIAOPT: set this dynamically on play/stop
	placeholder_image->mIsMediaTexture = true;
}



// virtual
void LLViewerMediaImpl::onMediaContentsChange(const EventType& event_in)
{
	LLMediaBase* media_source = event_in.getSubject();
	LLViewerImage* placeholder_image = gImageList.getImage( mMovieImageID );
	if ((placeholder_image) && (placeholder_image->getHasGLTexture()))
	{
		if (placeholder_image->getUseMipMaps())
		{
			// bad image!  NO MIPMAPS!
			initializePlaceholderImage(placeholder_image, media_source);
		}

		U8* data = media_source->getMediaData();
		S32 x_pos = 0;
		S32 y_pos = 0;
		S32 width = media_source->getMediaWidth();
		S32 height = media_source->getMediaHeight();
		S32 data_width = media_source->getMediaDataWidth();
		S32 data_height = media_source->getMediaDataHeight();
		placeholder_image->setSubImage(data, data_width, data_height,
			x_pos, y_pos, width, height);
	}
}


// virtual
void LLViewerMediaImpl::onMediaSizeChange(const EventType& event_in)
{
	LLMediaBase* media_source = event_in.getSubject();
	LLViewerImage* placeholder_image = gImageList.getImage( mMovieImageID );
	if (placeholder_image)
	{
		initializePlaceholderImage(placeholder_image, media_source);
	}
	else
	{
		llinfos << "no placeholder image" << llendl;
	}
}


		// Get the image we're using

	/*
	// update media stream if required
	LLMediaEngine* media_engine = LLMediaEngine::getInstance();
	if (media_engine)
	{
		if ( media_engine->update() )
		{
			LLUUID media_uuid = media_engine->getImageUUID();
			updateMovieImage(media_uuid, TRUE);
			if (!media_uuid.isNull())
			{
				LLViewerImage* viewerImage = getImage( media_uuid );
				if( viewerImage )
				{
					LLMediaBase* renderer = media_engine->getMediaRenderer();
					if ((renderer->getTextureWidth() != viewerImage->getWidth()) ||
						(renderer->getTextureHeight() != viewerImage->getHeight()) ||
						(renderer->getTextureDepth() != viewerImage->getComponents()) ||
						(viewerImage->getHasGLTexture() == FALSE))
					{
						// destroy existing GL image
						viewerImage->destroyGLTexture();
				
						// set new size
						viewerImage->setSize( renderer->getTextureWidth(),
												renderer->getTextureHeight(),
												renderer->getTextureDepth() );

						LLPointer<LLImageRaw> raw = new LLImageRaw(renderer->getTextureWidth(),
																	renderer->getTextureHeight(),
																	renderer->getTextureDepth());
						raw->clear(0x7f,0x7f,0x7f,0xff);
						viewerImage->createGLTexture(0, raw);
					}

					// Set the explicit format the instance wants
					viewerImage->setExplicitFormat(renderer->getTextureFormatInternal(), 
													renderer->getTextureFormatPrimary(), 
													renderer->getTextureFormatType(),
													renderer->getTextureFormatSwapBytes());
					// This should be redundant, but just in case:
					viewerImage->setUseMipMaps(FALSE);

					LLImageRaw* rawImage = media_engine->getImageRaw();
					if ( rawImage )
					{
						viewerImage->setSubImage(rawImage, 0, 0,
													renderer->getMediaWidth(),
													renderer->getMediaHeight());
					}
				}
				else
				{
					llwarns << "MediaEngine update unable to get viewer image for GL texture" << llendl;
				}
			}
		}
		else
		{
			LLUUID media_uuid = media_engine->getImageUUID();
			updateMovieImage(media_uuid, FALSE);
		}
	}
	*/


//////////////////////////////////////////////////////////////////////////////////////////
LLUUID LLViewerMediaImpl::getMediaTextureID()
{
	return mMovieImageID;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Wrapper class
//////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::initClass()
{
	LLMediaManagerData* init_data = new LLMediaManagerData;

//	std::string executable_dir = std::string( arg0 ).substr( 0, std::string( arg0 ).find_last_of("\\/") );
//	std::string component_dir = std::string( executable_dir ).substr( 0, std::string( executable_dir ).find_last_of("\\/") );
//	component_dir = std::string( component_dir ).substr( 0, std::string( component_dir ).find_last_of("\\/") );
//	component_dir = std::string( component_dir ).substr( 0, std::string( component_dir ).find_last_of("\\/") );
//	component_dir += "\\newview\\app_settings\\mozilla";


#if LL_DARWIN
	// For Mac OS, we store both the shared libraries and the runtime files (chrome/, plugins/, etc) in
	// Second Life.app/Contents/MacOS/.  This matches the way Firefox is distributed on the Mac.
	std::string component_dir(gDirUtilp->getExecutableDir());
#elif LL_WINDOWS
	std::string component_dir( gDirUtilp->getExpandedFilename( LL_PATH_APP_SETTINGS, "" ) );
	component_dir += gDirUtilp->getDirDelimiter();
  #ifdef LL_DEBUG
	component_dir += "mozilla_debug";
  #else // LL_DEBUG
	component_dir += "mozilla";
  #endif // LL_DEBUG
#elif LL_LINUX
	std::string component_dir( gDirUtilp->getExpandedFilename( LL_PATH_APP_SETTINGS, "" ) );
	component_dir += gDirUtilp->getDirDelimiter();
	component_dir += "mozilla-runtime-linux-i686";
#else
	std::string component_dir( gDirUtilp->getExpandedFilename( LL_PATH_APP_SETTINGS, "" ) );
	component_dir += gDirUtilp->getDirDelimiter();
	component_dir += "mozilla";
#endif

	// append our magic version number string to the browser user agent id
	std::ostringstream codec;
	codec << "[Second Life ";
	codec << "(" << gChannelName << ")";
	codec << " - " << LL_VERSION_MAJOR << "." << LL_VERSION_MINOR << "." << LL_VERSION_PATCH << "." << LL_VERSION_BUILD;
	codec << "]";
	init_data->setBrowserUserAgentId( codec.str() );

	std::string application_dir = gDirUtilp->getExecutableDir();
	
	init_data->setBrowserApplicationDir( application_dir );
	std::string profile_dir = gDirUtilp->getExpandedFilename( LL_PATH_MOZILLA_PROFILE, "" );
	init_data->setBrowserProfileDir( profile_dir );
	init_data->setBrowserComponentDir( component_dir );
	std::string profile_name("Second Life");
	init_data->setBrowserProfileName( profile_name );
	init_data->setBrowserParentWindow( gViewerWindow->getPlatformWindow() );

	LLMediaManager::initClass( init_data );

	LLMediaManager* mm = LLMediaManager::getInstance();
	LLMIMETypes::mime_info_map_t::const_iterator it;
	for (it = LLMIMETypes::sMap.begin(); it != LLMIMETypes::sMap.end(); ++it)
	{
		const LLString& mime_type = it->first;
		const LLMIMETypes::LLMIMEInfo& info = it->second;
		mm->addMimeTypeImplNameMap( mime_type, info.mImpl );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::cleanupClass()
{
	LLMediaManager::cleanupClass();
}

// static
void LLViewerMedia::play(const std::string& media_url,
						 const std::string& mime_type,
						 const LLUUID& placeholder_texture_id,
						 S32 media_width, S32 media_height, U8 media_auto_scale,
						 U8 media_loop)
{
	sViewerMediaImpl.play(media_url, mime_type, placeholder_texture_id,
						  media_width, media_height, media_auto_scale, media_loop);
}

// static
void LLViewerMedia::stop()
{
	sViewerMediaImpl.stop();
}

// static
void LLViewerMedia::pause()
{
	sViewerMediaImpl.pause();
}

// static
void LLViewerMedia::start()
{
	sViewerMediaImpl.start();
}

// static
void LLViewerMedia::seek(F32 time)
{
	sViewerMediaImpl.seek(time);
}

// static
void LLViewerMedia::setVolume(F32 volume)
{
	sViewerMediaImpl.setVolume(volume);
}

// static
LLMediaBase::EStatus LLViewerMedia::getStatus()
{
	return sViewerMediaImpl.getStatus();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
LLUUID LLViewerMedia::getMediaTextureID()
{
	return sViewerMediaImpl.getMediaTextureID();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::getMediaSize(S32 *media_width, S32 *media_height)
{
	// make sure we're valid

	if ( sViewerMediaImpl.mMediaSource != NULL )
	{
		*media_width = sViewerMediaImpl.mMediaSource->getMediaWidth(); 
		*media_height = sViewerMediaImpl.mMediaSource->getMediaHeight();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::getTextureSize(S32 *texture_width, S32 *texture_height)
{
	if ( sViewerMediaImpl.mMediaSource != NULL )
	{
		S32 media_width = sViewerMediaImpl.mMediaSource->getMediaWidth(); 
		S32 media_height = sViewerMediaImpl.mMediaSource->getMediaHeight();
		*texture_width = LLMediaManager::textureWidthFromMediaWidth( media_width );
		*texture_height = LLMediaManager::textureHeightFromMediaHeight( media_height );
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::updateImagesMediaStreams()
{
	sViewerMediaImpl.updateImagesMediaStreams();
}
//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::isMediaPlaying()
{
	LLMediaBase::EStatus status = sViewerMediaImpl.getStatus();
	return (status == LLMediaBase::STATUS_STARTED ); 
}
//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::isMediaPaused()
{
	LLMediaBase::EStatus status = sViewerMediaImpl.getStatus();
	return (status == LLMediaBase::STATUS_PAUSED); 
}
//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::hasMedia()
{
	return sViewerMediaImpl.mMediaSource != NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
//static 
bool LLViewerMedia::isActiveMediaTexture(const LLUUID& id)
{
	return (id.notNull()
		&& id == getMediaTextureID()
		&& isMediaPlaying());
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
std::string LLViewerMedia::getMediaURL()
{
	return sViewerMediaImpl.mMediaURL;
}
//////////////////////////////////////////////////////////////////////////////////////////
// static
std::string LLViewerMedia::getMimeType()
{
	return sViewerMediaImpl.mMimeType;
}
//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::setMimeType(std::string mime_type)
{
	sViewerMediaImpl.mMimeType = mime_type;
}


