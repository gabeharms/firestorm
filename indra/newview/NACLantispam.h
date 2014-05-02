#ifndef NACL_ANTISPAM_H
#define NACL_ANTISPAM_H

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "llsingleton.h"
#include "llavatarnamecache.h"

typedef enum e_antispam_queue
{
	ANTISPAM_QUEUE_CHAT,
	ANTISPAM_QUEUE_INVENTORY,
	ANTISPAM_QUEUE_IM,
	ANTISPAM_QUEUE_CALLING_CARD,
	ANTISPAM_QUEUE_SOUND,
	ANTISPAM_QUEUE_SOUND_PRELOAD,
	ANTISPAM_QUEUE_SCRIPT_DIALOG,
	ANTISPAM_QUEUE_TELEPORT,
	ANTISPAM_QUEUE_MAX
} EAntispamQueue;

typedef enum e_antispam_source_type
{
	ANTISPAM_SOURCE_AGENT,
	ANTISPAM_SOURCE_OBJECT
} EAntispamSource;

struct AntispamObjectData
{
	std::string		mName;
	EAntispamQueue	mQueue;
	U32				mCount;
	U32				mPeriod;
	std::string		mNotificationId;
};

class NACLAntiSpamQueueEntry
{
	friend class NACLAntiSpamQueue;
	friend class NACLAntiSpamRegistry;

public:
	U32 getEntryAmount();
	U32 getEntryTime();

protected:
	NACLAntiSpamQueueEntry();

	void clearEntry();
	void updateEntryAmount();
	void updateEntryTime();
	bool getBlocked();
	void setBlocked();

private:
	U32		mEntryAmount;
	U32		mEntryTime;
	bool	mBlocked;
};

typedef boost::unordered_map<LLUUID, NACLAntiSpamQueueEntry*, FSUUIDHash> spam_queue_entry_map_t;
typedef boost::unordered_set<LLUUID, FSUUIDHash> collision_sound_set_t;

class NACLAntiSpamQueue
{
	friend class NACLAntiSpamRegistry;

public:
	U32 getAmount();
	U32 getTime();

protected:
	NACLAntiSpamQueue(U32 time, U32 amount);
	~NACLAntiSpamQueue();

	void setAmount(U32 amount);
	void setTime(U32 time);

	void blockEntry(const LLUUID& source);
	S32 checkEntry(const LLUUID& source, U32 multiplier);
	NACLAntiSpamQueueEntry* getEntry(const LLUUID& source);

	void clearEntries();
	void purgeEntries();

private:
	spam_queue_entry_map_t	mEntries;
	U32						mQueueAmount;
	U32						mQueueTime;
};

class NACLAntiSpamRegistry : public LLSingleton<NACLAntiSpamRegistry>
{
	friend class LLSingleton<NACLAntiSpamRegistry>;

public:
	void setGlobalQueue(bool value);
	void setGlobalAmount(U32 amount);
	void setGlobalTime(U32 time);
	void setRegisteredQueueTime(EAntispamQueue queue, U32 time);
	void setRegisteredQueueAmount(EAntispamQueue queue, U32 amount);
	void setAllQueueTimes(U32 amount);
	void setAllQueueAmounts(U32 time);

	void blockOnQueue(EAntispamQueue queue, const LLUUID& source);
	bool checkQueue(EAntispamQueue queue, const LLUUID& source, EAntispamSource sourcetype, U32 multiplier = 1);
	bool checkNewlineFlood(EAntispamQueue queue, const LLUUID& source, const std::string& message);
	bool isBlockedOnQueue(EAntispamQueue queue, const LLUUID& source);

	void clearRegisteredQueue(EAntispamQueue queue);
	void purgeRegisteredQueue(EAntispamQueue queue);
	void clearAllQueues();
	void purgeAllQueues();

	bool isCollisionSound(const LLUUID& sound_id);

	void processObjectPropertiesFamily(LLMessageSystem* msg);

private:
	NACLAntiSpamRegistry();
	~NACLAntiSpamRegistry();

	const char* getQueueName(EAntispamQueue queue);

	void blockGlobalEntry(const LLUUID& source);
	S32 checkGlobalEntry(const LLUUID& source, U32 multiplier);

	void clearGlobalEntries();
	void purgeGlobalEntries();

	void onAvatarNameCallback(const LLUUID& av_id, const LLAvatarName& av_name, AntispamObjectData data, const LLUUID& request_id);

	void notify(AntispamObjectData data);

	NACLAntiSpamQueue*		mQueues[ANTISPAM_QUEUE_MAX];
	spam_queue_entry_map_t	mGlobalEntries;
	U32						mGlobalTime;
	U32						mGlobalAmount;
	bool					mGlobalQueue;
	collision_sound_set_t	mCollisionSounds;

	std::map<LLUUID, AntispamObjectData>	mObjectData;
	std::map<LLUUID, LLAvatarNameCache::callback_connection_t> mAvatarNameCallbackConnections;
};
#endif // NACL_ANTISPAM_H
