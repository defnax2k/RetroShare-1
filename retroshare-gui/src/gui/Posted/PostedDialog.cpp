/*
 * Retroshare Posted Dialog
 *
 * Copyright 2012-2012 by Robert Fernie.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License Version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 *
 * Please report all bugs and problems to "retroshare@lunamutt.com".
 *
 */

#include "PostedDialog.h"
#include "PostedItem.h"
#include "PostedGroupDialog.h"
#include "PostedListWidget.h"
#include "PostedUserNotify.h"
#include "gui/gxs/GxsGroupShareKey.h"
#include "gui/settings/rsharesettings.h"
#include "gui/common/GroupTreeWidget.h"

#include <retroshare/rsposted.h>

class PostedGroupInfoData : public RsUserdata
{
public:
	PostedGroupInfoData() : RsUserdata() {}

public:
	QMap<RsGxsGroupId, QString> mDescription;
};

/** Constructor */
PostedDialog::PostedDialog(QWidget *parent)
    : GxsGroupFrameDialog(rsPosted, parent)
{
}

PostedDialog::~PostedDialog()
{
}

UserNotify *PostedDialog::getUserNotify(QObject *parent)
{
	return new PostedUserNotify(rsPosted, parent);
}

QString PostedDialog::getHelpString() const
{
	QString hlp_str = tr("<h1><img width=\"32\" src=\":/home/img/question-64.png\">&nbsp;&nbsp;Posted</h1>    \
    <p>The posted service allows you to share internet links, that spread among UnseenP2P nodes like forums and \
	 channels</p> \
	 <p>Links can be commented by subscribed users. A promotion system also gives the opportunity to  \
	 enlight important links.</p> \
     <p>There is no restriction on which links are shared. Be careful when clicking on them.</p>\
    <p>Posted links are kept for %1 days, and sync-ed over the last %2 days, unless you change this.</p>\
                ").arg(QString::number(rsPosted->getDefaultStoragePeriod()/86400)).arg(QString::number(rsPosted->getDefaultSyncPeriod()/86400));

	return hlp_str ;
}

QString PostedDialog::text(TextType type)
{
	switch (type) {
	case TEXT_NAME:
		return tr("Posted Links");
	case TEXT_NEW:
		return tr("Create Topic");
	case TEXT_TODO:
		return "<b>Open points:</b><ul>"
		       "<li>Subreddits/tag to posts support"
		       "<li>Picture Support"
		       "<li>Navigate channel link"
		       "</ul>";

	case TEXT_YOUR_GROUP:
		return tr("My Topics");
	case TEXT_SUBSCRIBED_GROUP:
		return tr("Subscribed Topics");
	case TEXT_POPULAR_GROUP:
		return tr("Popular Topics");
	case TEXT_OTHER_GROUP:
		return tr("Other Topics");
	}

	return "";
}

QString PostedDialog::icon(IconType type)
{
	switch (type) {
	case ICON_NAME:
		return ":/icons/png/posted.png";
	case ICON_NEW:
		return ":/icons/png/add.png";
	case ICON_YOUR_GROUP:
		return ":/images/folder16.png";
	case ICON_SUBSCRIBED_GROUP:
		return ":/images/folder_red.png";
	case ICON_POPULAR_GROUP:
		return ":/images/folder_green.png";
	case ICON_OTHER_GROUP:
		return ":/images/folder_yellow.png";
	case ICON_DEFAULT:
		return "";
	}

	return "";
}

GxsGroupDialog *PostedDialog::createNewGroupDialog(TokenQueue *tokenQueue)
{
	return new PostedGroupDialog(tokenQueue, this);
}

GxsGroupDialog *PostedDialog::createGroupDialog(TokenQueue *tokenQueue, RsTokenService *tokenService, GxsGroupDialog::Mode mode, RsGxsGroupId groupId)
{
	return new PostedGroupDialog(tokenQueue, tokenService, mode, groupId, this);
}

int PostedDialog::shareKeyType()
{
    return POSTED_KEY_SHARE;
}

GxsMessageFrameWidget *PostedDialog::createMessageFrameWidget(const RsGxsGroupId &groupId)
{
	return new PostedListWidget(groupId);
}

RsGxsCommentService *PostedDialog::getCommentService()
{
	return rsPosted;
}

QWidget *PostedDialog::createCommentHeaderWidget(const RsGxsGroupId &grpId, const RsGxsMessageId &msgId)
{
	return new PostedItem(NULL, 0, grpId, msgId, true, false);
}

void PostedDialog::loadGroupSummaryToken(const uint32_t &token, std::list<RsGroupMetaData> &groupInfo, RsUserdata *&userdata)
{
	std::vector<RsPostedGroup> groups;
	rsPosted->getGroupData(token, groups);

	/* Save groups to fill description */
	PostedGroupInfoData *postedData = new PostedGroupInfoData;
	userdata = postedData;

	std::vector<RsPostedGroup>::iterator groupIt;
	for (groupIt = groups.begin(); groupIt != groups.end(); ++groupIt) {
		RsPostedGroup &group = *groupIt;
		groupInfo.push_back(group.mMeta);

		if (!group.mDescription.empty()) {
			postedData->mDescription[group.mMeta.mGroupId] = QString::fromUtf8(group.mDescription.c_str());
		}
	}
}

void PostedDialog::groupInfoToGroupItemInfo(const RsGroupMetaData &groupInfo, GroupItemInfo &groupItemInfo, const RsUserdata *userdata)
{
	GxsGroupFrameDialog::groupInfoToGroupItemInfo(groupInfo, groupItemInfo, userdata);

	const PostedGroupInfoData *postedData = dynamic_cast<const PostedGroupInfoData*>(userdata);
	if (!postedData) {
		std::cerr << "PostedDialog::groupInfoToGroupItemInfo() Failed to cast data to PostedGroupInfoData";
		std::cerr << std::endl;
		return;
	}

	QMap<RsGxsGroupId, QString>::const_iterator descriptionIt = postedData->mDescription.find(groupInfo.mGroupId);
	if (descriptionIt != postedData->mDescription.end()) {
		groupItemInfo.description = descriptionIt.value();
	}
}
