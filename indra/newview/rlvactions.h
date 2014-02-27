/** 
 *
 * Copyright (c) 2009-2013, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#ifndef RLV_ACTIONS_H
#define RLV_ACTIONS_H

#include "rlvdefines.h"

// ============================================================================
// RlvActions class declaration - developer-friendly non-RLVa code facing class, use in lieu of RlvHandler whenever possible
//

class RlvActions
{
	// =============
	// Communication
	// =============
public:
	/*
	 * Returns true if the user is allowed to receive IMs from the specified sender (can be an avatar or a group)
	 */
	static bool canReceiveIM(const LLUUID& idSender);

	/*
	 * Returns true if the user is allowed to send IMs to the specified recipient (can be an avatar or a group)
	 */
	static bool canSendIM(const LLUUID& idRecipient);

	/*
	 * Returns true if the user is allowed to start a - P2P or group - conversation with the specified UUID.
	 */
	static bool canStartIM(const LLUUID& idRecipient);								// @startim and @startimto

	// ========
	// Movement
	// ========
public:
	/*
	 * Returns true if the user can accept an incoming teleport offer from the specified avatar
	 */
	static bool canAcceptTpOffer(const LLUUID& idSender);

	/*
	 * Returns true if a teleport offer from the specified avatar should be auto-accepted
	 * (pass the null UUID to check if all teleport offers should be auto-accepted regardless of sender)
	 */
	static bool autoAcceptTeleportOffer(const LLUUID& idSender);

	/*
	 * Returns true if the user can accept an incoming teleport request from the specified avatar
	 */
	static bool canAcceptTpRequest(const LLUUID& idSender);

	/*
	 * Returns true if a teleport request from the specified avatar should be auto-accepted
	 * (pass the null UUID to check if all teleport requests should be auto-accepted regardless of requester)
	 */
	static bool autoAcceptTeleportRequest(const LLUUID& idRequester);

	// =================
	// World interaction
	// =================
public:
	/*
	 * Returns true if the user can stand up (returns true if the user isn't currently sitting)
	 */
	static bool canStand();

	/*
	 * Returns true if the user can see their in-world location
	 */
	static bool canShowLocation();

	// ================
	// Helper functions
	// ================
public:
	/*
	 * Convenience function to check for a behaviour without having to include rlvhandler.h. 
	 * Do NOT call this function if speed is important (i.e. per-frame)
	 */
	static bool hasBehaviour(ERlvBehaviour eBhvr);

	/*
	 * Returns true if a - P2P or group - IM session is open with the specified UUID.
	 */
	static bool hasOpenP2PSession(const LLUUID& idAgent);
	static bool hasOpenGroupSession(const LLUUID& idGroup);

	/*
	 * Convenience function to check if RLVa is enabled without having to include rlvhandler.h
	 */
	static bool isRlvEnabled();
};

// ============================================================================

#endif // RLV_ACTIONS_H
