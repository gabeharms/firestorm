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

#include "llviewerprecompiledheaders.h"
#include "llimview.h"
#include "llvoavatarself.h"
#include "rlvactions.h"
#include "rlvhandler.h"

// ============================================================================
// RlvActions member functions
//

// Checked: 2010-11-30 (RLVa-1.3.0)
bool RlvActions::canReceiveIM(const LLUUID& idSender)
{
	// User can receive an IM from "sender" (could be an agent or a group) if:
	//   - not generally restricted from receiving IMs (or the sender is an exception)
	//   - not specifically restricted from receiving an IM from the sender
	return 
		(!rlv_handler_t::isEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_RECVIM)) || (gRlvHandler.isException(RLV_BHVR_RECVIM, idSender)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_RECVIMFROM)) || (!gRlvHandler.isException(RLV_BHVR_RECVIMFROM, idSender)) ) );
}

// Checked: 2010-11-30 (RLVa-1.3.0)
bool RlvActions::canSendIM(const LLUUID& idRecipient)
{
	// User can send an IM to "recipient" (could be an agent or a group) if:
	//   - not generally restricted from sending IMs (or the recipient is an exception)
	//   - not specifically restricted from sending an IM to the recipient
	return 
		(!rlv_handler_t::isEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDIM)) || (gRlvHandler.isException(RLV_BHVR_SENDIM, idRecipient)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDIMTO)) || (!gRlvHandler.isException(RLV_BHVR_SENDIMTO, idRecipient)) ) );
}

// Checked: 2011-04-12 (RLVa-1.3.0)
bool RlvActions::canStartIM(const LLUUID& idRecipient)
{
	// User can start an IM session with "recipient" (could be an agent or a group) if:
	//   - not generally restricted from starting IM sessions (or the recipient is an exception)
	//   - not specifically restricted from starting an IM session with the recipient
	return 
		(!rlv_handler_t::isEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_STARTIM)) || (gRlvHandler.isException(RLV_BHVR_STARTIM, idRecipient)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_STARTIMTO)) || (!gRlvHandler.isException(RLV_BHVR_STARTIMTO, idRecipient)) ) );
}

// Checked: 2010-12-11 (RLVa-1.2.2)
bool RlvActions::canAcceptTpOffer(const LLUUID& idSender)
{
	return ((!gRlvHandler.hasBehaviour(RLV_BHVR_TPLURE)) || (gRlvHandler.isException(RLV_BHVR_TPLURE, idSender))) && (canStand());
}

// Checked: 2013-11-08 (RLVa-1.4.9)
bool RlvActions::autoAcceptTeleportOffer(const LLUUID& idSender)
{
	return ((idSender.notNull()) && (gRlvHandler.isException(RLV_BHVR_ACCEPTTP, idSender))) || (gRlvHandler.hasBehaviour(RLV_BHVR_ACCEPTTP));
}

// Checked: 2013-11-08 (RLVa-1.4.9)
bool RlvActions::canAcceptTpRequest(const LLUUID& idSender)
{
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_TPREQUEST)) || (gRlvHandler.isException(RLV_BHVR_TPREQUEST, idSender));
}

// Checked: 2013-11-08 (RLVa-1.4.9)
bool RlvActions::autoAcceptTeleportRequest(const LLUUID& idRequester)
{
	return ((idRequester.notNull()) && (gRlvHandler.isException(RLV_BHVR_ACCEPTTPREQUEST, idRequester))) || (gRlvHandler.hasBehaviour(RLV_BHVR_ACCEPTTPREQUEST));
}

// Checked: 2010-03-07 (RLVa-1.2.0)
bool RlvActions::canStand()
{
	// NOTE: return FALSE only if we're @unsit=n restricted and the avie is currently sitting on something and TRUE for everything else
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || ((isAgentAvatarValid()) && (!gAgentAvatarp->isSitting()));
}

// Checked: 2014-02-24 (RLVa-1.4.10)
bool RlvActions::canShowLocation()
{
	return !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
}

// Checked: 2013-05-10 (RLVa-1.4.9)
bool RlvActions::hasBehaviour(ERlvBehaviour eBhvr)
{
	return gRlvHandler.hasBehaviour(eBhvr);
}

// Checked: 2013-05-09 (RLVa-1.4.9)
bool RlvActions::hasOpenP2PSession(const LLUUID& idAgent)
{
	const LLUUID idSession = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, idAgent);
	return (idSession.notNull()) && (LLIMMgr::instance().hasSession(idSession));
}

// Checked: 2013-05-09 (RLVa-1.4.9)
bool RlvActions::hasOpenGroupSession(const LLUUID& idGroup)
{
	return (idGroup.notNull()) && (LLIMMgr::instance().hasSession(idGroup));
}

// Checked: 2013-11-08 (RLVa-1.4.9)
bool RlvActions::isRlvEnabled()
{
	return RlvHandler::isEnabled();
}

// ============================================================================
