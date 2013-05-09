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
#include "rlvactions.h"
#include "rlvhandler.h"

// ============================================================================
// RlvActions member functions
//

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

// ============================================================================
